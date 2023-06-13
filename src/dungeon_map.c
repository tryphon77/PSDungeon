#include <genesis.h>

#include "vec.h"

#define MAX_SIZE 32
#define MAP_SHIFT 5


typedef struct {
	u16 width;
	u16 height;
	u8 data[MAX_SIZE*MAX_SIZE];
} DungeonMap;


u8 DM_get_at_towards(DungeonMap *map, Vect2D_s16 *pos, Vect2D_s16 *dir) {
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

Vect2D_s16 forward = {0, -1};
Vect2D_s16 backward = {0, 1};
Vect2D_s16 left = {-1, 0};
Vect2D_s16 right = {1, 0};

void DM_turn_left() {
	VEC_rotate_left(&forward);
	VEC_rotate_left(&backward);
	VEC_rotate_left(&right);
	VEC_rotate_left(&left);
}
	
void DM_turn_right() {
	VEC_rotate_right(&forward);
	VEC_rotate_right(&backward);
	VEC_rotate_right(&right);
	VEC_rotate_right(&left);
}	
