#define _POSIX_C_SOURCE 200809L
#include <scenefx/render/pass.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wlr/config.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/util/region.h>
#include "config.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/layers.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

static void transform_output_damage(pixman_region32_t *damage, struct wlr_output *output) {
	int ow, oh;
	wlr_output_transformed_resolution(output, &ow, &oh);
	enum wl_output_transform transform =
		wlr_output_transform_invert(output->transform);
	wlr_region_transform(damage, damage, transform, ow, oh);
}

static void transform_output_box(struct wlr_box *box, struct wlr_output *output) {
	int ow, oh;
	wlr_output_transformed_resolution(output, &ow, &oh);
	enum wl_output_transform transform =
		wlr_output_transform_invert(output->transform);
	wlr_box_transform(box, box, transform, ow, oh);
}

struct decoration_data get_undecorated_decoration_data() {
	return (struct decoration_data) {
		.alpha = 1.0f,
		.dim = 0.0f,
		.dim_color = config->dim_inactive_colors.unfocused,
		.corner_radius = 0,
		.saturation = 1.0f,
		.has_titlebar = false,
		.blur = false,
		.discard_transparent = false,
		.shadow = false,
	};
}

/*
// TODO: Remove this ugly abomination with a complete border rework...
enum corner_location get_rotated_corner(enum corner_location corner_location,
		enum wl_output_transform transform) {
	if (corner_location == ALL || corner_location == NONE) {
		return corner_location;
	}
	switch (transform) {
		case WL_OUTPUT_TRANSFORM_NORMAL:
			return corner_location;
		case WL_OUTPUT_TRANSFORM_90:
			return (corner_location + 1) % 4;
		case WL_OUTPUT_TRANSFORM_180:
			return (corner_location + 2) % 4;
		case WL_OUTPUT_TRANSFORM_270:
			return (corner_location + 3) % 4;
		case WL_OUTPUT_TRANSFORM_FLIPPED:
			return (corner_location + (1 - 2 * (corner_location % 2))) % 4;
		case WL_OUTPUT_TRANSFORM_FLIPPED_90:
			return (corner_location + (4 - 2 * (corner_location % 2))) % 4;
		case WL_OUTPUT_TRANSFORM_FLIPPED_180:
			return (corner_location + (3 - 2 * (corner_location % 2))) % 4;
		case WL_OUTPUT_TRANSFORM_FLIPPED_270:
			return (corner_location + (2 - 2 * (corner_location % 2))) % 4;
	}
	return corner_location;
}
*/

/**
 * Apply scale to a width or height.
 *
 * One does not simply multiply the width by the scale. We allow fractional
 * scaling, which means the resulting scaled width might be a decimal.
 * So we round it.
 *
 * But even this can produce undesirable results depending on the X or Y offset
 * of the box. For example, with a scale of 1.5, a box with width=1 should not
 * scale to 2px if its X coordinate is 1, because the X coordinate would have
 * scaled to 2px.
 */
static int scale_length(int length, int offset, float scale) {
	return roundf((offset + length) * scale) - roundf(offset * scale);
}

static enum wlr_scale_filter_mode get_scale_filter(struct sway_output *output) {
	switch (output->scale_filter) {
	case SCALE_FILTER_LINEAR:
		return WLR_SCALE_FILTER_BILINEAR;
	case SCALE_FILTER_NEAREST:
		return WLR_SCALE_FILTER_NEAREST;
	default:
		abort(); // unreachable
	}
}

struct wlr_box get_monitor_box(struct wlr_output *output) {
	int width, height;
	wlr_output_transformed_resolution(output, &width, &height);
	struct wlr_box monitor_box = { 0, 0, width, height };
	return monitor_box;
}

static void render_texture(struct fx_render_context *ctx, struct wlr_texture *texture,
		const struct wlr_fbox *_src_box, const struct wlr_box *dst_box,
		const struct wlr_box *clip_box, enum wl_output_transform transform,
		struct decoration_data deco_data) {
	struct sway_output *output = ctx->output;

	struct wlr_box proj_box = *dst_box;

	struct wlr_fbox src_box = {0};
	if (_src_box) {
		src_box = *_src_box;
	}

	pixman_region32_t damage;
	pixman_region32_init_rect(&damage, proj_box.x, proj_box.y,
		proj_box.width, proj_box.height);
	pixman_region32_intersect(&damage, &damage, ctx->output_damage);

	if (clip_box) {
		pixman_region32_intersect_rect(&damage, &damage,
				clip_box->x, clip_box->y, clip_box->width, clip_box->height);
	}

	bool damaged = pixman_region32_not_empty(&damage);
	if (!damaged) {
		goto damage_finish;
	}

	transform_output_box(&proj_box, output->wlr_output);
	transform_output_damage(&damage, output->wlr_output);
	transform = wlr_output_transform_compose(transform, output->wlr_output->transform);

	fx_render_pass_add_texture(ctx->pass, &(struct fx_render_texture_options) {
		.base = {
			.texture = texture,
			.src_box = src_box,
			.dst_box = proj_box,
			.transform = transform,
			.alpha = &deco_data.alpha,
			.clip = &damage,
			.filter_mode = get_scale_filter(output),
		},
		.corner_radius = deco_data.corner_radius
	});

damage_finish:
	pixman_region32_fini(&damage);
}

/* Renders the blur for each damaged rect and swaps the buffer */
/* TODO
void render_blur_segments(struct fx_renderer *renderer,
		const float matrix[static 9], pixman_region32_t* damage,
		struct fx_framebuffer **buffer, struct blur_shader* shader,
		const struct wlr_box *box, int blur_radius) {
	if (*buffer == &renderer->effects_buffer) {
		fx_framebuffer_bind(&renderer->effects_buffer_swapped);
	} else {
		fx_framebuffer_bind(&renderer->effects_buffer);
	}

	if (pixman_region32_not_empty(damage)) {
		int nrects;
		pixman_box32_t *rects = pixman_region32_rectangles(damage, &nrects);
		for (int i = 0; i < nrects; ++i) {
			const pixman_box32_t box = rects[i];
			struct wlr_box new_box = { box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1 };
			fx_renderer_scissor(&new_box);
			fx_render_blur(renderer, matrix, buffer, shader, &new_box, blur_radius);
		}
	}

	if (*buffer != &renderer->effects_buffer) {
		*buffer = &renderer->effects_buffer;
	} else {
		*buffer = &renderer->effects_buffer_swapped;
	}
}

// Blurs the main_buffer content and returns the blurred framebuffer
struct fx_framebuffer *get_main_buffer_blur(struct fx_renderer *renderer, struct sway_output *output,
		pixman_region32_t *original_damage, const struct wlr_box *box) {
	struct wlr_output *wlr_output = output->wlr_output;
	struct wlr_box monitor_box = get_monitor_box(wlr_output);

	const enum wl_output_transform transform = wlr_output_transform_invert(wlr_output->transform);
	float matrix[9];
	wlr_matrix_project_box(matrix, &monitor_box, transform, 0, wlr_output->transform_matrix);

	float gl_matrix[9];
	wlr_matrix_multiply(gl_matrix, renderer->projection, matrix);

	pixman_region32_t damage;
	pixman_region32_init(&damage);
	pixman_region32_copy(&damage, original_damage);
	wlr_region_transform(&damage, &damage, transform, monitor_box.width, monitor_box.height);

	wlr_region_expand(&damage, &damage, config_get_blur_size());

	// Initially blur main_buffer content into the effects_buffers
	struct fx_framebuffer *current_buffer = &renderer->wlr_buffer;

	// Bind to blur framebuffer
	fx_framebuffer_bind(&renderer->effects_buffer);
	glBindTexture(renderer->wlr_buffer.texture.target, renderer->wlr_buffer.texture.id);

	// damage region will be scaled, make a temp
	pixman_region32_t tempDamage;
	pixman_region32_init(&tempDamage);

	int blur_radius = config->blur_params.radius;
	int blur_passes = config->blur_params.num_passes;

	// Downscale
	for (int i = 0; i < blur_passes; ++i) {
		wlr_region_scale(&tempDamage, &damage, 1.0f / (1 << (i + 1)));
		render_blur_segments(renderer, gl_matrix, &tempDamage, &current_buffer,
				&renderer->shaders.blur1, box, blur_radius);
	}

	// Upscale
	for (int i = blur_passes - 1; i >= 0; --i) {
		// when upsampling we make the region twice as big
		wlr_region_scale(&tempDamage, &damage, 1.0f / (1 << i));
		render_blur_segments(renderer, gl_matrix, &tempDamage, &current_buffer,
				&renderer->shaders.blur2, box, blur_radius);
	}

	float blur_noise = config->blur_params.noise;
	float blur_brightness = config->blur_params.brightness;
	float blur_contrast = config->blur_params.contrast;
	float blur_saturation = config->blur_params.saturation;

	// Render additional blur effects like saturation, noise, contrast, etc...
	if (config_should_parameters_blur_effects() && pixman_region32_not_empty(&damage)) {
		if (current_buffer == &renderer->effects_buffer) {
			fx_framebuffer_bind(&renderer->effects_buffer_swapped);
		} else {
			fx_framebuffer_bind(&renderer->effects_buffer);
		}
		int nrects;
		pixman_box32_t *rects = pixman_region32_rectangles(&damage, &nrects);
		for (int i = 0; i < nrects; ++i) {
			const pixman_box32_t box = rects[i];
			struct wlr_box new_box = { box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1 };
			fx_renderer_scissor(&new_box);
			fx_render_blur_effects(renderer, gl_matrix, &current_buffer, blur_noise,
				blur_brightness, blur_contrast, blur_saturation);
		}
		if (current_buffer != &renderer->effects_buffer) {
			current_buffer = &renderer->effects_buffer;
		} else {
			current_buffer = &renderer->effects_buffer_swapped;
		}
	}

	pixman_region32_fini(&tempDamage);
	pixman_region32_fini(&damage);

	// Bind back to the default buffer
	fx_framebuffer_bind(&renderer->wlr_buffer);

	return current_buffer;
}

void render_blur(bool optimized, struct sway_output *output,
		const pixman_region32_t *output_damage, const struct wlr_box *dst_box,
		pixman_region32_t *opaque_region, struct decoration_data *deco_data,
		struct blur_stencil_data *stencil_data) {
	struct wlr_output *wlr_output = output->wlr_output;
	struct fx_renderer *renderer = output->renderer;

	// Check if damage is inside of box rect
	pixman_region32_t damage = create_damage(*dst_box, output_damage);

	pixman_region32_t translucent_region;
	pixman_region32_init(&translucent_region);

	if (!pixman_region32_not_empty(&damage)) {
		goto damage_finish;
	}

	// Gets the translucent region
	pixman_box32_t surface_box = { 0, 0, dst_box->width, dst_box->height };
	pixman_region32_copy(&translucent_region, opaque_region);
	pixman_region32_inverse(&translucent_region, &translucent_region, &surface_box);
	if (!pixman_region32_not_empty(&translucent_region)) {
		goto damage_finish;
	}

	struct fx_framebuffer *buffer = &renderer->blur_buffer;
	if (!buffer->texture.id || !optimized) {
		pixman_region32_translate(&translucent_region, dst_box->x, dst_box->y);
		pixman_region32_intersect(&translucent_region, &translucent_region, &damage);

		// Render the blur into its own buffer
		buffer = get_main_buffer_blur(renderer, output, &translucent_region, dst_box);
	}

	// Get a stencil of the window ignoring transparent regions
	if (deco_data->discard_transparent && stencil_data) {
		fx_renderer_scissor(NULL);
		fx_renderer_stencil_mask_init();

		render_texture(wlr_output, output_damage, stencil_data->stencil_texture, stencil_data->stencil_src_box,
				dst_box, stencil_data->stencil_matrix, *deco_data);

		fx_renderer_stencil_mask_close(true);
	}

	// Draw the blurred texture
	struct wlr_box monitor_box = get_monitor_box(wlr_output);
	enum wl_output_transform transform = wlr_output_transform_invert(wlr_output->transform);
	float matrix[9];
	wlr_matrix_project_box(matrix, &monitor_box, transform, 0.0, wlr_output->transform_matrix);

	struct decoration_data blur_deco_data = get_undecorated_decoration_data();
	blur_deco_data.corner_radius = deco_data->corner_radius;
	blur_deco_data.has_titlebar = deco_data->has_titlebar;
	render_texture(wlr_output, &damage, &buffer->texture, NULL, dst_box, matrix, blur_deco_data);

	// Finish stenciling
	if (deco_data->discard_transparent && stencil_data) {
		fx_renderer_stencil_mask_fini();
	}

damage_finish:
	pixman_region32_fini(&damage);
	pixman_region32_fini(&translucent_region);
} */

// _box.x and .y are expected to be layout-local
// _box.width and .height are expected to be output-buffer-local
void render_box_shadow(struct fx_render_context *ctx, const struct wlr_box *_box,
		const float color[static 4], float blur_sigma, float corner_radius,
		float offset_x, float offset_y) {
	struct wlr_output *wlr_output = ctx->output->wlr_output;

	struct wlr_box box = *_box;
	box.x -= (ctx->output->lx - offset_x) * wlr_output->scale;
	box.y -= (ctx->output->ly - offset_y) * wlr_output->scale;

	pixman_region32_t damage;
	pixman_region32_init_rect(&damage, box.x - blur_sigma, box.y - blur_sigma,
		box.width + (blur_sigma * 2), box.height + (blur_sigma * 2));
	pixman_region32_intersect(&damage, &damage, ctx->output_damage);
	bool damaged = pixman_region32_not_empty(&damage);
	if (!damaged) {
		goto damage_finish;
	}

	transform_output_damage(&damage, wlr_output);
	transform_output_box(&box, wlr_output);

	struct fx_render_rect_options shadow_options = {
		.base = {
			.box = box,
			.clip = &damage, // Render with the original extended clip region
		},
		.scale = wlr_output->scale,// TODO: remove?
	};
	struct shadow_data shadow_data = {
		.enabled = true,
		.color = {
			.r = color[0],
			.g = color[1],
			.b = color[2],
			.a = color[3],
		},
		.blur_sigma = blur_sigma,
	};
	fx_render_pass_add_box_shadow(ctx->pass, &shadow_options, corner_radius,
			&shadow_data);

damage_finish:
	pixman_region32_fini(&damage);
}

static void render_surface_iterator(struct sway_output *output,
		struct sway_view *view, struct wlr_surface *surface,
		struct wlr_box *_box, void *_data) {
	struct render_data *data = _data;
	struct wlr_output *wlr_output = output->wlr_output;

	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if (!texture) {
		return;
	}

	struct wlr_fbox src_box;
	wlr_surface_get_buffer_source_box(surface, &src_box);

	struct wlr_box dst_box = *_box;
	struct wlr_box clip_box = *_box;
	if (data->clip_box != NULL) {
		clip_box.width = fmin(dst_box.width, data->clip_box->width);
		clip_box.height = fmin(dst_box.height, data->clip_box->height);
 	}
	scale_box(&dst_box, wlr_output->scale);
	scale_box(&clip_box, wlr_output->scale);

	struct decoration_data deco_data = data->deco_data;
	deco_data.corner_radius *= wlr_output->scale;

	/* TODO
	// render blur
	bool is_subsurface = view ? view->surface != surface : false;
	if (deco_data.blur && config_should_parameters_blur() && !is_subsurface) {
		pixman_region32_t opaque_region;
		pixman_region32_init(&opaque_region);

		bool has_alpha = false;
		if (deco_data.alpha < 1.0 || deco_data.dim_color[3] < 1.0) {
			has_alpha = true;
			pixman_region32_union_rect(&opaque_region, &opaque_region, 0, 0, 0, 0);
		} else {
			has_alpha = !surface->opaque;
			pixman_region32_copy(&opaque_region, &surface->opaque_region);
		}

		if (has_alpha) {
			struct wlr_box monitor_box = get_monitor_box(wlr_output);
			wlr_box_transform(&monitor_box, &monitor_box,
					wlr_output_transform_invert(wlr_output->transform), monitor_box.width, monitor_box.height);
			struct blur_stencil_data stencil_data = { &fx_texture, &src_box, matrix };
			bool should_optimize_blur = view ? !container_is_floating_or_child(view->container) || config->blur_xray : false;
			render_blur(should_optimize_blur, output, output_damage, &dst_box,
					&opaque_region, &deco_data, &stencil_data);
		}

		pixman_region32_fini(&opaque_region);
	} */

	deco_data.discard_transparent = false;

	render_texture(data->ctx, texture,
		&src_box, &dst_box, &clip_box, surface->current.transform, deco_data);

	wlr_presentation_surface_textured_on_output(server.presentation, surface,
		wlr_output);
}

// view will be NULL every time
static void render_layer_iterator(struct sway_output *output,
		struct sway_view *view, struct wlr_surface *surface,
		struct wlr_box *_box, void *_data) {
	// render the layer's surface
	render_surface_iterator(output, view, surface, _box, _data);

	struct render_data *data = _data;
	struct decoration_data deco_data = data->deco_data;

	// Ignore effects if this is a subsurface
	// TODO: replacement for !wlr_surface_is_layer_surface(surface)
	if (true) {
		deco_data = get_undecorated_decoration_data();
	}

	// render shadow
	if (deco_data.shadow && config_should_parameters_shadow()) {
		render_box_shadow(data->ctx, _box, config->shadow_color, config->shadow_blur_sigma,
			deco_data.corner_radius, config->shadow_offset_x, config->shadow_offset_y);
	}
}

static void render_layer_toplevel(struct fx_render_context *ctx, struct wl_list *layer_surfaces) {
	struct render_data data = {
		.deco_data = get_undecorated_decoration_data(),
 		.ctx = ctx,
	};
	output_layer_for_each_toplevel_surface(ctx->output, layer_surfaces,
		render_layer_iterator, &data);
}

 static void render_layer_popups(struct fx_render_context *ctx, struct wl_list *layer_surfaces) {
	struct render_data data = {
		.deco_data = get_undecorated_decoration_data(),
		.ctx = ctx,
	};
	output_layer_for_each_popup_surface(ctx->output, layer_surfaces,
		render_layer_iterator, &data);
}

#if HAVE_XWAYLAND
static void render_unmanaged(struct fx_render_context *ctx, struct wl_list *unmanaged) {
	struct render_data data = {
		.deco_data = get_undecorated_decoration_data(),
		.ctx = ctx,
	};
	output_unmanaged_for_each_surface(ctx->output, unmanaged,
		render_surface_iterator, &data);
}
#endif

static void render_drag_icons(struct fx_render_context *ctx, struct wl_list *drag_icons) {
	struct render_data data = {
		.deco_data = get_undecorated_decoration_data(),
		.ctx = ctx,
	};
 	output_drag_icons_for_each_surface(ctx->output, drag_icons,
		render_surface_iterator, &data);
}

/* TODO
void render_whole_output(struct fx_renderer *renderer, struct wlr_output *wlr_output,
		pixman_region32_t *output_damage, struct fx_texture *texture) {
	struct wlr_box monitor_box = get_monitor_box(wlr_output);
	enum wl_output_transform transform = wlr_output_transform_invert(wlr_output->transform);
	float matrix[9];
	wlr_matrix_project_box(matrix, &monitor_box, transform, 0.0, wlr_output->transform_matrix);

	render_texture(wlr_output, output_damage, texture, NULL, &monitor_box, matrix, get_undecorated_decoration_data());
}

void render_output_blur(struct sway_output *output, pixman_region32_t *damage) {
	struct wlr_output *wlr_output = output->wlr_output;
	struct fx_renderer *renderer = output->renderer;

	struct wlr_box monitor_box = get_monitor_box(wlr_output);
	pixman_region32_t fake_damage;
	pixman_region32_init_rect(&fake_damage, 0, 0, monitor_box.width, monitor_box.height);

	// Render the blur
	struct fx_framebuffer *buffer = get_main_buffer_blur(renderer, output, &fake_damage, &monitor_box);

	// Render the newly blurred content into the blur_buffer
	fx_framebuffer_update(&renderer->blur_buffer,
			output->renderer->viewport_width, output->renderer->viewport_height);
	fx_framebuffer_bind(&renderer->blur_buffer);

	// Clear the damaged region of the blur_buffer
	float clear_color[] = { 0, 0, 0, 0 };
	int nrects;
	pixman_box32_t *rects = pixman_region32_rectangles(damage, &nrects);
	for (int i = 0; i < nrects; ++i) {
		scissor_output(wlr_output, &rects[i]);
		fx_renderer_clear(clear_color);
	}
	render_whole_output(renderer, wlr_output, &fake_damage, &buffer->texture);
	fx_framebuffer_bind(&renderer->wlr_buffer);

	pixman_region32_fini(&fake_damage);

	renderer->blur_buffer_dirty = false;
} */

// _box.x and .y are expected to be layout-local
// _box.width and .height are expected to be output-buffer-local
void render_rect(struct fx_render_context *ctx, const struct wlr_box *_box,
		float color[static 4]) {
	struct wlr_output *wlr_output = ctx->output->wlr_output;

	struct wlr_box box = *_box;
	box.x -= ctx->output->lx * wlr_output->scale;
	box.y -= ctx->output->ly * wlr_output->scale;

	pixman_region32_t damage;
	pixman_region32_init_rect(&damage, box.x, box.y,
		box.width, box.height);
	pixman_region32_intersect(&damage, &damage, ctx->output_damage);
	bool damaged = pixman_region32_not_empty(&damage);
	if (!damaged) {
		goto damage_finish;
	}

	transform_output_damage(&damage, wlr_output);
	transform_output_box(&box, wlr_output);

	fx_render_pass_add_rect(ctx->pass, &(struct fx_render_rect_options){
		.base = {
			.box = box,
			.color = {
				.r = color[0],
				.g = color[1],
				.b = color[2],
				.a = color[3],
			},
			.clip = &damage,
		},
	});

damage_finish:
	pixman_region32_fini(&damage);
}

void render_rounded_rect(struct fx_render_context *ctx, const struct wlr_box *_box,
		float color[static 4], int corner_radius) {
	// TODO
	render_rect(ctx, _box, color);
}

// _box.x and .y are expected to be layout-local
// _box.width and .height are expected to be output-buffer-local
/* TODO
void render_border_corner(struct sway_output *output, pixman_region32_t *output_damage,
		const struct wlr_box *_box, const float color[static 4], int corner_radius,
		int border_thickness, enum corner_location corner_location) {
	struct wlr_output *wlr_output = output->wlr_output;
	struct fx_renderer *renderer = output->renderer;

	struct wlr_box box;
	memcpy(&box, _box, sizeof(struct wlr_box));
	box.x -= output->lx * wlr_output->scale;
	box.y -= output->ly * wlr_output->scale;

	pixman_region32_t damage = create_damage(box, output_damage);
	bool damaged = pixman_region32_not_empty(&damage);
	if (!damaged) {
		goto damage_finish;
	}

	float matrix[9];
	wlr_matrix_project_box(matrix, &box, WL_OUTPUT_TRANSFORM_NORMAL, 0,
			wlr_output->transform_matrix);

	enum wl_output_transform transform = wlr_output_transform_invert(wlr_output->transform);

	// ensure the box is updated as per the output orientation
	struct wlr_box transformed_box;
	int width, height;
	wlr_output_transformed_resolution(wlr_output, &width, &height);
	wlr_box_transform(&transformed_box, &box, transform, width, height);

	corner_location = get_rotated_corner(corner_location, transform);

	int nrects;
	pixman_box32_t *rects = pixman_region32_rectangles(&damage, &nrects);
	for (int i = 0; i < nrects; ++i) {
		scissor_output(wlr_output, &rects[i]);
		fx_render_border_corner(renderer, &transformed_box, color, matrix,
				corner_location, corner_radius, border_thickness);
	}

damage_finish:
	pixman_region32_fini(&damage);
} */

void premultiply_alpha(float color[4], float opacity) {
	color[3] *= opacity;
	color[0] *= color[3];
	color[1] *= color[3];
	color[2] *= color[3];
}

static void render_view_toplevels(struct fx_render_context *ctx,
		struct sway_view *view, struct decoration_data deco_data) {
	struct render_data data = {
		.deco_data = deco_data,
		.ctx = ctx,
	};
	// Clip the window to its view size, ignoring CSD
	struct wlr_box clip_box;
	if (!container_is_current_floating(view->container)) {
		// As we pass the geometry offsets to the surface iterator, we will
		// need to account for the offsets in the clip dimensions.
		clip_box.width = view->container->current.content_width + view->geometry.x;
		clip_box.height = view->container->current.content_height + view->geometry.y;
		data.clip_box = &clip_box;
	}
	// Render all toplevels without descending into popups
	double ox = view->container->surface_x -
		ctx->output->lx - view->geometry.x;
	double oy = view->container->surface_y -
		ctx->output->ly - view->geometry.y;
	output_surface_for_each_surface(ctx->output, view->surface, ox, oy,
			render_surface_iterator, &data);
}

static void render_view_popups(struct fx_render_context *ctx, struct sway_view *view,
		struct decoration_data deco_data) {
	struct render_data data = {
		.deco_data = deco_data,
		.ctx = ctx,
	};
	output_view_for_each_popup_surface(ctx->output, view,
		render_surface_iterator, &data);
}

static void render_saved_view(struct fx_render_context *ctx, struct sway_view *view, struct decoration_data deco_data) {
	struct sway_output *output = ctx->output;
	struct wlr_output *wlr_output = output->wlr_output;

	if (wl_list_empty(&view->saved_buffers)) {
		return;
	}

	bool floating = container_is_current_floating(view->container);

	struct sway_saved_buffer *saved_buf;
	wl_list_for_each(saved_buf, &view->saved_buffers, link) {
		if (!saved_buf->buffer->texture) {
			continue;
		}

		struct wlr_box proj_box = {
			.x = saved_buf->x - view->saved_geometry.x - output->lx,
			.y = saved_buf->y - view->saved_geometry.y - output->ly,
			.width = saved_buf->width,
			.height = saved_buf->height,
		};

		struct wlr_box output_box = {
			.width = output->width,
			.height = output->height,
		};

		struct wlr_box intersection;
		bool intersects = wlr_box_intersection(&intersection, &output_box, &proj_box);
		if (!intersects) {
			continue;
		}

		struct wlr_box dst_box = proj_box;
		struct wlr_box clip_box = proj_box;
		if (!floating) {
			clip_box.width = fmin(dst_box.width,
					view->container->current.content_width -
					(saved_buf->x - view->container->current.content_x) + view->saved_geometry.x);
			clip_box.height = fmin(dst_box.height,
					view->container->current.content_height -
					(saved_buf->y - view->container->current.content_y) + view->saved_geometry.y);
		}
		scale_box(&dst_box, wlr_output->scale);
		scale_box(&clip_box, wlr_output->scale);
		deco_data.corner_radius *= wlr_output->scale;

		// TODO: render blur

		render_texture(ctx, saved_buf->buffer->texture,
			&saved_buf->source_box, &dst_box, &clip_box, saved_buf->transform, deco_data);

		// FIXME: we should set the surface that this saved buffer originates from
		// as sampled here.
		// https://github.com/swaywm/sway/pull/4465#discussion_r321082059
	}
}

// TODO: rounded corners
/**
 * Render a view's surface, shadow, and left/bottom/right borders.
 */
static void render_view(struct fx_render_context *ctx, struct sway_container *con,
		struct border_colors *colors, struct decoration_data deco_data) {
	struct sway_view *view = con->view;

	if (!wl_list_empty(&view->saved_buffers)) {
		render_saved_view(ctx, view, deco_data);
	} else if (view->surface) {
		render_view_toplevels(ctx, view, deco_data);
	}

	struct sway_container_state *state = &con->current;

	if (state->border == B_CSD && !config->shadows_on_csd_enabled) {
		return;
	}

	struct wlr_box box;
	int corner_radius = deco_data.corner_radius;
	float output_scale = ctx->output->wlr_output->scale;

	// render shadow
	if (con->shadow_enabled && config->shadow_blur_sigma > 0 && config->shadow_color[3] > 0.0) {
		box.x = floor(state->x) - ctx->output->lx;
		box.y = floor(state->y) - ctx->output->ly;
		box.width = state->width;
		box.height = state->height;
		scale_box(&box, output_scale);
		int scaled_corner_radius = corner_radius == 0 ? 0 :
				(corner_radius + state->border_thickness) * output_scale;
		float* shadow_color = view_is_urgent(view) || state->focused ?
				config->shadow_color : config->shadow_inactive_color;
		render_box_shadow(ctx, &box, shadow_color, config->shadow_blur_sigma,
				scaled_corner_radius, config->shadow_offset_x, config->shadow_offset_y);
	}

	if (state->border == B_NONE || state->border == B_CSD) {
		return;
	}

	float color[4];

	if (state->border_left) {
		memcpy(&color, colors->child_border, sizeof(float) * 4);
		premultiply_alpha(color, con->alpha);
		box.x = floor(state->x);
		box.y = floor(state->content_y) + corner_radius;
		box.width = state->border_thickness;
		box.height = state->content_height - (2 * corner_radius);
		scale_box(&box, output_scale);
		render_rect(ctx, &box, color);
	}

	list_t *siblings = container_get_current_siblings(con);
	enum sway_container_layout layout =
		container_current_parent_layout(con);

	if (state->border_right) {
		if (!container_is_current_floating(con) && siblings->length == 1 && layout == L_HORIZ) {
			memcpy(&color, colors->indicator, sizeof(float) * 4);
		} else {
			memcpy(&color, colors->child_border, sizeof(float) * 4);
		}
		premultiply_alpha(color, con->alpha);
		box.x = floor(state->content_x + state->content_width);
		box.y = floor(state->content_y + corner_radius);
		box.width = state->border_thickness;
		box.height = state->content_height - (2 * corner_radius);
		scale_box(&box, output_scale);
		render_rect(ctx, &box, color);
	}

	if (state->border_bottom) {
		if (!container_is_current_floating(con) && siblings->length == 1 && layout == L_VERT) {
			memcpy(&color, colors->indicator, sizeof(float) * 4);
		} else {
			memcpy(&color, colors->child_border, sizeof(float) * 4);
		}
		premultiply_alpha(color, con->alpha);
		box.x = floor(state->x) + corner_radius;
		box.y = floor(state->content_y + state->content_height);
		box.width = state->width - (2 * corner_radius);
		box.height = state->border_thickness;
		scale_box(&box, output_scale);
		render_rect(ctx, &box, color);
	}
}

// TODO: rounded corners
/**
 * Render a titlebar.
 *
 * Care must be taken not to render over the same pixel multiple times,
 * otherwise the colors will be incorrect when using opacity.
 *
 * The height is: 1px border, 3px padding, font height, 3px padding, 1px border
 * The left side is: 1px border, 2px padding, title
 */
static void render_titlebar(struct fx_render_context *ctx, struct sway_container *con,
		int x, int y, int width, struct border_colors *colors, int corner_radius,
		struct wlr_texture *title_texture, struct wlr_texture *marks_texture) {
	struct wlr_box box;
	float color[4];
	struct sway_output *output = ctx->output;
	float output_scale = output->wlr_output->scale;
	double output_x = output->lx;
	double output_y = output->ly;
	int titlebar_border_thickness = config->titlebar_border_thickness;
	int titlebar_h_padding = config->titlebar_h_padding;
	int titlebar_v_padding = config->titlebar_v_padding;
	enum alignment title_align = config->title_align;
	// value by which all heights should be adjusted to counteract removed bottom border
	// TODO: int bottom_border_compensation = config->titlebar_separator ? 0 : titlebar_border_thickness;

	// Single pixel bar above title
	memcpy(&color, colors->border, sizeof(float) * 4);
	premultiply_alpha(color, con->alpha);
	box.x = x + corner_radius;
	box.y = y;
	box.width = width - (2 * corner_radius);
	box.height = titlebar_border_thickness;
	scale_box(&box, output_scale);
	render_rect(ctx, &box, color);

	// Single pixel bar below title
	box.x = x + corner_radius;
	box.y = y + container_titlebar_height() - titlebar_border_thickness;
	box.width = width - (2 * corner_radius);
	box.height = titlebar_border_thickness;
	scale_box(&box, output_scale);
	render_rect(ctx, &box, color);

	// Single pixel left edge
	box.x = x;
	box.y = y + titlebar_border_thickness;
	box.width = titlebar_border_thickness;
	box.height = container_titlebar_height() - titlebar_border_thickness * 2;
	scale_box(&box, output_scale);
	render_rect(ctx, &box, color);

	// Single pixel right edge
	box.x = x + width - titlebar_border_thickness;
	box.y = y + titlebar_border_thickness;
	box.width = titlebar_border_thickness;
	box.height = container_titlebar_height() - titlebar_border_thickness * 2;
	scale_box(&box, output_scale);
	render_rect(ctx, &box, color);

	int inner_x = x - output_x + titlebar_h_padding;
	int bg_y = y + titlebar_border_thickness;
	size_t inner_width = width - titlebar_h_padding * 2;

	// output-buffer local
	int ob_inner_x = roundf(inner_x * output_scale);
	int ob_inner_width = scale_length(inner_width, inner_x, output_scale);
	int ob_bg_height = scale_length(
			(titlebar_v_padding - titlebar_border_thickness) * 2 +
			config->font_height, bg_y, output_scale);

	// title marks textures should have no eyecandy
	struct decoration_data deco_data = get_undecorated_decoration_data();
	deco_data.alpha = con->alpha;

	// Marks
	int ob_marks_x = 0; // output-buffer-local
	int ob_marks_width = 0; // output-buffer-local
	if (config->show_marks && marks_texture) {
		struct wlr_box texture_box = {
			.width = marks_texture->width,
			.height = marks_texture->height,
		};
		ob_marks_width = texture_box.width;

		// The marks texture might be shorter than the config->font_height, in
		// which case we need to pad it as evenly as possible above and below.
		int ob_padding_total = ob_bg_height - texture_box.height;
		int ob_padding_above = floor(ob_padding_total / 2.0);
		int ob_padding_below = ceil(ob_padding_total / 2.0);

		// Render texture. If the title is on the right, the marks will be on
		// the left. Otherwise, they will be on the right.
		if (title_align == ALIGN_RIGHT || texture_box.width > ob_inner_width) {
			texture_box.x = ob_inner_x;
		} else {
			texture_box.x = ob_inner_x + ob_inner_width - texture_box.width;
		}
		ob_marks_x = texture_box.x;

		texture_box.y = round((bg_y - output_y) * output_scale) +
			ob_padding_above;

		struct wlr_box clip_box = texture_box;
		if (ob_inner_width < clip_box.width) {
			clip_box.width = ob_inner_width;
		}
		render_texture(ctx, marks_texture,
			NULL, &texture_box, &clip_box, WL_OUTPUT_TRANSFORM_NORMAL, deco_data);

		// Padding above
		memcpy(&color, colors->background, sizeof(float) * 4);
		premultiply_alpha(color, con->alpha);
		box.x = clip_box.x + round(output_x * output_scale);
		box.y = roundf((y + titlebar_border_thickness) * output_scale);
		box.width = clip_box.width;
		box.height = ob_padding_above;
		render_rect(ctx, &box, color);

		// Padding below
		box.y += ob_padding_above + clip_box.height;
		box.height = ob_padding_below;
		render_rect(ctx, &box, color);
	}

	// Title text
	int ob_title_x = 0;  // output-buffer-local
	int ob_title_width = 0; // output-buffer-local
	if (title_texture) {
		struct wlr_box texture_box = {
			.width = title_texture->width,
			.height = title_texture->height,
		};

		// The effective output may be NULL when con is not on any output.
		// This can happen because we render all children of containers,
		// even those that are out of the bounds of any output.
		struct sway_output *effective = container_get_effective_output(con);
		float title_scale = effective ? effective->wlr_output->scale : output_scale;
		texture_box.width = texture_box.width * output_scale / title_scale;
		texture_box.height = texture_box.height * output_scale / title_scale;
		ob_title_width = texture_box.width;

		// The title texture might be shorter than the config->font_height,
		// in which case we need to pad it above and below.
		int ob_padding_above = roundf((titlebar_v_padding -
					titlebar_border_thickness) * output_scale);
		int ob_padding_below = ob_bg_height - ob_padding_above -
			texture_box.height;

		// Render texture
		if (texture_box.width > ob_inner_width - ob_marks_width) {
			texture_box.x = (title_align == ALIGN_RIGHT && ob_marks_width)
				? ob_marks_x + ob_marks_width : ob_inner_x;
		} else if (title_align == ALIGN_LEFT) {
			texture_box.x = ob_inner_x;
		} else if (title_align == ALIGN_CENTER) {
			// If there are marks visible, center between the edge and marks.
			// Otherwise, center in the inner area.
			if (ob_marks_width) {
				texture_box.x = (ob_inner_x + ob_marks_x) / 2
					- texture_box.width / 2;
			} else {
				texture_box.x = ob_inner_x + ob_inner_width / 2
					- texture_box.width / 2;
			}
		} else {
			texture_box.x = ob_inner_x + ob_inner_width - texture_box.width;
		}
		ob_title_x = texture_box.x;

		texture_box.y =
			round((bg_y - output_y) * output_scale) + ob_padding_above;

		struct wlr_box clip_box = texture_box;
		if (ob_inner_width - ob_marks_width < clip_box.width) {
			clip_box.width = ob_inner_width - ob_marks_width;
		}

		render_texture(ctx, title_texture,
			NULL, &texture_box, &clip_box, WL_OUTPUT_TRANSFORM_NORMAL, deco_data);

		// Padding above
		memcpy(&color, colors->background, sizeof(float) * 4);
		premultiply_alpha(color, con->alpha);
		box.x = clip_box.x + round(output_x * output_scale);
		box.y = roundf((y + titlebar_border_thickness) * output_scale);
		box.width = clip_box.width;
		box.height = ob_padding_above;
		render_rect(ctx, &box, color);

		// Padding below
		box.y += ob_padding_above + clip_box.height;
		box.height = ob_padding_below;
		render_rect(ctx, &box, color);
	}

	// Determine the left + right extends of the textures (output-buffer local)
	int ob_left_x, ob_left_width, ob_right_x, ob_right_width;
	if (ob_title_width == 0 && ob_marks_width == 0) {
		ob_left_x = ob_inner_x;
		ob_left_width = 0;
		ob_right_x = ob_inner_x;
		ob_right_width = 0;
	} else if (ob_title_x < ob_marks_x) {
		ob_left_x = ob_title_x;
		ob_left_width = ob_title_width;
		ob_right_x = ob_marks_x;
		ob_right_width = ob_marks_width;
	} else {
		ob_left_x = ob_marks_x;
		ob_left_width = ob_marks_width;
		ob_right_x = ob_title_x;
		ob_right_width = ob_title_width;
	}
	if (ob_left_x < ob_inner_x) {
		ob_left_x = ob_inner_x;
	} else if (ob_left_x + ob_left_width > ob_right_x + ob_right_width) {
		ob_right_x = ob_left_x;
		ob_right_width = ob_left_width;
	}

	// Filler between title and marks
	box.width = ob_right_x - ob_left_x - ob_left_width;
	if (box.width > 0) {
		box.x = ob_left_x + ob_left_width + round(output_x * output_scale);
		box.y = roundf(bg_y * output_scale);
		box.height = ob_bg_height;
		render_rect(ctx, &box, color);
	}

	// Padding on left side
	box.x = x + titlebar_border_thickness;
	box.y = y + titlebar_border_thickness;
	box.width = titlebar_h_padding - titlebar_border_thickness;
	box.height = (titlebar_v_padding - titlebar_border_thickness) * 2 +
		config->font_height;
	scale_box(&box, output_scale);
	int left_x = ob_left_x + round(output_x * output_scale);
	if (box.x + box.width < left_x) {
		box.width += left_x - box.x - box.width;
	}
	render_rect(ctx, &box, color);

	// Padding on right side
	box.x = x + width - titlebar_h_padding;
	box.y = y + titlebar_border_thickness;
	box.width = titlebar_h_padding - titlebar_border_thickness;
	box.height = (titlebar_v_padding - titlebar_border_thickness) * 2 +
		config->font_height;
	scale_box(&box, output_scale);
	int right_rx = ob_right_x + ob_right_width + round(output_x * output_scale);
	if (right_rx < box.x) {
		box.width += box.x - right_rx;
		box.x = right_rx;
	}
	render_rect(ctx, &box, color);
}

// TODO: rounded corners
/**
 * Render the top border line for a view using "border pixel".
 */
static void render_top_border(struct fx_render_context *ctx, struct sway_container *con,
		struct border_colors *colors) {
	struct sway_container_state *state = &con->current;
	if (!state->border_top) {
		return;
	}
	struct wlr_box box;
	float color[4];
	float output_scale = ctx->output->wlr_output->scale;

	// Child border - top edge
	memcpy(&color, colors->child_border, sizeof(float) * 4);
	premultiply_alpha(color, con->alpha);
	box.x = floor(state->x);
	box.y = floor(state->y);
	box.width = state->width;
	box.height = state->border_thickness;
	scale_box(&box, output_scale);
	render_rect(ctx, &box, color);
}

struct parent_data {
	enum sway_container_layout layout;
	struct wlr_box box;
	list_t *children;
	bool focused;
	struct sway_container *active_child;
};

static void render_container(struct fx_render_context *ctx,
	struct sway_container *con, bool parent_focused);

// TODO: no rounding top corners when rendering with titlebar
/**
 * Render a container's children using a L_HORIZ or L_VERT layout.
 *
 * Wrap child views in borders and leave child containers borderless because
 * they'll apply their own borders to their children.
 */
static void render_containers_linear(struct fx_render_context *ctx, struct parent_data *parent) {
	for (int i = 0; i < parent->children->length; ++i) {
		struct sway_container *child = parent->children->items[i];

		if (container_is_scratchpad_hidden(child)) {
			continue;
		}

		if (child->view) {
			struct sway_view *view = child->view;
			struct border_colors *colors;
			struct wlr_texture *title_texture;
			struct wlr_texture *marks_texture;
			struct sway_container_state *state = &child->current;

			if (view_is_urgent(view)) {
				colors = &config->border_colors.urgent;
				title_texture = child->title_urgent;
				marks_texture = child->marks_urgent;
			} else if (state->focused || parent->focused) {
				colors = &config->border_colors.focused;
				title_texture = child->title_focused;
				marks_texture = child->marks_focused;
			} else if (child == parent->active_child) {
				colors = &config->border_colors.focused_inactive;
				title_texture = child->title_focused_inactive;
				marks_texture = child->marks_focused_inactive;
			} else {
				colors = &config->border_colors.unfocused;
				title_texture = child->title_unfocused;
				marks_texture = child->marks_unfocused;
			}

			bool has_titlebar = state->border == B_NORMAL;

			struct decoration_data deco_data = {
				.alpha = child->alpha,
				.dim_color = view_is_urgent(view)
						? config->dim_inactive_colors.urgent
						: config->dim_inactive_colors.unfocused,
				.dim = child->current.focused || parent->focused ? 0.0f : child->dim,
				// no corner radius if no gaps (allows smart_gaps to work as expected)
				.corner_radius = config->smart_corner_radius &&
						ctx->output->current.active_workspace->current_gaps.top == 0
						? 0 : child->corner_radius,
				.saturation = child->saturation,
				.has_titlebar = has_titlebar,
				.blur = child->blur_enabled,
				.discard_transparent = false,
				.shadow = child->shadow_enabled,
			};
			render_view(ctx, child, colors, deco_data);
			if (has_titlebar) {
				render_titlebar(ctx, child, floor(state->x),
						floor(state->y), state->width, colors,
						deco_data.corner_radius, title_texture, marks_texture);
			} else if (state->border == B_PIXEL) {
				render_top_border(ctx, child, colors);
			}
		} else {
			render_container(ctx, child,
					parent->focused || child->current.focused);
		}
	}
}

static bool container_is_focused(struct sway_container *con, void *data) {
	return con->current.focused;
}

static bool container_has_focused_child(struct sway_container *con) {
	return container_find_child(con, container_is_focused, NULL);
}

/**
 * Render a container's children using the L_TABBED layout.
 */
static void render_containers_tabbed(struct fx_render_context *ctx, struct parent_data *parent) {
	if (!parent->children->length) {
		return;
	}
	struct sway_container *current = parent->active_child;
	struct border_colors *current_colors = &config->border_colors.unfocused;
	int tab_width = parent->box.width / parent->children->length;

	struct decoration_data deco_data = {
		.alpha = current->alpha,
		.dim_color = current->view && view_is_urgent(current->view)
				? config->dim_inactive_colors.urgent
				: config->dim_inactive_colors.unfocused,
		.dim = current->current.focused || parent->focused ? 0.0f : current->dim,
		// no corner radius if no gaps (allows smart_gaps to work as expected)
		.corner_radius = config->smart_corner_radius &&
				ctx->output->current.active_workspace->current_gaps.top == 0
				? 0 : current->corner_radius,
		.saturation = current->saturation,
		.has_titlebar = true,
		.blur = current->blur_enabled,
		.discard_transparent = false,
		.shadow = current->shadow_enabled,
	};

	// Render tabs
	for (int i = 0; i < parent->children->length; ++i) {
		struct sway_container *child = parent->children->items[i];
		struct sway_view *view = child->view;
		struct sway_container_state *cstate = &child->current;
		struct border_colors *colors;
		struct wlr_texture *title_texture;
		struct wlr_texture *marks_texture;
		bool urgent = view ?
			view_is_urgent(view) : container_has_urgent_child(child);

		if (urgent) {
			colors = &config->border_colors.urgent;
			title_texture = child->title_urgent;
			marks_texture = child->marks_urgent;
		} else if (cstate->focused || parent->focused) {
			colors = &config->border_colors.focused;
			title_texture = child->title_focused;
			marks_texture = child->marks_focused;
		} else if (config->has_focused_tab_title && container_has_focused_child(child)) {
			colors = &config->border_colors.focused_tab_title;
			title_texture = child->title_focused_tab_title;
			marks_texture = child->marks_focused_tab_title;
		} else if (child == parent->active_child) {
			colors = &config->border_colors.focused_inactive;
			title_texture = child->title_focused_inactive;
			marks_texture = child->marks_focused_inactive;
		} else {
			colors = &config->border_colors.unfocused;
			title_texture = child->title_unfocused;
			marks_texture = child->marks_unfocused;
		}

		int x = floor(cstate->x + tab_width * i);

		// Make last tab use the remaining width of the parent
		if (i == parent->children->length - 1) {
			tab_width = parent->box.width - tab_width * i;
		}

		/* TODO
		// only round outer corners
		enum corner_location corner_location = NONE;
		if (i == 0) {
			if (i == parent->children->length - 1) {
				corner_location = ALL;
			} else {
				corner_location = TOP_LEFT;
			}
		} else if (i == parent->children->length - 1) {
			corner_location = TOP_RIGHT;
		} */

		render_titlebar(ctx, child, x, parent->box.y, tab_width,
		colors, deco_data.corner_radius, title_texture, marks_texture);

		if (child == current) {
			current_colors = colors;
		}
	}

	// Render surface and left/right/bottom borders
	if (current->view) {
		render_view(ctx, current, current_colors, deco_data);
	} else {
		render_container(ctx, current,
				parent->focused || current->current.focused);
	}
}

/**
 * Render a container's children using the L_STACKED layout.
 */
static void render_containers_stacked(struct fx_render_context *ctx, struct parent_data *parent) {
	if (!parent->children->length) {
		return;
	}
	struct sway_container *current = parent->active_child;
	struct border_colors *current_colors = &config->border_colors.unfocused;
	size_t titlebar_height = container_titlebar_height();

	struct decoration_data deco_data = {
		.alpha = current->alpha,
		.dim_color = current->view && view_is_urgent(current->view)
				? config->dim_inactive_colors.urgent
				: config->dim_inactive_colors.unfocused,
		.dim = current->current.focused || parent->focused ? 0.0f : current->dim,
		.saturation = current->saturation,
		.corner_radius = config->smart_corner_radius &&
				ctx->output->current.active_workspace->current_gaps.top == 0
				? 0 : current->corner_radius,
		.has_titlebar = true,
		.blur = current->blur_enabled,
		.discard_transparent = false,
		.shadow = current->shadow_enabled,
	};

	// Render titles
	for (int i = 0; i < parent->children->length; ++i) {
		struct sway_container *child = parent->children->items[i];
		struct sway_view *view = child->view;
		struct sway_container_state *cstate = &child->current;
		struct border_colors *colors;
		struct wlr_texture *title_texture;
		struct wlr_texture *marks_texture;
		bool urgent = view ?
			view_is_urgent(view) : container_has_urgent_child(child);

		if (urgent) {
			colors = &config->border_colors.urgent;
			title_texture = child->title_urgent;
			marks_texture = child->marks_urgent;
		} else if (cstate->focused || parent->focused) {
			colors = &config->border_colors.focused;
			title_texture = child->title_focused;
			marks_texture = child->marks_focused;
		} else if (config->has_focused_tab_title && container_has_focused_child(child)) {
			colors = &config->border_colors.focused_tab_title;
			title_texture = child->title_focused_tab_title;
			marks_texture = child->marks_focused_tab_title;
		 } else if (child == parent->active_child) {
			colors = &config->border_colors.focused_inactive;
			title_texture = child->title_focused_inactive;
			marks_texture = child->marks_focused_inactive;
		} else {
			colors = &config->border_colors.unfocused;
			title_texture = child->title_unfocused;
			marks_texture = child->marks_unfocused;
		}

		int y = parent->box.y + titlebar_height * i;
		render_titlebar(ctx, child, parent->box.x, y, parent->box.width,
				colors, deco_data.corner_radius, title_texture, marks_texture);

		if (child == current) {
			current_colors = colors;
		}
	}

	// Render surface and left/right/bottom borders
	if (current->view) {
		render_view(ctx, current, current_colors, deco_data);
	} else {
		render_container(ctx, current,
				parent->focused || current->current.focused);
	}
}

static void render_containers(struct fx_render_context *ctx, struct parent_data *parent) {
	if (config->hide_lone_tab && parent->children->length == 1) {
		struct sway_container *child = parent->children->items[0];
		if (child->view) {
			render_containers_linear(ctx, parent);
			return;
		}
	}

	switch (parent->layout) {
	case L_NONE:
	case L_HORIZ:
	case L_VERT:
		render_containers_linear(ctx, parent);
		break;
	case L_STACKED:
		render_containers_stacked(ctx, parent);
		break;
	case L_TABBED:
		render_containers_tabbed(ctx, parent);
		break;
	}
}

static void render_container(struct fx_render_context *ctx,
		struct sway_container *con, bool focused) {
	struct parent_data data = {
		.layout = con->current.layout,
		.box = {
			.x = floor(con->current.x),
			.y = floor(con->current.y),
			.width = con->current.width,
			.height = con->current.height,
		},
		.children = con->current.children,
		.focused = focused,
		.active_child = con->current.focused_inactive_child,
	};
	render_containers(ctx, &data);
}

static void render_workspace(struct fx_render_context *ctx,
		struct sway_workspace *ws, bool focused) {
	struct parent_data data = {
		.layout = ws->current.layout,
		.box = {
			.x = floor(ws->current.x),
			.y = floor(ws->current.y),
			.width = ws->current.width,
			.height = ws->current.height,
		},
		.children = ws->current.tiling,
		.focused = focused,
		.active_child = ws->current.focused_inactive_child,
	};
	render_containers(ctx, &data);
}

static void render_floating_container(struct fx_render_context *ctx,
		struct sway_container *con) {
	struct sway_container_state *state = &con->current;
	if (con->view) {
		struct sway_view *view = con->view;
		struct border_colors *colors;
		struct wlr_texture *title_texture;
		struct wlr_texture *marks_texture;

		if (view_is_urgent(view)) {
			colors = &config->border_colors.urgent;
			title_texture = con->title_urgent;
			marks_texture = con->marks_urgent;
		} else if (state->focused) {
			colors = &config->border_colors.focused;
			title_texture = con->title_focused;
			marks_texture = con->marks_focused;
		} else {
			colors = &config->border_colors.unfocused;
			title_texture = con->title_unfocused;
			marks_texture = con->marks_unfocused;
		}

		bool has_titlebar = state->border == B_NORMAL;
		struct decoration_data deco_data = {
			.alpha = con->alpha,
			.dim_color = view_is_urgent(view)
					? config->dim_inactive_colors.urgent
					: config->dim_inactive_colors.unfocused,
			.dim = con->current.focused ? 0.0f : con->dim,
			.saturation = con->saturation,
			.corner_radius = con->corner_radius,
			.has_titlebar = has_titlebar,
			.blur = con->blur_enabled,
			.discard_transparent = false,
			.shadow = con->shadow_enabled,
		};
		render_view(ctx, con, colors, deco_data);
		if (has_titlebar) {
			render_titlebar(ctx, con, floor(con->current.x), floor(con->current.y),
					con->current.width, colors, deco_data.corner_radius, title_texture, marks_texture);
		} else if (state->border == B_PIXEL) {
			render_top_border(ctx, con, colors);
		}
	} else {
		render_container(ctx, con, state->focused);
	}
}

static void render_floating(struct fx_render_context *ctx) {
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		for (int j = 0; j < output->current.workspaces->length; ++j) {
			struct sway_workspace *ws = output->current.workspaces->items[j];
			if (!workspace_is_visible(ws)) {
				continue;
			}
			for (int k = 0; k < ws->current.floating->length; ++k) {
				struct sway_container *floater = ws->current.floating->items[k];
				if (floater->current.fullscreen_mode != FULLSCREEN_NONE) {
					continue;
				}
				render_floating_container(ctx, floater);
			}
		}
	}
}

static void render_seatops(struct fx_render_context *ctx) {
	struct sway_seat *seat;
	wl_list_for_each(seat, &server.input->seats, link) {
		seatop_render(seat, ctx);
	}
}

void output_render(struct fx_render_context *ctx) {
	struct wlr_output *wlr_output = ctx->output->wlr_output;
	struct sway_output *output = ctx->output;
	const pixman_region32_t *damage = ctx->output_damage;

	struct sway_workspace *workspace = output->current.active_workspace;
	if (workspace == NULL) {
		return;
	}

	struct sway_container *fullscreen_con = root->fullscreen_global;
	if (!fullscreen_con) {
		fullscreen_con = workspace->current.fullscreen;
	}

	if (!pixman_region32_not_empty(damage)) {
		// Output isn't damaged but needs buffer swap
		goto renderer_end;
	}

	if (debug.damage == DAMAGE_HIGHLIGHT) {
		fx_render_pass_add_rect(ctx->pass, &(struct fx_render_rect_options){
			.base = {
				.box = { .width = wlr_output->width, .height = wlr_output->height },
				.color = { .r = 1, .g = 1, .b = 0, .a = 1 },
			},
		});
	}
	pixman_region32_t transformed_damage;
	pixman_region32_init(&transformed_damage);
	pixman_region32_copy(&transformed_damage, damage);
	transform_output_damage(&transformed_damage, wlr_output);

	if (server.session_lock.locked) {
		struct wlr_render_color clear_color = {
			.a = 1.0f
		};
		if (server.session_lock.lock == NULL) {
			// abandoned lock -> red BG
			clear_color.r = 1.f;
		}

		fx_render_pass_add_rect(ctx->pass, &(struct fx_render_rect_options){
			.base = {
				.box = { .width = wlr_output->width, .height = wlr_output->height },
				.color = clear_color,
				.clip = &transformed_damage,
			},
		});

		if (server.session_lock.lock != NULL) {
			struct render_data data = {
				.deco_data = get_undecorated_decoration_data(),
				.ctx = ctx,
			};

			struct wlr_session_lock_surface_v1 *lock_surface;
			wl_list_for_each(lock_surface, &server.session_lock.lock->surfaces, link) {
				if (lock_surface->output != wlr_output) {
					continue;
				}
				if (!lock_surface->surface->mapped) {
					continue;
				}

				output_surface_for_each_surface(output, lock_surface->surface,
					0.0, 0.0, render_surface_iterator, &data);
			}
		}
		goto renderer_end;
	}

	if (output_has_opaque_overlay_layer_surface(output)) {
		goto render_overlay;
	}

	if (fullscreen_con) {
		fx_render_pass_add_rect(ctx->pass, &(struct fx_render_rect_options){
			.base = {
				.box = { .width = wlr_output->width, .height = wlr_output->height },
				.color = { .r = 0, .g = 0, .b = 0, .a = 1 },
				.clip = &transformed_damage,
			},
		});

		if (fullscreen_con->view) {
			struct decoration_data deco_data = get_undecorated_decoration_data();
			deco_data.saturation = fullscreen_con->saturation;
			if (!wl_list_empty(&fullscreen_con->view->saved_buffers)) {
				render_saved_view(ctx, fullscreen_con->view, deco_data);
			} else if (fullscreen_con->view->surface) {
				render_view_toplevels(ctx, fullscreen_con->view, deco_data);
			}
		} else {
			render_container(ctx, fullscreen_con, fullscreen_con->current.focused);
		}

		for (int i = 0; i < workspace->current.floating->length; ++i) {
			struct sway_container *floater =
				workspace->current.floating->items[i];
			if (container_is_transient_for(floater, fullscreen_con)) {
				render_floating_container(ctx, floater);
			}
		}
#if HAVE_XWAYLAND
		render_unmanaged(ctx, &root->xwayland_unmanaged);
#endif
	} else {
		/* TODO
		pixman_region32_t blur_region;
		pixman_region32_init(&blur_region);
		bool workspace_has_blur = workspace_get_blur_info(workspace, &blur_region);
		// Expand the damage to compensate for blur
		if (workspace_has_blur) {
			// Skip the blur artifact prevention if damaging the whole viewport
			if (renderer->blur_buffer_dirty) {
				// Needs to be extended before clearing
				pixman_region32_union_rect(damage, damage,
						0, 0, output_width, output_height);
			} else {
				// copy the surrounding content where the blur would display artifacts
				// and draw it above the artifacts

				// ensure that the damage isn't expanding past the output's size
				int32_t damage_width = damage->extents.x2 - damage->extents.x1;
				int32_t damage_height = damage->extents.y2 - damage->extents.y1;
				if (damage_width > output_width || damage_height > output_height) {
					pixman_region32_intersect_rect(damage, damage,
							0, 0, output_width, output_height);
				} else {
					// Expand the original damage to compensate for surrounding
					// blurred views to avoid sharp edges between damage regions
					wlr_region_expand(damage, damage, config_get_blur_size());
				}

				pixman_region32_t extended_damage;
				pixman_region32_init(&extended_damage);
				pixman_region32_intersect(&extended_damage, damage, &blur_region);
				// Expand the region to compensate for blur artifacts
				wlr_region_expand(&extended_damage, &extended_damage, config_get_blur_size());
				// Limit to the monitors viewport
				pixman_region32_intersect_rect(&extended_damage, &extended_damage,
						0, 0, output_width, output_height);

				// capture the padding pixels around the blur where artifacts will be drawn
				pixman_region32_subtract(&renderer->blur_padding_region,
						&extended_damage, damage);
				// Combine into the surface damage (we need to redraw the padding area as well)
				pixman_region32_union(damage, damage, &extended_damage);
				pixman_region32_fini(&extended_damage);

				// Capture the padding pixels before blur for later use
				fx_framebuffer_bind(&renderer->blur_saved_pixels_buffer);
				// TODO: Investigate blitting instead
				render_whole_output(renderer, wlr_output,
						&renderer->blur_padding_region, &renderer->wlr_buffer.texture);
				fx_framebuffer_bind(&renderer->wlr_buffer);
			}
		}
		pixman_region32_fini(&blur_region);
		*/

		fx_render_pass_add_rect(ctx->pass, &(struct fx_render_rect_options){
			.base = {
				.box = { .width = wlr_output->width, .height = wlr_output->height },
				.color = { .r = 0.25f, .g = 0.25f, .b = 0.25f, .a = 1 },
				.clip = &transformed_damage,
			},
		});

		render_layer_toplevel(ctx,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
		render_layer_toplevel(ctx,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);

		// check if the background needs to be blurred
		/* TODO
		if (workspace_has_blur && renderer->blur_buffer_dirty) {
			render_output_blur(output, damage);
		} */

		render_workspace(ctx, workspace, workspace->current.focused);
		render_floating(ctx);
#if HAVE_XWAYLAND
		render_unmanaged(ctx, &root->xwayland_unmanaged);
#endif
		render_layer_toplevel(ctx,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);

		render_layer_popups(ctx,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
		render_layer_popups(ctx,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);
		render_layer_popups(ctx,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
	}

	render_seatops(ctx);

	struct sway_seat *seat = input_manager_current_seat();
	struct sway_container *focus = seat_get_focused_container(seat);
	if (focus && focus->view) {
		struct decoration_data deco_data = {
			.alpha = focus->alpha,
			.dim_color = view_is_urgent(focus->view)
					? config->dim_inactive_colors.urgent
					: config->dim_inactive_colors.unfocused,
			.dim = focus->current.focused ? 0.0f : focus->dim,
			.corner_radius = 0,
			.saturation = focus->saturation,
			.has_titlebar = false,
			.blur = false,
			.discard_transparent = false,
			.shadow = false,
		};
		render_view_popups(ctx, focus->view, deco_data);
	}

render_overlay:
	render_layer_toplevel(ctx,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);
	render_layer_popups(ctx,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);
	render_drag_icons(ctx, &root->drag_icons);

renderer_end:
	/* TODO
	// Not needed if we damaged the whole viewport
	if (!renderer->blur_buffer_dirty) {
		// Render the saved pixels over the blur artifacts
		// TODO: Investigate blitting instead
		render_whole_output(renderer, wlr_output, &renderer->blur_padding_region,
				&renderer->blur_saved_pixels_buffer.texture);
	} */

	/* TODO
	fx_renderer_end(output->renderer);
	fx_renderer_scissor(NULL);
	*/

	pixman_region32_fini(&transformed_damage);
	wlr_output_add_software_cursors_to_render_pass(wlr_output, &ctx->pass->base, damage);
}
