#include <genesis.h>
#include "vec.h"


void VEC_advance_by(Vect2D_s16 *src, Vect2D_s16 *dir) {
	src->x += dir->x;
	src->y += dir->y;
}

void VEC_rotate_left(Vect2D_s16 *src) {
	s16 temp = src->x;
	src->x = src->y;
	src->y = -temp;
}

void VEC_rotate_right(Vect2D_s16 *src) {
	s16 temp = src->x;
	src->x = -src->y;
	src->y = temp;
}
