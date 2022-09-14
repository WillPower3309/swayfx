#ifndef _SWAY_OPENGL_H
#define _SWAY_OPENGL_H

#include <GLES2/gl2.h>

struct border_render_data {
	int thickness;
	float color[4];
};

struct gles2_surface_shader {
	GLuint program;
	GLint proj;
	GLint tex;
	GLint pos_attrib;
	GLint tex_attrib;
	GLint half_size;
	GLint position;
	GLint alpha;
	GLint radius;
	GLint half_border_thickness;
	GLint border_color;
};

struct gles2_tex_shader {
	GLuint program;
	GLint proj;
	GLint tex;
	GLint pos_attrib;
	GLint tex_attrib;
	GLint alpha;
};

struct fx_renderer {
	struct wlr_egl *egl;

	float projection[9];

	// Shaders
	struct {
		struct {
			GLuint program;
			GLint proj;
			GLint color;
			GLint pos_attrib;
		} quad;
		struct gles2_surface_shader surface_rgba;
		struct gles2_surface_shader surface_rgbx;
		struct gles2_surface_shader surface_ext;
		struct gles2_tex_shader tex_rgba;
		struct gles2_tex_shader tex_rgbx;
		struct gles2_tex_shader tex_ext;
	} shaders;
};

struct fx_renderer *fx_renderer_create(struct wlr_egl *egl);

void fx_renderer_begin(struct fx_renderer *renderer, uint32_t width, uint32_t height);

void fx_renderer_end();

void fx_renderer_clear(const float color[static 4]);

void fx_renderer_scissor(struct wlr_box *box);

bool fx_render_surface_with_matrix(struct fx_renderer *renderer, struct wlr_texture *wlr_texture,
		const struct wlr_fbox *src_box, const struct wlr_box *dst_box, const float matrix[static 9],
		float alpha, int radius, struct border_render_data border_data);

bool fx_render_subtexture_with_matrix(struct fx_renderer *renderer, struct wlr_texture *wlr_texture,
		const struct wlr_fbox *src_box, const float matrix[static 9], float alpha);

bool fx_render_texture_with_matrix(struct fx_renderer *renderer, struct wlr_texture *wlr_texture,
		const float matrix[static 9], float alpha);

void fx_render_rect(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float projection[static 9]);

#endif
