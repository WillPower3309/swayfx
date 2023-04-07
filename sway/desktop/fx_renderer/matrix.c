#include <math.h>
#include <string.h>
#include <wlr/types/wlr_output.h>

#include "sway/desktop/fx_renderer/matrix.h"

static const float transforms[][9] = {
	[WL_OUTPUT_TRANSFORM_NORMAL] = {
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_90] = {
		0.0f, 1.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_180] = {
		-1.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_270] = {
		0.0f, -1.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_FLIPPED] = {
		-1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_FLIPPED_90] = {
		0.0f, 1.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_FLIPPED_180] = {
		1.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_FLIPPED_270] = {
		0.0f, -1.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
};

void matrix_projection(float mat[static 9], int width, int height,
		enum wl_output_transform transform) {
	memset(mat, 0, sizeof(*mat) * 9);

	const float *t = transforms[transform];
	float x = 2.0f / width;
	float y = 2.0f / height;

	// Rotation + reflection
	mat[0] = x * t[0];
	mat[1] = x * t[1];
	mat[3] = y * -t[3];
	mat[4] = y * -t[4];

	// Translation
	mat[2] = -copysign(1.0f, mat[0] + mat[1]);
	mat[5] = -copysign(1.0f, mat[3] + mat[4]);

	// Identity
	mat[8] = 1.0f;
}
