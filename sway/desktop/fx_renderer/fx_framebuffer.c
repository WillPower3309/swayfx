#include "log.h"
#include "sway/desktop/fx_renderer/fx_framebuffer.h"
#include "sway/desktop/fx_renderer/fx_stencilbuffer.h"
#include "sway/desktop/fx_renderer/fx_texture.h"

struct fx_framebuffer fx_framebuffer_create() {
	return (struct fx_framebuffer) {
		.fb = -1,
		.stencil_buffer = fx_stencilbuffer_create(),
		.texture = fx_texture_create(),
	};
}

void fx_framebuffer_bind(struct fx_framebuffer *buffer) {
	glBindFramebuffer(GL_FRAMEBUFFER, buffer->fb);
}

void fx_framebuffer_update(struct fx_framebuffer *buffer, int width, int height) {
	bool first_alloc = false;

	if (buffer->fb == (uint32_t) -1) {
		glGenFramebuffers(1, &buffer->fb);
		first_alloc = true;
	}

	if (buffer->texture.id == 0) {
		first_alloc = true;
		glGenTextures(1, &buffer->texture.id);
		glBindTexture(GL_TEXTURE_2D, buffer->texture.id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}

	if (first_alloc || buffer->texture.width != width || buffer->texture.height != height) {
		glBindTexture(GL_TEXTURE_2D, buffer->texture.id);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, buffer->fb);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
				buffer->texture.id, 0);
		buffer->texture.target = GL_TEXTURE_2D;
		buffer->texture.has_alpha = false;
		buffer->texture.width = width;
		buffer->texture.height = height;

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			sway_log(SWAY_ERROR, "Framebuffer incomplete, couldn't create! (FB status: %i)", status);
			return;
		}
		sway_log(SWAY_DEBUG, "Framebuffer created, status %i", status);
	}

	glBindTexture(GL_TEXTURE_2D, 0);
}

void fx_framebuffer_add_stencil_buffer(struct fx_framebuffer *buffer, int width, int height) {
	bool first_alloc = false;

	if (buffer->stencil_buffer.rb == (uint32_t) -1) {
		glGenRenderbuffers(1, &buffer->stencil_buffer.rb);
		first_alloc = true;
	}

	if (first_alloc || buffer->stencil_buffer.width != width || buffer->stencil_buffer.height != height) {
		glBindRenderbuffer(GL_RENDERBUFFER, buffer->stencil_buffer.rb);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, width, height);
		buffer->stencil_buffer.width = width;
		buffer->stencil_buffer.height = height;
	}

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, buffer->stencil_buffer.rb);
}

void fx_framebuffer_release(struct fx_framebuffer *buffer) {
	// Release the framebuffer
	if (buffer->fb != (uint32_t) -1 && buffer->fb) {
		glDeleteFramebuffers(1, &buffer->fb);
	}
	buffer->fb = -1;

	// Release the stencil buffer
	fx_stencilbuffer_release(&buffer->stencil_buffer);

	// Release the texture
	fx_texture_release(&buffer->texture);
}
