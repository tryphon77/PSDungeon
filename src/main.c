#include <genesis.h>
#include <kdebug.h>

#include "dungeon_map.h"
#include "d3d_renderer.h"

#include "scenes.h"


#define DISABLE_DISPLAY TRUE

#define VEC_SET(vec, v_x, v_y) (vec).x = (v_x); (vec).y = (v_y)

// future
void D3D_doVBlankProcess();

u16 joy;
u16 ind;

const u16 palette_data[] = {
	0x000, 0x4E4, 0xAEA, 0x4A4, 0xEA0, 0xE40, 0x440, 0xEE0, 
	0xA00, 0x000, 0xE0E, 0xE0E, 0xE0E, 0xE0E, 0xE0E, 0xE0E
};

/*
// =========================== dungeon handler

#define MAX_SIZE 32
#define MAP_SHIFT 5

typedef struct {
	u16 width;
	u16 height;
	u8 data[MAX_SIZE*MAX_SIZE];
} DungeonMap;

DungeonMap current_map;

u8 get_map(DungeonMap *map, Vect2D_s16 *pos, Vect2D_s16 *dir) {
	s16 x = pos->x + dir->x;
	s16 y = pos->y + dir->y;
	
	if ((0 <= x) && (x < map->width) && (0 <= y) && (y < map->height)) {
		return map->data[(y << MAP_SHIFT) + x];
	}
	
	return 1;
}

void DM_init(DungeonMap *self, u16 width, u16 height, const u8 *source) {
//	kprintf("DM_init: self=%p, width=%d, height=%d, data=%p, source=%p", self, width, height, data, source);
	memsetU32((u32*) self->data, 0, width*height/4);
	
	self->width = width;
	self->height = height;
	
	u8 *line_start = self->data;
	for (u16 i = 0 ; i < height ; i++) {
		u8 *pos = line_start;
		for (u16 j = 0 ; j < width ; j++) {
			*(pos++) = *(source++);
		}
		line_start += MAX_SIZE;
	}
}	

void advance_by(Vect2D_s16 *src, Vect2D_s16 *dir) {
	src->x += dir->x;
	src->y += dir->y;
}

void rotate_left(Vect2D_s16 *src) {
	s16 temp = src->x;
	src->x = src->y;
	src->y = -temp;
}

void rotate_right(Vect2D_s16 *src) {
	s16 temp = src->x;
	src->x = -src->y;
	src->y = temp;
}

Vect2D_s16 forward = {0, -1};
Vect2D_s16 backward = {0, 1};
Vect2D_s16 left = {-1, 0};
Vect2D_s16 right = {1, 0};

void DM_turn_left() {
	rotate_left(&forward);
	rotate_left(&backward);
	rotate_left(&right);
	rotate_left(&left);
}
	
void DM_turn_right() {
	rotate_right(&forward);
	rotate_right(&backward);
	rotate_right(&right);
	rotate_right(&left);
}	

// ========================================================================================






// ========================== 3D dungeon renderer

#define BUFFER_START 0x10

typedef struct {
	u16 tile_id;
	u16 nb_steps;
	DungeonMap *map;
	Vect2D_s16 pos;
	u16 pos_in_buffer;
	u16 pos_in_plane;
	u16 *pos_in_plane_buffer;
	const s16 *columns;
	u16 columns_len;
	const s16 *const *scenes[];
} D3DRenderer;

u16 tilemap_buffer[32*24];
u8 buffer_B_flag = FALSE;
u8 bg_color = 0;

s16 scenes_columns[32*8];
s16 *scenes_columns_index;

u8 scenes_columns_indexes[8];


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


void D3D_init(D3DRenderer *self, DungeonMap *map, s16 x, s16 y, u8 orientation) {
	self->map = map;
	self->pos.x = x;
	self->pos.y = y;
	
	switch (orientation) {
		case 0:
		VEC_SET(forward, 1, 0);
		VEC_SET(backward, -1, 0);
		VEC_SET(right, 0, 1);
		VEC_SET(left, 0, -1);
		break;

		case 1:
		VEC_SET(forward, 0, -1);
		VEC_SET(backward, 0, 1);
		VEC_SET(right, 1, 0);
		VEC_SET(left, -1, 0);
		break;

		case 2:
		VEC_SET(forward, -1, 0);
		VEC_SET(backward, 1, 0);
		VEC_SET(right, 0, -1);
		VEC_SET(left, 0, 1);
		break;

		case 3:
		VEC_SET(forward, 0, 1);
		VEC_SET(backward, 0, -1);
		VEC_SET(right, -1, 0);
		VEC_SET(left, 1, 0);
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

static u16 send_column(s16 column_id, u16 pos) {
//	kprintf("send_column column_id=%d vpos=%X", column_id, pos);
	
	const u32 *column_data = columns_data + 104*column_id;
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


static const s16 *const *get_half_corridor(Vect2D_s16 *start, Vect2D_s16 *dir) {
	Vect2D_s16 pos = {start->x, start->y};

	u8 res = 0;
	u16 n;

	for (n = 0 ; n < 4 ; n++) {
		if (get_map(&current_map, &pos, &forward)) {
			res |= 1;
			advance_by(&pos, &forward);
			break;
		}
		advance_by(&pos, &forward);
	}
	if (n == 4) n--;
	
	for (u16 i = 0 ; i < n + 1 ; i++) {
		advance_by(&pos, &backward);
		res <<= 1;
		if (get_map(&current_map, &pos, dir)) {
			res |= 1;
		}
	}
	
	return scenes[n][res];
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

void D3D_send_half_screen(D3DRenderer *self) {
//	kprintf("send_full_scene");

	for (int i = 0 ; i < 16 ; i++) {
		s16 column_id = *(self->columns++);
		
		if (column_id < 0) {
			send_tilemap_column(
				self->pos_in_plane_buffer, 
				self->pos_in_plane, 
				self->tile_id, 
				0x0800
			);
			send_column(
				-column_id - 1, 
				self->pos_in_buffer
			);
		}
		else {
			send_tilemap_column(
				self->pos_in_plane_buffer, 
				self->pos_in_plane, 
				self->tile_id, 
				0x0000
			);
			send_column(
				column_id - 1, 
				self->pos_in_buffer
			);
		}
		self->pos_in_plane_buffer += 24;
		self->pos_in_plane += 2;
		self->pos_in_buffer += 13;
		self->tile_id += 13;
	}
//	kprintf("/send_full_scene");
}

void D3D_next_frame(D3DRenderer *self) {	
    vu16 *pw = (u16 *) VDP_CTRL_PORT;

	if (buffer_B_flag) {
		self->tile_id = BUFFER_START + 0x1A0;
		self->pos_in_buffer = BUFFER_START + 0x1A0;
		self->pos_in_plane = 0xC000;
		self->pos_in_plane_buffer = tilemap_buffer;
		
		D3D_send_half_screen(self);
		D3D_doVBlankProcess();
		D3D_send_half_screen(self);
		D3D_doVBlankProcess();
		*pw = 0x8200 | (0xC000 / 0x400);
		*pw = 0x8400 | (0xC000 / 0x2000);	
	}
	else {
		self->tile_id = BUFFER_START;
		self->pos_in_buffer = BUFFER_START;
		self->pos_in_plane = 0xE000;
		self->pos_in_plane_buffer = tilemap_buffer;
		
		D3D_send_half_screen(self);
		D3D_doVBlankProcess();
		D3D_send_half_screen(self);
		D3D_doVBlankProcess();
		*pw = 0x8200 | (0xE000 / 0x400);
		*pw = 0x8400 | (0xE000 / 0x2000);	
	}
	buffer_B_flag = !buffer_B_flag;
}
	
void D3D_build_front_scene(D3DRenderer *self, u8 reversed) {
	const s16 *const *left_part = get_half_corridor(&(self->pos), &left);
	const s16 *const *right_part = get_half_corridor(&(self->pos), &right);

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

	self->columns = scenes_columns;
}


void D3D_doVBlankProcess() {
	SYS_doVBlankProcessEx(ON_VBLANK);
	if (DISABLE_DISPLAY) {
		VDP_setEnable(TRUE);
		VDP_setHIntCounter(192);
	}
}


// ======================================================================================

void D3D_move_forward(D3DRenderer *self) {
//	kprintf("init_forward");
	for (u16 i = 0 ; i < 5 ; i++) {
		D3D_next_frame(self);
	}
	
	advance_by(&(self->pos), &forward);
	D3D_build_front_scene(self, FALSE);
	D3D_next_frame(self);
}

void D3D_move_backward(D3DRenderer *self) {
//	kprintf("init_backward");
	
	advance_by(&(self->pos), &backward);
	D3D_build_front_scene(self, TRUE);

	for (u16 i = 0 ; i < 6 ; i++) {
		D3D_next_frame(self);
	}
	
	D3D_build_front_scene(self, FALSE);
}

void D3D_turn_left(D3DRenderer *self) {
	u8 scene_id;

	clear_scenes_columns();

	if (get_map(&current_map, &(self->pos), &forward)) {
		if (get_map(&current_map, &(self->pos), &left)) {
			scene_id = 3;
		}
		else {
			scene_id = 1;
		}
	}
	else if (get_map(&current_map, &(self->pos), &left)) {
		scene_id = 2;
	}
	else {
		scene_id = 0;
	}

	append_columns_to_scenes(
		self,
		turns[scene_id],
		TRUE,
		FALSE
	);

	self->columns = scenes_columns;

	for (u8 k = 0 ; k < self->nb_steps ; k++) {
		D3D_next_frame(self);
	}

	DM_turn_left();

	D3D_build_front_scene(self, FALSE);
	D3D_next_frame(self);
}

void D3D_turn_right(D3DRenderer *self) {
	u8 scene_id;
	
	clear_scenes_columns();

	if (get_map(&current_map, &(self->pos), &forward)) {
		if (get_map(&current_map, &(self->pos), &right)) {
			scene_id = 3;
		}
		else {
			scene_id = 1;
		}
	}
	else if (get_map(&current_map, &(self->pos), &right)) {
		scene_id = 2;
	}
	else {
		scene_id = 0;
	}
	
	append_columns_to_scenes(
		self,
		turns[scene_id],
		FALSE,
		FALSE
	);

	self->columns = scenes_columns;

	for (u8 k = 0 ; k < self->nb_steps ; k++) {
		D3D_next_frame(self);
	}
	
	DM_turn_right();

	D3D_build_front_scene(self, FALSE);
	D3D_next_frame(self);	
}











// ==================================================================================================
*/


const u8 test_dungeon[] = {
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 0, 0, 0, 0, 0, 0, 1,
	1, 1, 0, 1, 0, 1, 0, 1,
	1, 1, 0, 0, 0, 0, 0, 1, 
	1, 1, 1, 1, 1, 1, 0, 1,
	1, 1, 1, 1, 1, 1, 0, 1,
	1, 1, 1, 1, 1, 1, 0, 1,
	1, 1, 1, 1, 1, 1, 1, 1
};

DungeonMap current_map;
D3DRenderer renderer;

int main(bool hard) {

	ind = TILE_USER_INDEX;	
		
    // initialization
    VDP_setScreenWidth320();
	VDP_setScreenHeight224();

    PAL_setColors(0, palette_data, 16, CPU);

    // just to monitor frame CPU usage
    SYS_showFrameLoad(TRUE);

	// initialize
	DM_init(&current_map, 8, 8, test_dungeon);
	D3D_init(&renderer, &current_map, &scenes_data, 6, 6, 1);

	if (TRUE) {

		D3D_build_front_scene(&renderer, FALSE);
		D3D_next_frame(&renderer);
		
		while (TRUE) {
			joy = JOY_readJoypad(JOY_1);

			if (joy & BUTTON_UP) {
				if (!DM_get_at_towards(&current_map, &(renderer.pos), &forward)) {
					D3D_move_forward(&renderer);
					continue;
				}
			}

			else if (joy & BUTTON_DOWN) {
				if (!DM_get_at_towards(&current_map, &(renderer.pos), &backward)) {
					D3D_move_backward(&renderer);
					continue;
				}
			}

			else if (joy & BUTTON_LEFT) {
				D3D_turn_left(&renderer);
				continue;
			}

			else if (joy & BUTTON_RIGHT) {
				D3D_turn_right(&renderer);
				continue;
			}

			D3D_doVBlankProcess();
		}
	}
}
