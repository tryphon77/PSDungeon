#ifndef VEC_H_
#define VEC_H_


#define VEC_set(vec, v_x, v_y) (vec).x = (v_x); (vec).y = (v_y)

void VEC_advance_by(Vect2D_s16 *src, Vect2D_s16 *dir);
void VEC_rotate_left(Vect2D_s16 *src);
void VEC_rotate_right(Vect2D_s16 *src);

#endif
