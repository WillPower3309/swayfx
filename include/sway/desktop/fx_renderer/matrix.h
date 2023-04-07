#ifndef _MATRIX_H
#define _MATRIX_H

#include <wlr/types/wlr_output.h>

void matrix_projection(float mat[static 9], int width, int height,
		enum wl_output_transform transform);

#endif
