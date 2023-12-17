#include <stdio.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/render/interface.h>
#include <wlr/render/allocator.h>

#include "log.h"
#include "render/egl.h"
#include "render/gles2.h"
#include "sway/desktop/fx_renderer/fx_renderer.h"

static void handle_buffer_destroy(struct wlr_addon *addon) {
	struct fx_framebuffer *buffer =
		wl_container_of(addon, buffer, addon);
	fx_framebuffer_release(buffer);
}

static const struct wlr_addon_interface buffer_addon_impl = {
	.name = "fx_framebuffer",
	.destroy = handle_buffer_destroy,
};


struct fx_framebuffer fx_framebuffer_create(void) {
	return (struct fx_framebuffer) {
		.initialized = false,
		.fbo = -1,
		.rbo = -1,
		.wlr_buffer = NULL,
		.image = NULL,
	};
}

void fx_framebuffer_bind(struct fx_framebuffer *fx_buffer) {
	glBindFramebuffer(GL_FRAMEBUFFER, fx_buffer->fbo);
}

void fx_framebuffer_bind_wlr_fbo(struct fx_renderer *renderer) {
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->wlr_main_buffer_fbo);
}

void fx_framebuffer_update(struct fx_renderer *fx_renderer, struct fx_framebuffer *fx_buffer,
		int width, int height) {
	struct wlr_output *output = fx_renderer->wlr_output;

	fx_buffer->renderer = fx_renderer;

	bool first_alloc = false;

	if (!fx_buffer->wlr_buffer ||
			fx_buffer->wlr_buffer->width != width ||
			fx_buffer->wlr_buffer->height != height) {
		wlr_buffer_drop(fx_buffer->wlr_buffer);
		fx_buffer->wlr_buffer = wlr_allocator_create_buffer(output->allocator,
				width, height, fx_renderer->drm_format);
		first_alloc = true;
	}

	if (fx_buffer->fbo == (uint32_t) -1 || first_alloc) {
		glGenFramebuffers(1, &fx_buffer->fbo);
		first_alloc = true;
	}

	if (fx_buffer->rbo == (uint32_t) -1 || first_alloc) {
		struct wlr_dmabuf_attributes dmabuf = {0};
		if (!wlr_buffer_get_dmabuf(fx_buffer->wlr_buffer, &dmabuf)) {
			goto error_buffer;
		}

		bool external_only;
		fx_buffer->image = wlr_egl_create_image_from_dmabuf(fx_renderer->egl,
			&dmabuf, &external_only);
		if (fx_buffer->image == EGL_NO_IMAGE_KHR) {
			goto error_buffer;
		}

		glGenRenderbuffers(1, &fx_buffer->rbo);
		glBindRenderbuffer(GL_RENDERBUFFER, fx_buffer->rbo);
		fx_renderer->procs.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER,
			fx_buffer->image);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, fx_buffer->fbo);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_RENDERBUFFER, fx_buffer->rbo);
		GLenum fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		if (fb_status != GL_FRAMEBUFFER_COMPLETE) {
			wlr_log(WLR_ERROR, "Failed to create FBO");
			goto error_image;
		}
	}

	if (!fx_buffer->initialized) {
		fx_buffer->initialized = true;

		wlr_addon_init(&fx_buffer->addon, &fx_buffer->wlr_buffer->addons, fx_renderer,
			&buffer_addon_impl);

		wl_list_insert(&fx_renderer->buffers, &fx_buffer->link);
	}

	if (first_alloc) {
		wlr_log(WLR_DEBUG, "Created GL FBO for buffer %dx%d",
			fx_buffer->wlr_buffer->width, fx_buffer->wlr_buffer->height);
	}

	return;
error_image:
	wlr_egl_destroy_image(fx_renderer->egl, fx_buffer->image);
error_buffer:
	wlr_log(WLR_ERROR, "Could not create FX buffer! Aborting...");
	abort();
}

void fx_framebuffer_release(struct fx_framebuffer *fx_buffer) {
	// Release the framebuffer
	struct wlr_egl_context prev_ctx;
	if (fx_buffer->initialized) {
		wl_list_remove(&fx_buffer->link);
		wlr_addon_finish(&fx_buffer->addon);

		wlr_egl_save_context(&prev_ctx);
		wlr_egl_make_current(fx_buffer->renderer->egl);
	}

	glDeleteFramebuffers(1, &fx_buffer->fbo);
	fx_buffer->fbo = -1;
	glDeleteRenderbuffers(1, &fx_buffer->rbo);
	fx_buffer->rbo = -1;


	if (fx_buffer->initialized) {
		wlr_egl_destroy_image(fx_buffer->renderer->egl, fx_buffer->image);

		wlr_egl_restore_context(&prev_ctx);
	}

	fx_buffer->initialized = false;
}
