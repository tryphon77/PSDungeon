#ifndef D3D_RENDERER_H_
#define D3D_RENDERER_H_

#include <genesis.h> 
#include "dungeon_map.h"

#define DISABLE_DISPLAY TRUE

#define BUFFER_START 0x10

/* 	There are several data arrays or buffers in the code. 
	To stay consistent, I use the terminology below :
		* a _buffer is in RAM
		* a _vbuffer is in VRAM
		* a _data is in ROM
	The data in these arrays can represent different type of data :
		* a tile_ is a 32 bytes 8x8 pixels tile
		* a attributes_ is a 1 word data containing tile attributes
		* a column_ is a 1 signed word referrencing a specific column
*/

/* @TODO: the "quoted" names are to be renamed in a less confusing way */

/*	ScenesData
	struct containing all gfx data to render a dungeon
*/	
typedef struct {
	/*	tiles data of the entire tileset.
		Stored in column
		Grouped in chunks of 13 tiles, describing the 13 bottom part of a column, since the
		remaining 11 tiles are vflipped versions of these tiles */
	const u32 *columns_data;
	
	/*	array of "front scenes"
		a "front scene" is itself an array of 4 pointers to scenes_group :
			the 1st contains pointers to "scene_steps" of length 1 (just in front of a wall)
			the 2nd contains pointers to "scene_steps" of length 2 (a lateral wall/path then a wall)
			etc.
		a scene_steps array is made of pointers to "actual scenes" organised in a binary way :
			if an edge of a corridor is of the form |X.XX| where X is a wall and . is a path,
			then the corrsponding scene will be at position 0b1011 = 11 in the 4th scene_steps array.
			Some combination are impossible, because there can't be 2 consecutive path (e.g. 0b1001),
			the corresponding pointer will be NULL
		an "actual scene" is an array of pointers towards N "scene_composition", one for each step of
		the forward animation, terminated by a NULL
		A "scene_composition" is an array of int, the 1st is the number of subsequent elements in the array, the others are column_id, or -column_id if the column must be hflipped
		A column_id is an offset in the columns_data field described above
	*/
	const s16 *const *const *const *scenes;
			
	/* 	array of 4 pointers to "actual scenes" : 
			the 1st is "no wall forward, no wall in the direction of turn
			the 2nd is "no wall forward, a wall in the direction of turn
			the 3rd is "a wall forward, no wall in the direction of turn
			the 4th is "a wall forward, a wall in the direction of turn
		Each "actual scene" is described in the section above
	*/	
	const s16 *const *const *turns;
} ScenesData;

/*	D3DRenderer: tote-bag containing data needed to render the Dungeon */
typedef struct {
	/* tile_id of the first tile of the next column to draw */
	u16 tile_id;

	/* number of steps of the current animation */
	u16 nb_steps;
	
	/* current map attached to the renderer */
	const DungeonMap *map;
	
	/* SceneData object to render the dungeon */
	const ScenesData *scenes_data;

	/* actual position of the player in the map */
	Vect2D_s16 pos;

	/* actual VRAM address where to put the next tile data ("pos_in_tile_vbuffer") */
	u16 pos_in_tile_vbuffer;
	
	/* vpos in plane_A or plane_B to write the tileset data of the next column ("pos_in_attributes_vbuffer") */
	u16 pos_in_attributes_vbuffer;
	
	/* position in RAM where to write tilemap data of the next column ("pos_in_attributes_buffer") */
	u16 *pos_in_attributes_buffer;

	/* position in RAM where to read the next column_id of the current scene ("pos_in_column_buffer") */
	const s16 *pos_in_column_buffer;
	
	/* position in ROM of the tileset */
	const u32 *columns_data;
} D3DRenderer;


/* 	void D3D_init(D3DRenderer *self, const DungeonMap *map, const ScenesData *scenes_data, s16 x, s16 y, u8 orientation) 
		intialize an already declared D3DRenderer.
		map: the DungeonMap object describing the map
		scenes_data: the textures for rendering the map
		x, y: initial position in map (top, left = 0, 0)
		orientation: 0=left, 1=up, 2=right, 3=bottom 
*/

void D3D_init(D3DRenderer *self, const DungeonMap *map, const ScenesData *scenes, s16 x, s16 y, u8 orientation);


/* 	void D3D_send_half_screen(D3DRenderer *self)
	prepare data for updating half a screen
	self: the D3DRenderer object
	
	internally, the columns_id must have been prepared by ??? and self->pos_in_column_buffer points to 
	the next column to render.	
*/

void D3D_send_half_screen(D3DRenderer *self);


/* 	void D3D_next_frame(D3DRenderer *self) 
	render next frame
	calls 2 times D3D_send_half_screen, then switches frame_buffers 
*/

void D3D_next_frame(D3DRenderer *self);


/*	void D3D_build_front_scene(D3DRenderer *self, u8 reversed)
	builds the colonnes_buffer from the left/right scenes determinded by get_half_corridor
*/

void D3D_build_front_scene(D3DRenderer *self, u8 reversed);


void D3D_doVBlankProcess();

// ======================================================================================

void D3D_move_forward(D3DRenderer *self);
void D3D_move_backward(D3DRenderer *self);
void D3D_turn_left(D3DRenderer *self);
void D3D_turn_right(D3DRenderer *self);

#endif
