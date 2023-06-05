#include <assert.h>
#include <wlr/render/gles2.h>

#include "sway/desktop/fx_renderer/fx_stencilbuffer.h"

struct fx_stencilbuffer fx_stencilbuffer_create() {
	return (struct fx_stencilbuffer) {
		.rb = -1,
		.width = -1,
		.height = -1,
	};
}

void fx_stencilbuffer_release(struct fx_stencilbuffer *stencil_buffer) {
	if (stencil_buffer->rb != (uint32_t) -1 && stencil_buffer->rb) {
		glDeleteRenderbuffers(1, &stencil_buffer->rb);
	}
	stencil_buffer->rb = -1;
	stencil_buffer->width = -1;
	stencil_buffer->height = -1;
}
