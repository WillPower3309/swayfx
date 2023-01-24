#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <drm_fourcc.h>
#include "cairo.h"
#include "stringop.h"
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"

/**
 * Extract a `source_width` by `source_height` rectangle from cairo surface `source`
 * starting at `source_x`, `source_y`, scale it down to `dest_width` by `dest_height` 
 * and transform it to a `wlr_texture` using `renderer`.
 */
static struct wlr_texture *extract_scaled_texture(struct wlr_renderer * renderer, cairo_surface_t *source,
		float source_x, float source_y, float source_width, float source_height,
		float dest_width, float dest_height) {
	float w = cairo_image_surface_get_width(source);
	float h = cairo_image_surface_get_height(source);
	cairo_surface_t *unscaled_surface = 
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, source_width, source_height);
	cairo_t *cr = cairo_create(unscaled_surface);
	cairo_set_source_surface(cr, source, -source_x, -source_y);
	cairo_rectangle (cr, 0, 0, source_width, source_height);
	cairo_fill(cr);
	cairo_destroy(cr);

	w = cairo_image_surface_get_width(unscaled_surface);
	h = cairo_image_surface_get_height(unscaled_surface);
	cairo_surface_t *image_surface = 
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, dest_width, dest_height);
	cr = cairo_create(image_surface);
	cairo_scale(cr, dest_width / w, dest_height / h);
	cairo_set_source_surface(cr, unscaled_surface, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);

	cairo_surface_flush(image_surface);
	struct wlr_texture *texture = wlr_texture_from_pixels(renderer, DRM_FORMAT_ARGB8888,
		cairo_image_surface_get_width(image_surface) * 4,
		cairo_image_surface_get_width(image_surface),
		cairo_image_surface_get_height(image_surface),
		cairo_image_surface_get_data(image_surface));
	cairo_surface_destroy(image_surface);

	return texture;
}

struct cmd_results *cmd_border_texture(int argc, char **argv) {
	if (!config->active) return cmd_results_new(CMD_DEFER, NULL);
	
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "border_texture", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	float border_thickness = config->border_thickness;

	if (!root->outputs->length) {
		return cmd_results_new(CMD_FAILURE, "Can't run border_texture with no outputs available");
	}
	struct sway_output *output = root->outputs->items[0];
	struct wlr_renderer *renderer = output->wlr_output->renderer;

	char *path = strdup("contrib/borders/bevel.png");
	if (!expand_path(&path)) {
		error = cmd_results_new(CMD_INVALID, "Invalid syntax (%s)", path);
		free(path);
		return error;
	}
	if (!path) {
		sway_log(SWAY_ERROR, "Failed to allocate expanded path");
		return cmd_results_new(CMD_FAILURE, "Unable to allocate resource");
	}

	bool can_access = access(path, F_OK) != -1;
	if (!can_access) {
		free(path);
		return cmd_results_new(CMD_FAILURE, "Unable to access border images file '%s'", path);

	}

	cairo_surface_t *combined_surface = cairo_image_surface_create_from_png(path);
	free(path);
	float w = cairo_image_surface_get_width(combined_surface);
	float h = cairo_image_surface_get_height(combined_surface);

	config->bottom_edge_border_texture = extract_scaled_texture(renderer, combined_surface, 
			floor(w / 2), floor(h / 2), 1, floor(h / 2), 1, border_thickness);
	config->top_edge_border_texture = extract_scaled_texture(renderer, combined_surface, 
			floor(w / 2), 0, 1, floor(h / 2), 1, border_thickness);
	config->left_edge_border_texture = extract_scaled_texture(renderer, combined_surface, 
			0, floor(h / 2), floor(w / 2), 1, border_thickness, 1);
	config->right_edge_border_texture = extract_scaled_texture(renderer, combined_surface, 
			floor(w / 2), floor(h / 2), floor(w / 2), 1, border_thickness, 1);

	config->bl_corner_border_texture = extract_scaled_texture(renderer, combined_surface,
			0, floor(h / 2) + 1, floor(w / 2), floor(h / 2), 
			border_thickness, border_thickness);
	config->br_corner_border_texture = extract_scaled_texture(renderer, combined_surface,
			floor(w / 2) + 1, floor(h / 2) + 1, floor(w / 2), floor(h / 2), 
			border_thickness, border_thickness);
	config->tl_corner_border_texture = extract_scaled_texture(renderer, combined_surface,
			0, 0, floor(w / 2), floor(h / 2), 
			border_thickness, border_thickness);
	config->tr_corner_border_texture = extract_scaled_texture(renderer, combined_surface,
			floor(w / 2) + 1, 0, floor(w / 2), floor(h / 2), 
			border_thickness, border_thickness);

	cairo_surface_destroy(combined_surface);


	return cmd_results_new(CMD_SUCCESS, NULL);
}
