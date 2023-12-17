#ifndef FX_STENCILBUFFER_H
#define FX_STENCILBUFFER_H

#include <GLES2/gl2.h>
#include <stdbool.h>
#include <wlr/render/wlr_texture.h>

struct fx_stencilbuffer {
	GLuint rb;
	int width;
	int height;
};

struct fx_stencilbuffer fx_stencilbuffer_create(void);

void fx_stencilbuffer_init(struct fx_stencilbuffer *stencil_buffer, int width, int height);

void fx_stencilbuffer_release(struct fx_stencilbuffer *stencil_buffer);

#endif
