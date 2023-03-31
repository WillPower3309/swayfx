#ifndef _SWAY_OPENGL_H
#define _SWAY_OPENGL_H

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdbool.h>

#include "sway/config.h"
#include "sway/tree/container.h"

enum corner_location { ALL, TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT, NONE };

enum fx_gles2_shader_source {
	WLR_GLES2_SHADER_SOURCE_TEXTURE_RGBA = 1,
	WLR_GLES2_SHADER_SOURCE_TEXTURE_RGBX = 2,
	WLR_GLES2_SHADER_SOURCE_TEXTURE_EXTERNAL = 3,
	WLR_GLES2_SHADER_SOURCE_NOT_TEXTURE = 4,
};

struct decoration_data {
	float alpha;
	float saturation;
	int corner_radius;
	float dim;
	float* dim_color;
	bool has_titlebar;
	bool blur;
};

struct fx_texture {
	GLuint target;
	GLuint id;
	bool has_alpha;
	int width;
	int height;
};

struct fx_framebuffer {
	struct fx_texture texture;
	GLuint fb;
};

struct gles2_tex_shader {
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

struct rounded_quad_shader {
	GLuint program;
	GLint proj;
	GLint color;
	GLint pos_attrib;
	GLint size;
	GLint position;
	GLint radius;
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

struct fx_renderer {
	struct wlr_egl *egl;

	float projection[9];

	pixman_region32_t *original_damage;

	GLint wlr_fb;

	struct sway_output *sway_output;

	GLuint stencil_buffer_id;

	struct fx_framebuffer main_buffer; // The main FB used for rendering
	struct fx_framebuffer blur_buffer; // Contains the blurred background for tiled windows
	struct fx_framebuffer effects_buffer;
	struct fx_framebuffer effects_buffer_swapped;

	bool blur_buffer_dirty;

	struct {
		bool OES_egl_image_external;
	} exts;

	struct {
		PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
	} procs;


	// Shaders
	struct {
		struct {
			GLuint program;
			GLint proj;
			GLint color;
			GLint pos_attrib;
		} quad;

		struct rounded_quad_shader rounded_quad;
		struct rounded_quad_shader rounded_tl_quad;
		struct rounded_quad_shader rounded_tr_quad;

		struct blur_shader blur1;
		struct blur_shader blur2;

		struct {
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
		} corner;

		struct {
			GLuint program;
			GLint proj;
			GLint color;
			GLint pos_attrib;
			GLint position;
			GLint size;
			GLint blur_sigma;
			GLint corner_radius;
		} box_shadow;

		struct gles2_tex_shader tex_rgba;
		struct gles2_tex_shader tex_rgbx;
		struct gles2_tex_shader tex_ext;
	} shaders;
};

void scissor_output(struct wlr_output *wlr_output,
		pixman_box32_t *rect);

int fx_get_container_expanded_size(struct sway_container *con);

struct fx_texture fx_texture_from_texture(struct wlr_texture* tex);

void fx_bind_framebuffer(struct fx_framebuffer *buffer, GLsizei width, GLsizei height);

void fx_create_framebuffer(struct wlr_output *output, struct fx_framebuffer *buffer,
		bool bind);

struct fx_renderer *fx_renderer_create(struct wlr_egl *egl);

void fx_renderer_fini(struct fx_renderer *renderer);

void fx_renderer_begin(struct fx_renderer *renderer, struct sway_output *output,
		pixman_region32_t *original_damage);

void fx_renderer_end(struct fx_renderer *renderer);

void fx_renderer_clear(const float color[static 4]);

void fx_renderer_scissor(struct wlr_box *box);

void fx_render_whole_output(struct fx_renderer *renderer, pixman_region32_t *original_damage,
		struct fx_texture *texture);

bool fx_render_subtexture_with_matrix(struct fx_renderer *renderer, struct fx_texture *fx_texture,
		const struct wlr_fbox *src_box, const struct wlr_box *dst_box, const float matrix[static 9],
		struct decoration_data deco_data);

bool fx_render_texture_with_matrix(struct fx_renderer *renderer, struct fx_texture *fx_texture,
		const struct wlr_box *dst_box, const float matrix[static 9], struct decoration_data deco_data);

void fx_render_rect(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float projection[static 9]);

void fx_render_rounded_rect(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float projection[static 9],
		int radius, enum corner_location corner_location);

void fx_render_border_corner(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float projection[static 9],
		enum corner_location corner_location, int radius, int border_thickness);

void fx_render_box_shadow(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float projection[static 9], int radius, float blur_sigma);

struct fx_framebuffer *fx_get_back_buffer_blur(struct fx_renderer *renderer, struct sway_output *wlr_output,
		pixman_region32_t *damage, const float matrix[static 9], const float box_matrix[static 9],
		const struct wlr_box *box, struct decoration_data deco_data, struct blur_parameters blur_params);

#endif
