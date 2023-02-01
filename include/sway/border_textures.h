#ifndef _SWAY_BORDER_TEXTURES_H
#define _SWAY_BORDER_TEXTURES_H

#include <cairo.h>
#include <wlr/render/wlr_renderer.h>
#include "list.h"

/**
 * Stores unscaled border textures and caches any scaled ones.
 * See `get_border_textures` for usage.
 */
struct border_textures_manager {
  cairo_surface_t *bottom_edge_texture_surface;
  cairo_surface_t *top_edge_texture_surface;
  cairo_surface_t *left_edge_texture_surface;
  cairo_surface_t *right_edge_texture_surface;
  cairo_surface_t *bl_corner_texture_surface;
  cairo_surface_t *br_corner_texture_surface;
  cairo_surface_t *tl_corner_texture_surface;
  cairo_surface_t *tr_corner_texture_surface;

  list_t *scaled;
};

/**
 * Stores usable wlr textures and remembers their scale.
 */
struct border_textures {
  int scale;

	struct wlr_texture *bottom_edge_texture;
	struct wlr_texture *top_edge_texture;
	struct wlr_texture *left_edge_texture;
	struct wlr_texture *right_edge_texture;
	struct wlr_texture *bl_corner_texture;
	struct wlr_texture *br_corner_texture;
	struct wlr_texture *tl_corner_texture;
	struct wlr_texture *tr_corner_texture;
};

struct border_textures_manager *create_border_textures_manager(cairo_surface_t *);
struct border_textures *scale_border_textures(struct wlr_renderer *, struct border_textures_manager *, int);
void delete_border_textures(struct border_textures*);
void delete_border_textures_manager(struct border_textures_manager*);
/**
 * Pull cached scaled textures or render a new set if appropriate scale wasn't found.
 * Returns NULL when scale < 1. This procedure is the only way you should interact with border_textures_manager.
 */
struct border_textures *get_border_textures(struct wlr_renderer *, struct border_textures_manager*, int);

#endif
