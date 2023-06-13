#ifndef DUNGEON_MAP_H_
#define DUNGEON_MAP_H_

#include <genesis.h>



#define MAX_SIZE 32
#define MAP_SHIFT 5

typedef struct {
	u16 width;
	u16 height;
	u8 data[MAX_SIZE*MAX_SIZE];
} DungeonMap;

u8 DM_get_at_towards(const DungeonMap *map, Vect2D_s16 *pos, Vect2D_s16 *dir);
void DM_init(const DungeonMap *self, u16 width, u16 height, const u8 *source);

Vect2D_s16 forward; 
Vect2D_s16 backward; 
Vect2D_s16 left; 
Vect2D_s16 right; 

void DM_turn_left();
void DM_turn_right();

#endif
