#include <assert.h>
#include <wlr/render/gles2.h>

#include "sway/desktop/fx_renderer/fx_texture.h"

struct fx_texture fx_texture_create() {
	return (struct fx_texture) {
		.id = 0,
		.target = 0,
		.width = -1,
		.height = -1,
	};
}

struct fx_texture fx_texture_from_wlr_texture(struct wlr_texture *texture) {
	assert(wlr_texture_is_gles2(texture));

	struct wlr_gles2_texture_attribs texture_attrs;
	wlr_gles2_texture_get_attribs(texture, &texture_attrs);

	return (struct fx_texture) {
		.target = texture_attrs.target,
		.id = texture_attrs.tex,
		.has_alpha = texture_attrs.has_alpha,
		.width = texture->width,
		.height = texture->height,
	};
}

void fx_texture_release(struct fx_texture *texture) {
	if (texture->id) {
		glDeleteTextures(1, &texture->id);
	}
	texture->id = 0;
	texture->width = -1;
	texture->height = -1;
}
