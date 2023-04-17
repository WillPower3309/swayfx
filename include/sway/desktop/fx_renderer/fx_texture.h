#ifndef FX_TEXTURE_H
#define FX_TEXTURE_H

#include <GLES2/gl2.h>
#include <stdbool.h>
#include <wlr/render/wlr_texture.h>

struct fx_texture {
	GLuint target;
	GLuint id;
	bool has_alpha;
	int width;
	int height;
};

struct fx_texture fx_texture_from_wlr_texture(struct wlr_texture* tex);

#endif
