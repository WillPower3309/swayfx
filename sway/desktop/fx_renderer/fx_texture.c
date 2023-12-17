#include <assert.h>
#include <stdlib.h>
#include <wlr/render/interface.h>
#include <wlr/render/gles2.h>
#include <wlr/types/wlr_buffer.h>
#include <drm_fourcc.h>

#include "log.h"
#include "render/egl.h"
#include "sway/desktop/fx_renderer/fx_renderer.h"
#include "sway/desktop/fx_renderer/pixel_format.h"

static const struct wlr_texture_impl texture_impl;

bool wlr_texture_is_fx(struct wlr_texture *wlr_texture) {
	return wlr_texture->impl == &texture_impl;
}

struct fx_texture *fx_get_texture(struct wlr_texture *wlr_texture) {
	assert(wlr_texture_is_fx(wlr_texture));
	return (struct fx_texture *) wlr_texture;
}

static bool check_stride(const struct wlr_pixel_format_info *fmt,
		uint32_t stride, uint32_t width) {
	if (stride % (fmt->bpp / 8) != 0) {
		sway_log(SWAY_ERROR, "Invalid stride %d (incompatible with %d "
			"bytes-per-pixel)", stride, fmt->bpp / 8);
		return false;
	}
	if (stride < width * (fmt->bpp / 8)) {
		sway_log(SWAY_ERROR, "Invalid stride %d (too small for %d "
			"bytes-per-pixel and width %d)", stride, fmt->bpp / 8, width);
		return false;
	}
	return true;
}

static bool fx_texture_update_from_buffer(struct wlr_texture *wlr_texture,
		struct wlr_buffer *buffer, pixman_region32_t *damage) {
	struct fx_texture *texture = fx_get_texture(wlr_texture);

	if (texture->target != GL_TEXTURE_2D || texture->image != EGL_NO_IMAGE_KHR) {
		return false;
	}

	void *data;
	uint32_t format;
	size_t stride;
	if (!wlr_buffer_begin_data_ptr_access(buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_READ, &data, &format, &stride)) {
		return false;
	}

	if (format != texture->drm_format) {
		wlr_buffer_end_data_ptr_access(buffer);
		return false;
	}

	const struct wlr_pixel_format_info *drm_fmt =
		drm_get_pixel_format_info(texture->drm_format);
	assert(drm_fmt);

	if (!check_stride(drm_fmt, stride, buffer->width)) {
		wlr_buffer_end_data_ptr_access(buffer);
		return false;
	}

	struct wlr_egl_context prev_ctx;
	wlr_egl_save_context(&prev_ctx);
	wlr_egl_make_current(texture->fx_renderer->egl);

	glBindTexture(GL_TEXTURE_2D, texture->tex);

	int rects_len = 0;
	pixman_box32_t *rects = pixman_region32_rectangles(damage, &rects_len);

	for (int i = 0; i < rects_len; i++) {
		pixman_box32_t rect = rects[i];

		glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, stride / (drm_fmt->bpp / 8));
		glPixelStorei(GL_UNPACK_SKIP_PIXELS_EXT, rect.x1);
		glPixelStorei(GL_UNPACK_SKIP_ROWS_EXT, rect.y1);

		int width = rect.x2 - rect.x1;
		int height = rect.y2 - rect.y1;
		glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x1, rect.y1, width, height,
			GL_RGBA, GL_UNSIGNED_BYTE, data);
	}

	glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS_EXT, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS_EXT, 0);

	glBindTexture(GL_TEXTURE_2D, 0);

	wlr_egl_restore_context(&prev_ctx);

	wlr_buffer_end_data_ptr_access(buffer);

	return true;
}

static bool fx_texture_invalidate(struct fx_texture *texture) {
	if (texture->image == EGL_NO_IMAGE_KHR) {
		return false;
	}
	if (texture->target == GL_TEXTURE_EXTERNAL_OES) {
		// External changes are immediately made visible by the GL implementation
		return true;
	}

	struct wlr_egl_context prev_ctx;
	wlr_egl_save_context(&prev_ctx);
	wlr_egl_make_current(texture->fx_renderer->egl);

	glBindTexture(texture->target, texture->tex);
	texture->fx_renderer->procs.glEGLImageTargetTexture2DOES(texture->target,
		texture->image);
	glBindTexture(texture->target, 0);

	wlr_egl_restore_context(&prev_ctx);

	return true;
}

void fx_texture_destroy(struct fx_texture *texture) {
	wl_list_remove(&texture->link);
	if (texture->buffer != NULL) {
		wlr_addon_finish(&texture->buffer_addon);
	}

	struct wlr_egl_context prev_ctx;
	wlr_egl_save_context(&prev_ctx);
	wlr_egl_make_current(texture->fx_renderer->egl);

	glDeleteTextures(1, &texture->tex);
	wlr_egl_destroy_image(texture->fx_renderer->egl, texture->image);

	wlr_egl_restore_context(&prev_ctx);

	free(texture);
}

static void fx_texture_unref(struct wlr_texture *wlr_texture) {
	struct fx_texture *texture = fx_get_texture(wlr_texture);
	if (texture->buffer != NULL) {
		// Keep the texture around, in case the buffer is re-used later. We're
		// still listening to the buffer's destroy event.
		wlr_buffer_unlock(texture->buffer);
	} else {
		fx_texture_destroy(texture);
	}
}

static const struct wlr_texture_impl texture_impl = {
	.update_from_buffer = fx_texture_update_from_buffer,
	.destroy = fx_texture_unref,
};

static struct fx_texture *fx_texture_create(
		struct fx_renderer *renderer, uint32_t width, uint32_t height) {
	struct fx_texture *texture = calloc(1, sizeof(struct fx_texture));
	if (texture == NULL) {
		sway_log_errno(SWAY_ERROR, "Allocation failed");
		return NULL;
	}
	wlr_texture_init(&texture->wlr_texture, &texture_impl, width, height);
	texture->fx_renderer = renderer;
	wl_list_insert(&renderer->textures, &texture->link);
	return texture;
}

static struct fx_texture *fx_texture_from_pixels(
		struct fx_renderer *renderer,
		uint32_t drm_format, uint32_t stride, uint32_t width,
		uint32_t height, const void *data) {
	const struct wlr_pixel_format_info *drm_fmt =
		drm_get_pixel_format_info(drm_format);
	assert(drm_fmt);

	if (!check_stride(drm_fmt, stride, width)) {
		return NULL;
	}

	struct fx_texture *texture =
		fx_texture_create(renderer, width, height);
	if (texture == NULL) {
		return NULL;
	}
	texture->target = GL_TEXTURE_2D;
	texture->has_alpha = false;
	texture->drm_format = DRM_FORMAT_XBGR8888;

	struct wlr_egl_context prev_ctx;
	wlr_egl_save_context(&prev_ctx);
	wlr_egl_make_current(renderer->egl);

	glGenTextures(1, &texture->tex);
	glBindTexture(GL_TEXTURE_2D, texture->tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, stride / (drm_fmt->bpp / 8));
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, data);
	glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);

	glBindTexture(GL_TEXTURE_2D, 0);

	wlr_egl_restore_context(&prev_ctx);

	return texture;
}

static struct wlr_texture *fx_texture_from_dmabuf(
		struct fx_renderer *renderer,
		struct wlr_dmabuf_attributes *attribs) {

	if (!renderer->procs.glEGLImageTargetTexture2DOES) {
		return NULL;
	}

	struct fx_texture *texture =
		fx_texture_create(renderer, attribs->width, attribs->height);
	if (texture == NULL) {
		return NULL;
	}
	texture->drm_format = DRM_FORMAT_INVALID; // texture can't be written anyways

	const struct wlr_pixel_format_info *drm_fmt =
		drm_get_pixel_format_info(attribs->format);
	if (drm_fmt != NULL) {
		texture->has_alpha = drm_fmt->has_alpha;
	} else {
		// We don't know, assume the texture has an alpha channel
		texture->has_alpha = true;
	}

	struct wlr_egl_context prev_ctx;
	wlr_egl_save_context(&prev_ctx);
	wlr_egl_make_current(renderer->egl);

	bool external_only;
	texture->image =
		wlr_egl_create_image_from_dmabuf(renderer->egl, attribs, &external_only);
	if (texture->image == EGL_NO_IMAGE_KHR) {
		sway_log(SWAY_ERROR, "Failed to create EGL image from DMA-BUF");
		wlr_egl_restore_context(&prev_ctx);
		wl_list_remove(&texture->link);
		free(texture);
		return NULL;
	}

	texture->target = external_only ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;

	glGenTextures(1, &texture->tex);
	glBindTexture(texture->target, texture->tex);
	glTexParameteri(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	renderer->procs.glEGLImageTargetTexture2DOES(texture->target, texture->image);
	glBindTexture(texture->target, 0);

	wlr_egl_restore_context(&prev_ctx);

	return &texture->wlr_texture;
}

static void texture_handle_buffer_destroy(struct wlr_addon *addon) {
	struct fx_texture *texture =
		wl_container_of(addon, texture, buffer_addon);
	fx_texture_destroy(texture);
}

static const struct wlr_addon_interface texture_addon_impl = {
	.name = "fx_texture",
	.destroy = texture_handle_buffer_destroy,
};

static struct fx_texture *fx_texture_from_dmabuf_buffer(
		struct fx_renderer *renderer, struct wlr_buffer *buffer,
		struct wlr_dmabuf_attributes *dmabuf) {
	struct wlr_addon *addon =
		wlr_addon_find(&buffer->addons, renderer, &texture_addon_impl);
	if (addon != NULL) {
		struct fx_texture *texture =
			wl_container_of(addon, texture, buffer_addon);
		if (!fx_texture_invalidate(texture)) {
			sway_log(SWAY_ERROR, "Failed to invalidate texture");
			return false;
		}
		wlr_buffer_lock(texture->buffer);
		return texture;
	}

	struct wlr_texture *wlr_texture =
		fx_texture_from_dmabuf(renderer, dmabuf);
	if (wlr_texture == NULL) {
		return false;
	}

	struct fx_texture *texture = fx_get_texture(wlr_texture);
	texture->buffer = wlr_buffer_lock(buffer);
	wlr_addon_init(&texture->buffer_addon, &buffer->addons,
		renderer, &texture_addon_impl);

	return texture;
}

struct fx_texture *fx_texture_from_buffer(struct fx_renderer *renderer,
		struct wlr_buffer *buffer) {
	void *data;
	uint32_t format;
	size_t stride;
	struct wlr_dmabuf_attributes dmabuf;
	if (wlr_buffer_get_dmabuf(buffer, &dmabuf)) {
		return fx_texture_from_dmabuf_buffer(renderer, buffer, &dmabuf);
	} else if (wlr_buffer_begin_data_ptr_access(buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_READ, &data, &format, &stride)) {
		struct fx_texture *tex = fx_texture_from_pixels(renderer,
			format, stride, buffer->width, buffer->height, data);
		wlr_buffer_end_data_ptr_access(buffer);
		return tex;
	} else {
		return NULL;
	}
}

void wlr_gles2_texture_get_fx_attribs(struct fx_texture *texture,
		struct wlr_gles2_texture_attribs *attribs) {
	memset(attribs, 0, sizeof(*attribs));
	attribs->target = texture->target;
	attribs->tex = texture->tex;
	attribs->has_alpha = texture->has_alpha;
}
