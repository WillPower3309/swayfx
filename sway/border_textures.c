#include <stdlib.h>
#include <assert.h>
#include <drm_fourcc.h>
#include <wlr/render/wlr_texture.h>
#include "sway/border_textures.h"
#include "log.h"

cairo_surface_t *extract_rectangle(cairo_surface_t *source, int x, int y, int width, int height) {
	cairo_surface_t *rect =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

	cairo_t *cr = cairo_create(rect);
	cairo_set_source_surface(cr, source, -x, -y);
	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill(cr);
	cairo_destroy(cr);

	return rect;
}

struct border_textures_manager *create_border_textures_manager(cairo_surface_t *source) {
	struct border_textures_manager *textures = malloc(sizeof(struct border_textures_manager));
	float w = cairo_image_surface_get_width(source);
	float h = cairo_image_surface_get_height(source);

	if (!((w > 0) && (h > 0))) {
		sway_log(SWAY_ERROR, "Border texture invalid.");
		return NULL;
	}

	textures->bottom_edge_texture_surface =
		extract_rectangle(source, floor(w / 2), floor(h / 2),  1, floor(h / 2));
	textures->top_edge_texture_surface =
		extract_rectangle(source, floor(w / 2), 0, 1, floor(h / 2));
	textures->left_edge_texture_surface =
		extract_rectangle(source, 0, floor(h / 2), floor(w / 2), 1);
	textures->right_edge_texture_surface =
		extract_rectangle(source, floor(w / 2), floor(h / 2), floor(w / 2), 1);
	textures->bl_corner_texture_surface =
		extract_rectangle(source, 0, floor(h / 2) + 1, floor(w / 2), floor(h / 2));
	textures->br_corner_texture_surface =
		extract_rectangle(source, floor(w / 2) + 1, floor(h / 2) + 1, floor(w / 2), floor(h / 2));
	textures->tl_corner_texture_surface =
		extract_rectangle(source, 0, 0, floor(w / 2), floor(h / 2));
	textures->tr_corner_texture_surface =
		extract_rectangle(source, floor(w / 2) + 1, 0, floor(w / 2), floor(h / 2));

	textures->scaled = create_list();

	return textures;
};

struct wlr_texture *scale_surface(
		struct wlr_renderer *renderer, cairo_surface_t *source, float width, float height) {
	int w = cairo_image_surface_get_width(source);
	int h = cairo_image_surface_get_height(source);

	assert(w > 0);
	assert(h > 0);

	cairo_surface_t *image_surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cr = cairo_create(image_surface);
	cairo_scale(cr, width / w, height / h);
	cairo_set_source_surface(cr, source, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);

	struct wlr_texture *texture = wlr_texture_from_pixels(renderer, DRM_FORMAT_ARGB8888,
		cairo_image_surface_get_width(image_surface) * 4,
		cairo_image_surface_get_width(image_surface),
		cairo_image_surface_get_height(image_surface),
		cairo_image_surface_get_data(image_surface));
	cairo_surface_destroy(image_surface);
	return texture;
}

struct border_textures *scale_border_textures(
		struct wlr_renderer *renderer, struct border_textures_manager *source, int scale) {
	struct border_textures *textures = malloc(sizeof(struct border_textures));

	textures->bottom_edge_texture = scale_surface(renderer, source->bottom_edge_texture_surface, 1, scale);
	textures->top_edge_texture =	scale_surface(renderer, source->top_edge_texture_surface, 1, scale);
	textures->left_edge_texture =	scale_surface(renderer, source->left_edge_texture_surface, scale, 1);
	textures->right_edge_texture =	scale_surface(renderer, source->right_edge_texture_surface, scale, 1);
	textures->bl_corner_texture =	scale_surface(renderer, source->bl_corner_texture_surface, scale, scale);
	textures->br_corner_texture =	scale_surface(renderer, source->br_corner_texture_surface, scale, scale);
	textures->tl_corner_texture =	scale_surface(renderer, source->tl_corner_texture_surface, scale, scale);
	textures->tr_corner_texture =	scale_surface(renderer, source->tr_corner_texture_surface, scale, scale);
	textures->scale = scale;

	return textures;
};

void delete_border_textures(struct border_textures* border_textures) {
	if (border_textures == NULL) {
		return;
	}

	wlr_texture_destroy(border_textures->bottom_edge_texture);
	wlr_texture_destroy(border_textures->left_edge_texture);
	wlr_texture_destroy(border_textures->right_edge_texture);
	wlr_texture_destroy(border_textures->top_edge_texture);
	wlr_texture_destroy(border_textures->bl_corner_texture);
	wlr_texture_destroy(border_textures->br_corner_texture);
	wlr_texture_destroy(border_textures->tl_corner_texture);
	wlr_texture_destroy(border_textures->tr_corner_texture);

	free(border_textures);
};

void delete_border_textures_manager(struct border_textures_manager* border_textures_manager) {
	if (border_textures_manager == NULL) {
		return;
	}

	cairo_surface_destroy(border_textures_manager->bottom_edge_texture_surface);
	cairo_surface_destroy(border_textures_manager->top_edge_texture_surface);
	cairo_surface_destroy(border_textures_manager->left_edge_texture_surface);
	cairo_surface_destroy(border_textures_manager->right_edge_texture_surface);
	cairo_surface_destroy(border_textures_manager->bl_corner_texture_surface);
	cairo_surface_destroy(border_textures_manager->br_corner_texture_surface);
	cairo_surface_destroy(border_textures_manager->tl_corner_texture_surface);
	cairo_surface_destroy(border_textures_manager->tr_corner_texture_surface);

	for (int i = 0; i < border_textures_manager->scaled->length; ++i) {
		delete_border_textures(border_textures_manager->scaled->items[i]);
	}
	list_free(border_textures_manager->scaled);

	free(border_textures_manager);
};

struct border_textures *get_border_textures(
		struct wlr_renderer *renderer, struct border_textures_manager* border_textures_manager, int scale) {
	struct border_textures *textures = NULL;

	if (scale < 1) {
		return NULL;
	}

	for (int i = 0; i < border_textures_manager->scaled->length; ++i) {
		textures = border_textures_manager->scaled->items[i];
		if (textures->scale == scale) {
			return textures;
		}
	}

	sway_log(SWAY_DEBUG, "Border textures: generating textures (%i).", scale);
	textures = scale_border_textures(renderer, border_textures_manager, scale);
	if (!textures) {
		sway_log(SWAY_ERROR, "Failed generating border textures.");
		return NULL;
	}
	if (border_textures_manager->scaled->length > 100) {
		sway_log(SWAY_ERROR,
				"Border texture cache contains more than 100 entries.");
	}
	list_add(border_textures_manager->scaled, textures);
	return textures;
};
