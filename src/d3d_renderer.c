#include <genesis.h> 

#include "vec.h"
#include "dungeon_map.h"
#include "d3d_renderer.h"

/* frame_buffer_A is located at VRAM pos 0xE000
   frame_buffer_B is located at VRA% pos 0xC000 */ 

/* pos in VRAM of the start of tileset buffers
   2 buffers are needed for buffer_A, 2 for buffer_B
   Each buffer contients 16*13 = 0xD0 tiles = 0x1A0 bytes */  
#define BUFFER_START 0x10

/* tilemap buffer for half screen : 32 cols of 24 tiles = 1536 bytes */
u16 tilemap_buffer[32*24];

/* TRUE iff displayed buffer is frame_buffer_A, while working buffer is frame_buffer_B */
u8 buffer_B_flag = FALSE;


// u8 bg_color = 0;

/* columns buffer : contains the whole 32 colonnes of all scenes (max. 8) composing current animation */
s16 scenes_columns[32*8];

/* scenes_columns_indexes[k] is the number of columns set in the k-th frame of the animation */
u8 scenes_columns_indexes[8];
s16 *scenes_columns_index;


HINTERRUPT_CALLBACK hint_callback() {
	// disable display
	VDP_setEnable(FALSE);
	// disable HInt
//	VDP_setHInterrupt(FALSE);
}

static void clear_scenes_columns() {
	for (u8 i = 0 ; i < 8 ; i++) {
		scenes_columns_indexes[i] = 0;
	}
}


/* 	void D3D_init(D3DRenderer *self, const DungeonMap *map, const ScenesData *scenes_data, s16 x, s16 y, u8 orientation) 
		intialize an already declared D3DRenderer.
		map: the DungeonMap object describing the map
		scenes_data: the textures for rendering the map
		x, y: initial position in map (top, left = 0, 0)
		orientation: 0=left, 1=up, 2=right, 3=bottom */
void D3D_init(D3DRenderer *self, const DungeonMap *map, const ScenesData *scenes_data, s16 x, s16 y, u8 orientation) {
	self->map = map;
	self->pos.x = x;
	self->pos.y = y;
	self->columns_data = scenes_data->columns_data;
	self->scenes_data = scenes_data;
	
	switch (orientation) {
		case 0:
		VEC_set(forward, 1, 0);
		VEC_set(backward, -1, 0);
		VEC_set(right, 0, 1);
		VEC_set(left, 0, -1);
		break;

		case 1:
		VEC_set(forward, 0, -1);
		VEC_set(backward, 0, 1);
		VEC_set(right, 1, 0);
		VEC_set(left, -1, 0);
		break;

		case 2:
		VEC_set(forward, -1, 0);
		VEC_set(backward, 1, 0);
		VEC_set(right, 0, -1);
		VEC_set(left, 0, 1);
		break;

		case 3:
		VEC_set(forward, 0, 1);
		VEC_set(backward, 0, -1);
		VEC_set(right, -1, 0);
		VEC_set(left, 1, 0);
		break;
		
		default:
		kprintf("Bad orientation: %d", orientation);
	}
	
	clear_scenes_columns();

	if (DISABLE_DISPLAY) {
		SYS_setHIntCallback(hint_callback);
		VDP_setHInterrupt(TRUE);
		VDP_setHIntCounter(192);
	}
}

static u16 send_column(D3DRenderer *self, s16 column_id, u16 pos) {
//	kprintf("send_column column_id=%d vpos=%X", column_id, pos);
	
	const u32 *column_data = self->columns_data + 104*column_id;
	VDP_loadTileData(column_data, pos, 13, DMA_QUEUE);
	pos += 13;
	
	return pos;
}

static void send_tilemap_column(u16 *source, u16 dest, u16 tile_id, u16 flag) {
//	kprintf("send_tilemap_column: source=%p dest=%04X, tile_id=%04X, flag=%04X", source, dest, tile_id, flag);
	
	u8 succeed = DMA_transfer(DMA_QUEUE, DMA_VRAM, source, dest, 24, 128);
	if (!succeed) kprintf("DMA_queueDma failed!");

	tile_id += 11;

	for (u16 i = 0 ; i < 11 ; i++) {
		tile_id -= 1;
		*source = 0x1000 | flag | tile_id;
		source += 1;
	}

	for (u16 i = 0 ; i < 13 ; i++) {
		*source = flag | tile_id;
		tile_id += 1;
		source += 1;
	}
}

/*	determines data representing one of the two edges of the corridor in front of the viewer
	start:	position of the player
	dir:	left/right
*/
static const s16 *const *get_half_corridor(D3DRenderer *self, Vect2D_s16 *start, Vect2D_s16 *dir) {
	Vect2D_s16 pos = {start->x, start->y};

	u8 res = 0;
	u16 n;

	for (n = 0 ; n < 4 ; n++) {
		if (DM_get_at_towards(self->map, &pos, &forward)) {
			res |= 1;
			VEC_advance_by(&pos, &forward);
			break;
		}
		VEC_advance_by(&pos, &forward);
	}
	if (n == 4) n--;
	
	for (u16 i = 0 ; i < n + 1 ; i++) {
		VEC_advance_by(&pos, &backward);
		res <<= 1;
		if (DM_get_at_towards(self->map, &pos, dir)) {
			res |= 1;
		}
	}
	
	return self->scenes_data->scenes[n][res];
}



static void append_columns_to_scenes(D3DRenderer *self, const s16 *const *const scenes, u8 hflip, u8 reversed) {	
//	kprintf("append_columns_to_scenes: scenes=%p, hflip=%d, reversed=%d", scenes, hflip, reversed);
	
	s16 *dest_left = scenes_columns;
	s16 *dest;
	u16 nb_columns;
	
	const s16 *const *pos = scenes;
	self->nb_steps = 0;
	while (*(pos++)) {
		self->nb_steps += 1;		
	}
	
	u8 step = reversed? self->nb_steps-1 : 0;
	
	for (u8 k = 0 ; k < self->nb_steps ; k++) {
		dest = dest_left + scenes_columns_indexes[k];
		
		const s16 *source = scenes[step];
		if (reversed) step--; else step++;

		nb_columns = *(source++);

		if (hflip) {
			dest += nb_columns;
			for (u8 j = 0 ; j < nb_columns ; j++) {
				*(--dest) = -(*(source++));
			}
		}

		else {
			for (u8 j = 0 ; j < nb_columns ; j++) {
				*(dest++) = *(source++);
			}
		}
				
		dest_left += 32;
		scenes_columns_indexes[k] += nb_columns;
	}

	scenes_columns_index += nb_columns;
}


/* 	void D3D_send_half_screen(D3DRenderer *self)
	prepare data for updating half a screen
	self: the D3DRenderer object
	
	internally, the columns_id must have been prepared by ??? and self->pos_in_column_buffer points to 
	the next column to render.	
*/

void D3D_send_half_screen(D3DRenderer *self) {
//	kprintf("send_full_scene");

	for (int i = 0 ; i < 16 ; i++) {
		s16 column_id = *(self->pos_in_column_buffer++);
		
		if (column_id < 0) {
			send_tilemap_column(
				self->pos_in_attributes_buffer, 
				self->pos_in_attributes_vbuffer, 
				self->tile_id, 
				0x0800
			);
			send_column(
				self,
				-column_id - 1, 
				self->pos_in_tile_vbuffer
			);
		}
		else {
			send_tilemap_column(
				self->pos_in_attributes_buffer, 
				self->pos_in_attributes_vbuffer, 
				self->tile_id, 
				0x0000
			);
			send_column(
				self,
				column_id - 1, 
				self->pos_in_tile_vbuffer
			);
		}
		self->pos_in_attributes_buffer += 24;
		self->pos_in_attributes_vbuffer += 2;
		self->pos_in_tile_vbuffer += 13;
		self->tile_id += 13;
	}
//	kprintf("/send_full_scene");
}


/* 	void D3D_next_frame(D3DRenderer *self) 
	render next frame
	calls 2 times D3D_send_half_screen, then switches frame_buffers 
*/

void D3D_next_frame(D3DRenderer *self) {	
    vu16 *pw = (u16 *) VDP_CTRL_PORT;

	if (buffer_B_flag) {
		self->tile_id = BUFFER_START + 0x1A0;
		self->pos_in_tile_vbuffer = BUFFER_START + 0x1A0;
		self->pos_in_attributes_vbuffer = 0xC000;
		self->pos_in_attributes_buffer = tilemap_buffer;
		
		D3D_send_half_screen(self);
		D3D_doVBlankProcess();
		D3D_send_half_screen(self);
		D3D_doVBlankProcess();
		*pw = 0x8200 | (0xC000 / 0x400);
		*pw = 0x8400 | (0xC000 / 0x2000);	
	}
	else {
		self->tile_id = BUFFER_START;
		self->pos_in_tile_vbuffer = BUFFER_START;
		self->pos_in_attributes_vbuffer = 0xE000;
		self->pos_in_attributes_buffer = tilemap_buffer;
		
		D3D_send_half_screen(self);
		D3D_doVBlankProcess();
		D3D_send_half_screen(self);
		D3D_doVBlankProcess();
		*pw = 0x8200 | (0xE000 / 0x400);
		*pw = 0x8400 | (0xE000 / 0x2000);	
	}
	buffer_B_flag = !buffer_B_flag;
}
	

/*	void D3D_build_front_scene(D3DRenderer *self, u8 reversed)
	builds the colonnes_buffer from the left/right scenes determinded by get_half_corridor
*/

void D3D_build_front_scene(D3DRenderer *self, u8 reversed) {
	const s16 *const *left_part = get_half_corridor(self, &(self->pos), &left);
	const s16 *const *right_part = get_half_corridor(self, &(self->pos), &right);

	clear_scenes_columns();

	append_columns_to_scenes(
		self,
		left_part,
		FALSE,
		reversed
	);

	append_columns_to_scenes(
		self,
		right_part,
		TRUE,
		reversed
	);

	self->pos_in_column_buffer = scenes_columns;
}


/* */
void D3D_doVBlankProcess() {
	SYS_doVBlankProcessEx(ON_VBLANK);
	if (DISABLE_DISPLAY) {
		VDP_setEnable(TRUE);
		VDP_setHIntCounter(192);
	}
}


// ======================================================================================

/*	void D3D_move_forward(D3DRenderer *self)
	prepares and plays the animation of a step forward
	Normally, when the player is waiting, the forward animation is already loaded in
	the columns buffer.
	The new front scene is loaded in last frame
*/
void D3D_move_forward(D3DRenderer *self) {
//	kprintf("init_forward");
	for (u16 i = 0 ; i < 5 ; i++) {
		D3D_next_frame(self);
	}
	
	VEC_advance_by(&(self->pos), &forward);
	D3D_build_front_scene(self, FALSE);
	D3D_next_frame(self);
}

/*	void D3D_move_backward(D3DRenderer *self)
	prepares and plays the animation of a step backward
	The new scene must be loaded as first frame
*/
void D3D_move_backward(D3DRenderer *self) {
//	kprintf("init_backward");
	
	VEC_advance_by(&(self->pos), &backward);
	D3D_build_front_scene(self, TRUE);

	for (u16 i = 0 ; i < 6 ; i++) {
		D3D_next_frame(self);
	}
	
	D3D_build_front_scene(self, FALSE);
}

/*	void D3D_turn_left(D3DRenderer *self)
	prepares and plays animation of a left turn
*/
void D3D_turn_left(D3DRenderer *self) {
	u8 scene_id;

	clear_scenes_columns();

	if (DM_get_at_towards(self->map, &(self->pos), &forward)) {
		if (DM_get_at_towards(self->map, &(self->pos), &left)) {
			scene_id = 3;
		}
		else {
			scene_id = 1;
		}
	}
	else if (DM_get_at_towards(self->map, &(self->pos), &left)) {
		scene_id = 2;
	}
	else {
		scene_id = 0;
	}

	append_columns_to_scenes(
		self,
		self->scenes_data->turns[scene_id],
		TRUE,
		FALSE
	);

	self->pos_in_column_buffer = scenes_columns;

	for (u8 k = 0 ; k < self->nb_steps ; k++) {
		D3D_next_frame(self);
	}

	DM_turn_left();

	D3D_build_front_scene(self, FALSE);
	D3D_next_frame(self);
}

/*	void D3D_turn_right(D3DRenderer *self)
	prepares and plays animation of a right turn
*/
void D3D_turn_right(D3DRenderer *self) {
	u8 scene_id;
	
	clear_scenes_columns();

	if (DM_get_at_towards(self->map, &(self->pos), &forward)) {
		if (DM_get_at_towards(self->map, &(self->pos), &right)) {
			scene_id = 3;
		}
		else {
			scene_id = 1;
		}
	}
	else if (DM_get_at_towards(self->map, &(self->pos), &right)) {
		scene_id = 2;
	}
	else {
		scene_id = 0;
	}
	
	append_columns_to_scenes(
		self,
		self->scenes_data->turns[scene_id],
		FALSE,
		FALSE
	);

	self->pos_in_column_buffer = scenes_columns;

	for (u8 k = 0 ; k < self->nb_steps ; k++) {
		D3D_next_frame(self);
	}
	
	DM_turn_right();

	D3D_build_front_scene(self, FALSE);
	D3D_next_frame(self);	
}
