#include "log.h"
#include "sway/desktop/fx_renderer/fx_framebuffer.h"

struct fx_framebuffer fx_framebuffer_create() {
	return (struct fx_framebuffer) {
		.fb = -1,
		.stencil_buffer = -1,
		.texture.id = 0,
		.texture.target = 0,
		.texture.width = -1,
		.texture.height = -1,
	};
}

void fx_framebuffer_bind(struct fx_framebuffer *buffer) {
	glBindFramebuffer(GL_FRAMEBUFFER, buffer->fb);
}

void fx_framebuffer_update(struct fx_framebuffer *buffer, int width, int height) {
	bool firstAlloc = false;

	// Create a new framebuffer
	if (buffer->fb == (uint32_t) -1) {
		glGenFramebuffers(1, &buffer->fb);
		firstAlloc = true;
	}

	if (buffer->texture.id == 0) {
		firstAlloc = true;
		glGenTextures(1, &buffer->texture.id);
		glBindTexture(GL_TEXTURE_2D, buffer->texture.id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}

	if (firstAlloc || buffer->texture.width != width || buffer->texture.height != height) {
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

	// Bind the default framebuffer
	glBindTexture(GL_TEXTURE_2D, 0);
}

void fx_framebuffer_add_stencil_buffer(struct fx_framebuffer *buffer, int width, int height) {
	if (buffer->stencil_buffer == (uint32_t) -1) {
		glGenRenderbuffers(1, &buffer->stencil_buffer);
		glBindRenderbuffer(GL_RENDERBUFFER, buffer->stencil_buffer);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, width, height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, buffer->stencil_buffer);
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			sway_log(SWAY_ERROR, "Stencil buffer incomplete, couldn't create! (FB status: %i)", status);
			return;
		}
		sway_log(SWAY_DEBUG, "Stencil buffer created, status %i", status);
	}
}

void fx_framebuffer_release(struct fx_framebuffer *buffer) {
	// Release the framebuffer
	if (buffer->fb != (uint32_t) -1 && buffer->fb) {
		glDeleteFramebuffers(1, &buffer->fb);
	}
	buffer->fb= -1;

	// Release the stencil buffer
	if (buffer->stencil_buffer != (uint32_t)-1 && buffer->stencil_buffer) {
		glDeleteRenderbuffers(1, &buffer->stencil_buffer);
	}
	buffer->stencil_buffer = -1;

	// Release the texture
	if (buffer->texture.id) {
		glDeleteTextures(1, &buffer->texture.id);
	}
	buffer->texture.id = 0;
	buffer->texture.width = -1;
	buffer->texture.height = -1;
}
