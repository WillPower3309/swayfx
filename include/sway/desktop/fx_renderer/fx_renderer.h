#ifndef _SWAY_OPENGL_H
#define _SWAY_OPENGL_H

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdbool.h>
#include <wlr/render/egl.h>

#include "sway/desktop/fx_renderer/fx_framebuffer.h"
#include "sway/desktop/fx_renderer/fx_texture.h"

enum corner_location { TOP_LEFT, TOP_RIGHT, BOTTOM_RIGHT, BOTTOM_LEFT, ALL, NONE };

enum fx_tex_shader_source {
	SHADER_SOURCE_TEXTURE_RGBA = 1,
	SHADER_SOURCE_TEXTURE_RGBX = 2,
	SHADER_SOURCE_TEXTURE_EXTERNAL = 3,
};

enum fx_rounded_quad_shader_source {
	SHADER_SOURCE_QUAD_ROUND = 1,
	SHADER_SOURCE_QUAD_ROUND_TOP_LEFT = 2,
	SHADER_SOURCE_QUAD_ROUND_TOP_RIGHT = 3,
	SHADER_SOURCE_QUAD_ROUND_BOTTOM_RIGHT = 4,
	SHADER_SOURCE_QUAD_ROUND_BOTTOM_LEFT = 5,
};

struct decoration_data {
	float alpha;
	float saturation;
	int corner_radius;
	float dim;
	float *dim_color;
	bool has_titlebar;
	bool blur;
	bool shadow;
};

struct blur_shader {
	GLuint program;
	GLint proj;
	GLint tex;
	GLint pos_attrib;
	GLint tex_attrib;
	GLint radius;
	GLint halfpixel;
};

struct box_shadow_shader {
	GLuint program;
	GLint proj;
	GLint color;
	GLint pos_attrib;
	GLint position;
	GLint size;
	GLint blur_sigma;
	GLint corner_radius;
};

struct corner_shader {
	GLuint program;
	GLint proj;
	GLint color;
	GLint pos_attrib;
	GLint is_top_left;
	GLint is_top_right;
	GLint is_bottom_left;
	GLint is_bottom_right;
	GLint position;
	GLint radius;
	GLint half_size;
	GLint half_thickness;
};

struct quad_shader {
	GLuint program;
	GLint proj;
	GLint color;
	GLint pos_attrib;
};

struct rounded_quad_shader {
	GLuint program;
	GLint proj;
	GLint color;
	GLint pos_attrib;
	GLint size;
	GLint position;
	GLint radius;
};

struct stencil_mask_shader {
	GLuint program;
	GLint proj;
	GLint color;
	GLint pos_attrib;
	GLint half_size;
	GLint position;
	GLint radius;
};

struct tex_shader {
	GLuint program;
	GLint proj;
	GLint tex;
	GLint alpha;
	GLint pos_attrib;
	GLint tex_attrib;
	GLint size;
	GLint position;
	GLint radius;
	GLint saturation;
	GLint dim;
	GLint dim_color;
	GLint has_titlebar;
};

struct fx_renderer {
	float projection[9];

	int viewport_width, viewport_height;

	struct wlr_output *wlr_output;

	// The framebuffer used by wlroots
	struct fx_framebuffer wlr_buffer;
	// Contains the blurred background for tiled windows
	struct fx_framebuffer blur_buffer;
	// Contains the original pixels to draw over the areas where artifact are visible
	struct fx_framebuffer blur_saved_pixels_buffer;
	// Blur swaps between the two effects buffers everytime it scales the image
	// Buffer used for effects
	struct fx_framebuffer effects_buffer;
	// Swap buffer used for effects
	struct fx_framebuffer effects_buffer_swapped;

	// The region where there's blur
	pixman_region32_t blur_padding_region;

	bool blur_buffer_dirty;

	struct {
		bool OES_egl_image_external;
	} exts;

	struct {
		PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
	} procs;

	struct {
		struct box_shadow_shader box_shadow;
		struct blur_shader blur1;
		struct blur_shader blur2;
		struct corner_shader corner;
		struct quad_shader quad;
		struct rounded_quad_shader rounded_quad;
		struct rounded_quad_shader rounded_tl_quad;
		struct rounded_quad_shader rounded_tr_quad;
		struct rounded_quad_shader rounded_bl_quad;
		struct rounded_quad_shader rounded_br_quad;
		struct stencil_mask_shader stencil_mask;
		struct tex_shader tex_rgba;
		struct tex_shader tex_rgbx;
		struct tex_shader tex_ext;
	} shaders;
};

struct fx_renderer *fx_renderer_create(struct wlr_egl *egl, struct wlr_output *output);

void fx_renderer_fini(struct fx_renderer *renderer);

void fx_renderer_begin(struct fx_renderer *renderer, int width, int height);

void fx_renderer_end(struct fx_renderer *renderer);

void fx_renderer_clear(const float color[static 4]);

void fx_renderer_scissor(struct wlr_box *box);

bool fx_render_subtexture_with_matrix(struct fx_renderer *renderer, struct fx_texture *fx_texture,
		const struct wlr_fbox *src_box, const struct wlr_box *dst_box, const float matrix[static 9],
		struct decoration_data deco_data);

bool fx_render_texture_with_matrix(struct fx_renderer *renderer, struct fx_texture *fx_texture,
		const struct wlr_box *dst_box, const float matrix[static 9], struct decoration_data deco_data);

void fx_render_rect(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float projection[static 9]);

void fx_render_rounded_rect(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float matrix[static 9], int radius,
		enum corner_location corner_location);

void fx_render_border_corner(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float matrix[static 9],
		enum corner_location corner_location, int radius, int border_thickness);

void fx_render_box_shadow(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float matrix[static 9], int radius,
		float blur_sigma);

struct fx_framebuffer *fx_render_main_buffer_blur(struct fx_renderer *renderer, const float matrix[static 9],
		pixman_region32_t *damage, const struct wlr_box *dst_box, int blur_radius, int blur_passes);
#endif
