/*
	The original wlr_renderer was heavily referenced in making this project
	https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/render/gles2
*/

#include <assert.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <wlr/backend.h>
#include <wlr/render/egl.h>
#include <wlr/render/gles2.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/util/box.h>

#include "log.h"
#include "sway/desktop/fx_renderer/fx_framebuffer.h"
#include "sway/desktop/fx_renderer/fx_renderer.h"
#include "sway/desktop/fx_renderer/fx_stencilbuffer.h"
#include "sway/desktop/fx_renderer/fx_texture.h"
#include "sway/desktop/fx_renderer/matrix.h"
#include "sway/server.h"

// shaders
#include "blur1_frag_src.h"
#include "blur2_frag_src.h"
#include "box_shadow_frag_src.h"
#include "common_vert_src.h"
#include "corner_frag_src.h"
#include "quad_frag_src.h"
#include "quad_round_frag_src.h"
#include "stencil_mask_frag_src.h"
#include "tex_frag_src.h"

static const GLfloat verts[] = {
	1, 0, // top right
	0, 0, // top left
	1, 1, // bottom right
	0, 1, // bottom left
};

static GLuint compile_shader(GLuint type, const GLchar *src) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);

	GLint ok;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (ok == GL_FALSE) {
		sway_log(SWAY_ERROR, "Failed to compile shader");
		glDeleteShader(shader);
		shader = 0;
	}

	return shader;
}

static GLuint link_program(const GLchar *frag_src) {
	const GLchar *vert_src = common_vert_src;
	GLuint vert = compile_shader(GL_VERTEX_SHADER, vert_src);
	if (!vert) {
		goto error;
	}

	GLuint frag = compile_shader(GL_FRAGMENT_SHADER, frag_src);
	if (!frag) {
		glDeleteShader(vert);
		goto error;
	}

	GLuint prog = glCreateProgram();
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram(prog);

	glDetachShader(prog, vert);
	glDetachShader(prog, frag);
	glDeleteShader(vert);
	glDeleteShader(frag);

	GLint ok;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (ok == GL_FALSE) {
		sway_log(SWAY_ERROR, "Failed to link shader");
		glDeleteProgram(prog);
		goto error;
	}

	return prog;

error:
	return 0;
}

static bool link_blur_program(struct blur_shader *shader, const char *shader_program) {
	GLuint prog;
	shader->program = prog = link_program(shader_program);
	if (!shader->program) {
		return false;
	}
	shader->proj = glGetUniformLocation(prog, "proj");
	shader->tex = glGetUniformLocation(prog, "tex");
	shader->pos_attrib = glGetAttribLocation(prog, "pos");
	shader->tex_attrib = glGetAttribLocation(prog, "texcoord");
	shader->radius = glGetUniformLocation(prog, "radius");
	shader->halfpixel = glGetUniformLocation(prog, "halfpixel");

	return true;
}

static bool link_box_shadow_program(struct box_shadow_shader *shader) {
	GLuint prog;
	shader->program = prog = link_program(box_shadow_frag_src);
	if (!shader->program) {
		return false;
	}
	shader->proj = glGetUniformLocation(prog, "proj");
	shader->color = glGetUniformLocation(prog, "color");
	shader->pos_attrib = glGetAttribLocation(prog, "pos");
	shader->position = glGetUniformLocation(prog, "position");
	shader->size = glGetUniformLocation(prog, "size");
	shader->blur_sigma = glGetUniformLocation(prog, "blur_sigma");
	shader->corner_radius = glGetUniformLocation(prog, "corner_radius");

	return true;
}

static bool link_corner_program(struct corner_shader *shader) {
	GLuint prog;
	shader->program = prog = link_program(corner_frag_src);
	if (!shader->program) {
		return false;
	}
	shader->proj = glGetUniformLocation(prog, "proj");
	shader->color = glGetUniformLocation(prog, "color");
	shader->pos_attrib = glGetAttribLocation(prog, "pos");
	shader->position = glGetUniformLocation(prog, "position");
	shader->half_size = glGetUniformLocation(prog, "half_size");
	shader->half_thickness = glGetUniformLocation(prog, "half_thickness");
	shader->radius = glGetUniformLocation(prog, "radius");
	shader->is_top_left = glGetUniformLocation(prog, "is_top_left");
	shader->is_top_right = glGetUniformLocation(prog, "is_top_right");
	shader->is_bottom_left = glGetUniformLocation(prog, "is_bottom_left");
	shader->is_bottom_right = glGetUniformLocation(prog, "is_bottom_right");

	return true;
}

static bool link_quad_program(struct quad_shader *shader) {
	GLuint prog;
	shader->program = prog = link_program(quad_frag_src);
	if (!shader->program) {
		return false;
	}

	shader->proj = glGetUniformLocation(prog, "proj");
	shader->color = glGetUniformLocation(prog, "color");
	shader->pos_attrib = glGetAttribLocation(prog, "pos");

	return true;
}

static bool link_rounded_quad_program(struct rounded_quad_shader *shader,
		enum fx_rounded_quad_shader_source source) {
	GLchar quad_src[2048];
	snprintf(quad_src, sizeof(quad_src),
		"#define SOURCE %d\n%s", source, quad_round_frag_src);

	GLuint prog;
	shader->program = prog = link_program(quad_src);
	if (!shader->program) {
		return false;
	}

	shader->proj = glGetUniformLocation(prog, "proj");
	shader->color = glGetUniformLocation(prog, "color");
	shader->pos_attrib = glGetAttribLocation(prog, "pos");
	shader->size = glGetUniformLocation(prog, "size");
	shader->position = glGetUniformLocation(prog, "position");
	shader->radius = glGetUniformLocation(prog, "radius");

	return true;
}

static bool link_stencil_mask_program(struct stencil_mask_shader *shader) {
	GLuint prog;
	shader->program = prog = link_program(stencil_mask_frag_src);
	if (!shader->program) {
		return false;
	}

	shader->proj = glGetUniformLocation(prog, "proj");
	shader->color = glGetUniformLocation(prog, "color");
	shader->pos_attrib = glGetAttribLocation(prog, "pos");
	shader->position = glGetUniformLocation(prog, "position");
	shader->half_size = glGetUniformLocation(prog, "half_size");
	shader->radius = glGetUniformLocation(prog, "radius");

	return true;
}

static bool link_tex_program(struct tex_shader *shader,
		enum fx_tex_shader_source source) {
	GLchar frag_src[2048];
	snprintf(frag_src, sizeof(frag_src),
		"#define SOURCE %d\n%s", source, tex_frag_src);

	GLuint prog;
	shader->program = prog = link_program(frag_src);
	if (!shader->program) {
		return false;
	}

	shader->proj = glGetUniformLocation(prog, "proj");
	shader->tex = glGetUniformLocation(prog, "tex");
	shader->alpha = glGetUniformLocation(prog, "alpha");
	shader->dim = glGetUniformLocation(prog, "dim");
	shader->dim_color = glGetUniformLocation(prog, "dim_color");
	shader->pos_attrib = glGetAttribLocation(prog, "pos");
	shader->tex_attrib = glGetAttribLocation(prog, "texcoord");
	shader->size = glGetUniformLocation(prog, "size");
	shader->position = glGetUniformLocation(prog, "position");
	shader->radius = glGetUniformLocation(prog, "radius");
	shader->saturation = glGetUniformLocation(prog, "saturation");
	shader->has_titlebar = glGetUniformLocation(prog, "has_titlebar");
	shader->discard_transparent = glGetUniformLocation(prog, "discard_transparent");

	return true;
}

static bool check_gl_ext(const char *exts, const char *ext) {
	size_t extlen = strlen(ext);
	const char *end = exts + strlen(exts);

	while (exts < end) {
		if (exts[0] == ' ') {
			exts++;
			continue;
		}
		size_t n = strcspn(exts, " ");
		if (n == extlen && strncmp(ext, exts, n) == 0) {
			return true;
		}
		exts += n;
	}
	return false;
}

static void load_gl_proc(void *proc_ptr, const char *name) {
	void *proc = (void *)eglGetProcAddress(name);
	if (proc == NULL) {
		sway_log(SWAY_ERROR, "GLES2 RENDERER: eglGetProcAddress(%s) failed", name);
		abort();
	}
	*(void **)proc_ptr = proc;
}

struct fx_renderer *fx_renderer_create(struct wlr_egl *egl, struct wlr_output *wlr_output) {
	struct fx_renderer *renderer = calloc(1, sizeof(struct fx_renderer));
	if (renderer == NULL) {
		return NULL;
	}

	renderer->wlr_output = wlr_output;

	// TODO: wlr_egl_make_current or eglMakeCurrent?
	// TODO: assert instead of conditional statement?
	if (!eglMakeCurrent(wlr_egl_get_display(egl), EGL_NO_SURFACE, EGL_NO_SURFACE,
			wlr_egl_get_context(egl))) {
		sway_log(SWAY_ERROR, "GLES2 RENDERER: Could not make EGL current");
		return NULL;
	}

	renderer->wlr_buffer = fx_framebuffer_create();
	renderer->blur_buffer = fx_framebuffer_create();
	renderer->blur_saved_pixels_buffer = fx_framebuffer_create();
	renderer->effects_buffer = fx_framebuffer_create();
	renderer->effects_buffer_swapped = fx_framebuffer_create();

	renderer->blur_buffer_dirty = true;

	// get extensions
	const char *exts_str = (const char *)glGetString(GL_EXTENSIONS);
	if (exts_str == NULL) {
		sway_log(SWAY_ERROR, "GLES2 RENDERER: Failed to get GL_EXTENSIONS");
		return NULL;
	}

	sway_log(SWAY_INFO, "Creating swayfx GLES2 renderer");
	sway_log(SWAY_INFO, "Using %s", glGetString(GL_VERSION));
	sway_log(SWAY_INFO, "GL vendor: %s", glGetString(GL_VENDOR));
	sway_log(SWAY_INFO, "GL renderer: %s", glGetString(GL_RENDERER));
	sway_log(SWAY_INFO, "Supported GLES2 extensions: %s", exts_str);

	// TODO: the rest of the gl checks
	if (check_gl_ext(exts_str, "GL_OES_EGL_image_external")) {
		renderer->exts.OES_egl_image_external = true;
		load_gl_proc(&renderer->procs.glEGLImageTargetTexture2DOES,
			"glEGLImageTargetTexture2DOES");
	}

	// blur shaders
	if (!link_blur_program(&renderer->shaders.blur1, blur1_frag_src)) {
		goto error;
	}
	if (!link_blur_program(&renderer->shaders.blur2, blur2_frag_src)) {
		goto error;
	}
	// box shadow shader
	if (!link_box_shadow_program(&renderer->shaders.box_shadow)) {
		goto error;
	}
	// corner border shader
	if (!link_corner_program(&renderer->shaders.corner)) {
		goto error;
	}
	// quad fragment shader
	if (!link_quad_program(&renderer->shaders.quad)) {
		goto error;
	}
	// rounded quad fragment shaders
	if (!link_rounded_quad_program(&renderer->shaders.rounded_quad,
			SHADER_SOURCE_QUAD_ROUND)) {
		goto error;
	}
	if (!link_rounded_quad_program(&renderer->shaders.rounded_tl_quad,
			SHADER_SOURCE_QUAD_ROUND_TOP_LEFT)) {
		goto error;
	}
	if (!link_rounded_quad_program(&renderer->shaders.rounded_tr_quad,
			SHADER_SOURCE_QUAD_ROUND_TOP_RIGHT)) {
		goto error;
	}
	if (!link_rounded_quad_program(&renderer->shaders.rounded_bl_quad,
			SHADER_SOURCE_QUAD_ROUND_BOTTOM_LEFT)) {
		goto error;
	}
	if (!link_rounded_quad_program(&renderer->shaders.rounded_br_quad,
			SHADER_SOURCE_QUAD_ROUND_BOTTOM_RIGHT)) {
		goto error;
	}
	// stencil mask shader
	if (!link_stencil_mask_program(&renderer->shaders.stencil_mask)) {
		goto error;
	}
	// fragment shaders
	if (!link_tex_program(&renderer->shaders.tex_rgba, SHADER_SOURCE_TEXTURE_RGBA)) {
		goto error;
	}
	if (!link_tex_program(&renderer->shaders.tex_rgbx, SHADER_SOURCE_TEXTURE_RGBX)) {
		goto error;
	}
	if (!link_tex_program(&renderer->shaders.tex_ext, SHADER_SOURCE_TEXTURE_EXTERNAL)) {
		goto error;
	}

	if (!eglMakeCurrent(wlr_egl_get_display(egl),
				EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
		sway_log(SWAY_ERROR, "GLES2 RENDERER: Could not unset current EGL");
		goto error;
	}

	sway_log(SWAY_INFO, "GLES2 RENDERER: Shaders Initialized Successfully");
	return renderer;

error:
	glDeleteProgram(renderer->shaders.blur1.program);
	glDeleteProgram(renderer->shaders.blur2.program);
	glDeleteProgram(renderer->shaders.box_shadow.program);
	glDeleteProgram(renderer->shaders.corner.program);
	glDeleteProgram(renderer->shaders.quad.program);
	glDeleteProgram(renderer->shaders.rounded_quad.program);
	glDeleteProgram(renderer->shaders.rounded_bl_quad.program);
	glDeleteProgram(renderer->shaders.rounded_br_quad.program);
	glDeleteProgram(renderer->shaders.rounded_tl_quad.program);
	glDeleteProgram(renderer->shaders.rounded_tr_quad.program);
	glDeleteProgram(renderer->shaders.stencil_mask.program);
	glDeleteProgram(renderer->shaders.tex_rgba.program);
	glDeleteProgram(renderer->shaders.tex_rgbx.program);
	glDeleteProgram(renderer->shaders.tex_ext.program);

	if (!eglMakeCurrent(wlr_egl_get_display(egl),
				EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
		sway_log(SWAY_ERROR, "GLES2 RENDERER: Could not unset current EGL");
	}

	// TODO: more freeing?
	free(renderer);

	sway_log(SWAY_ERROR, "GLES2 RENDERER: Error Initializing Shaders");
	return NULL;
}

void fx_renderer_fini(struct fx_renderer *renderer) {
	fx_framebuffer_release(&renderer->blur_buffer);
	fx_framebuffer_release(&renderer->blur_saved_pixels_buffer);
	fx_framebuffer_release(&renderer->effects_buffer);
	fx_framebuffer_release(&renderer->effects_buffer_swapped);
}

void fx_renderer_begin(struct fx_renderer *renderer, int width, int height) {
	glViewport(0, 0, width, height);
	renderer->viewport_width = width;
	renderer->viewport_height = height;

	// Store the wlr FBO
	renderer->wlr_buffer.fb =
		wlr_gles2_renderer_get_current_fbo(renderer->wlr_output->renderer);
	// Get the fx_texture
	struct wlr_texture *wlr_texture = wlr_texture_from_buffer(
			renderer->wlr_output->renderer, renderer->wlr_output->back_buffer);
	renderer->wlr_buffer.texture = fx_texture_from_wlr_texture(wlr_texture);
	wlr_texture_destroy(wlr_texture);
	// Add the stencil to the wlr fbo
	fx_framebuffer_add_stencil_buffer(&renderer->wlr_buffer, width, height);

	// Create the framebuffers
	fx_framebuffer_update(&renderer->blur_saved_pixels_buffer, width, height);
	fx_framebuffer_update(&renderer->effects_buffer, width, height);
	fx_framebuffer_update(&renderer->effects_buffer_swapped, width, height);

	// Add a stencil buffer to the main buffer & bind the main buffer
	fx_framebuffer_bind(&renderer->wlr_buffer);

	pixman_region32_init(&renderer->blur_padding_region);

	// refresh projection matrix
	matrix_projection(renderer->projection, width, height,
		WL_OUTPUT_TRANSFORM_FLIPPED_180);

	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void fx_renderer_end(struct fx_renderer *renderer) {
	pixman_region32_fini(&renderer->blur_padding_region);
}

void fx_renderer_clear(const float color[static 4]) {
	glClearColor(color[0], color[1], color[2], color[3]);
	glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void fx_renderer_scissor(struct wlr_box *box) {
	if (box) {
		glScissor(box->x, box->y, box->width, box->height);
		glEnable(GL_SCISSOR_TEST);
	} else {
		glDisable(GL_SCISSOR_TEST);
	}
}

void fx_renderer_stencil_mask_init() {
	glClearStencil(0);
	glClear(GL_STENCIL_BUFFER_BIT);
	glEnable(GL_STENCIL_TEST);

	glStencilFunc(GL_ALWAYS, 1, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	// Disable writing to color buffer
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
}

void fx_renderer_stencil_mask_close(bool draw_inside_mask) {
	// Reenable writing to color buffer
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	if (draw_inside_mask) {
		glStencilFunc(GL_EQUAL, 1, 0xFF);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		return;
	}
	glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
}

void fx_renderer_stencil_mask_fini() {
	glClearStencil(0);
	glClear(GL_STENCIL_BUFFER_BIT);
	glDisable(GL_STENCIL_TEST);
}

bool fx_render_subtexture_with_matrix(struct fx_renderer *renderer, struct fx_texture *fx_texture,
		const struct wlr_fbox *src_box, const struct wlr_box *dst_box, const float matrix[static 9],
		struct decoration_data deco_data) {

	struct tex_shader *shader = NULL;

	switch (fx_texture->target) {
	case GL_TEXTURE_2D:
		if (fx_texture->has_alpha) {
			shader = &renderer->shaders.tex_rgba;
		} else {
			shader = &renderer->shaders.tex_rgbx;
		}
		break;
	case GL_TEXTURE_EXTERNAL_OES:
		shader = &renderer->shaders.tex_ext;

		if (!renderer->exts.OES_egl_image_external) {
			sway_log(SWAY_ERROR, "Failed to render texture: "
				"GL_TEXTURE_EXTERNAL_OES not supported");
			return false;
		}
		break;
	default:
		sway_log(SWAY_ERROR, "Aborting render");
		abort();
	}

	float gl_matrix[9];
	wlr_matrix_multiply(gl_matrix, renderer->projection, matrix);

	// OpenGL ES 2 requires the glUniformMatrix3fv transpose parameter to be set
	// to GL_FALSE
	wlr_matrix_transpose(gl_matrix, gl_matrix);

	// if there's no opacity or rounded corners we don't need to blend
	if (!fx_texture->has_alpha && deco_data.alpha == 1.0 && !deco_data.corner_radius) {
		glDisable(GL_BLEND);
	} else {
		glEnable(GL_BLEND);
	}

	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(fx_texture->target, fx_texture->id);

	glTexParameteri(fx_texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glUseProgram(shader->program);

	float* dim_color = deco_data.dim_color;

	glUniformMatrix3fv(shader->proj, 1, GL_FALSE, gl_matrix);
	glUniform1i(shader->tex, 0);
	glUniform2f(shader->size, dst_box->width, dst_box->height);
	glUniform2f(shader->position, dst_box->x, dst_box->y);
	glUniform1f(shader->alpha, deco_data.alpha);
	glUniform1f(shader->dim, deco_data.dim);
	glUniform4f(shader->dim_color, dim_color[0], dim_color[1], dim_color[2], dim_color[3]);
	glUniform1f(shader->has_titlebar, deco_data.has_titlebar);
	glUniform1f(shader->discard_transparent, deco_data.discard_transparent);
	glUniform1f(shader->saturation, deco_data.saturation);
	glUniform1f(shader->radius, deco_data.corner_radius);

	const GLfloat x1 = src_box->x / fx_texture->width;
	const GLfloat y1 = src_box->y / fx_texture->height;
	const GLfloat x2 = (src_box->x + src_box->width) / fx_texture->width;
	const GLfloat y2 = (src_box->y + src_box->height) / fx_texture->height;
	const GLfloat texcoord[] = {
		x2, y1, // top right
		x1, y1, // top left
		x2, y2, // bottom right
		x1, y2, // bottom left
	};

	glVertexAttribPointer(shader->pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(shader->tex_attrib, 2, GL_FLOAT, GL_FALSE, 0, texcoord);

	glEnableVertexAttribArray(shader->pos_attrib);
	glEnableVertexAttribArray(shader->tex_attrib);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(shader->pos_attrib);
	glDisableVertexAttribArray(shader->tex_attrib);

	glBindTexture(fx_texture->target, 0);

	return true;
}

bool fx_render_texture_with_matrix(struct fx_renderer *renderer, struct fx_texture *texture,
		const struct wlr_box *dst_box, const float matrix[static 9],
		struct decoration_data deco_data) {
	struct wlr_fbox src_box = {
		.x = 0,
		.y = 0,
		.width = texture->width,
		.height = texture->height,
	};
	return fx_render_subtexture_with_matrix(renderer, texture, &src_box,
			dst_box, matrix, deco_data);
}

void fx_render_rect(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float projection[static 9]) {
	if (box->width == 0 || box->height == 0) {
		return;
	}
	assert(box->width > 0 && box->height > 0);
	float matrix[9];
	wlr_matrix_project_box(matrix, box, WL_OUTPUT_TRANSFORM_NORMAL, 0, projection);

	float gl_matrix[9];
	wlr_matrix_multiply(gl_matrix, renderer->projection, matrix);

	// TODO: investigate why matrix is flipped prior to this cmd
	// wlr_matrix_multiply(gl_matrix, flip_180, gl_matrix);

	wlr_matrix_transpose(gl_matrix, gl_matrix);

	if (color[3] == 1.0) {
		glDisable(GL_BLEND);
	} else {
		glEnable(GL_BLEND);
	}

	struct quad_shader shader = renderer->shaders.quad;
	glUseProgram(shader.program);

	glUniformMatrix3fv(shader.proj, 1, GL_FALSE, gl_matrix);
	glUniform4f(shader.color, color[0], color[1], color[2], color[3]);

	glVertexAttribPointer(shader.pos_attrib, 2, GL_FLOAT, GL_FALSE,
			0, verts);

	glEnableVertexAttribArray(shader.pos_attrib);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(shader.pos_attrib);
}

void fx_render_rounded_rect(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float matrix[static 9], int radius,
		enum corner_location corner_location) {
	if (box->width == 0 || box->height == 0) {
		return;
	}
	assert(box->width > 0 && box->height > 0);

	struct rounded_quad_shader *shader = NULL;

	switch (corner_location) {
		case ALL:
			shader = &renderer->shaders.rounded_quad;
			break;
		case TOP_LEFT:
			shader = &renderer->shaders.rounded_tl_quad;
			break;
		case TOP_RIGHT:
			shader = &renderer->shaders.rounded_tr_quad;
			break;
		case BOTTOM_LEFT:
			shader = &renderer->shaders.rounded_bl_quad;
			break;
		case BOTTOM_RIGHT:
			shader = &renderer->shaders.rounded_br_quad;
			break;
		default:
			sway_log(SWAY_ERROR, "Invalid Corner Location. Aborting render");
			abort();
	}

	float gl_matrix[9];
	wlr_matrix_multiply(gl_matrix, renderer->projection, matrix);

	// TODO: investigate why matrix is flipped prior to this cmd
	// wlr_matrix_multiply(gl_matrix, flip_180, gl_matrix);

	wlr_matrix_transpose(gl_matrix, gl_matrix);

	glEnable(GL_BLEND);

	glUseProgram(shader->program);

	glUniformMatrix3fv(shader->proj, 1, GL_FALSE, gl_matrix);
	glUniform4f(shader->color, color[0], color[1], color[2], color[3]);

	// rounded corners
	glUniform2f(shader->size, box->width, box->height);
	glUniform2f(shader->position, box->x, box->y);
	glUniform1f(shader->radius, radius);

	glVertexAttribPointer(shader->pos_attrib, 2, GL_FLOAT, GL_FALSE,
			0, verts);

	glEnableVertexAttribArray(shader->pos_attrib);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(shader->pos_attrib);
}

void fx_render_border_corner(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float matrix[static 9],
		enum corner_location corner_location, int radius, int border_thickness) {
	if (border_thickness == 0 || box->width == 0 || box->height == 0) {
		return;
	}
	assert(box->width > 0 && box->height > 0);

	float gl_matrix[9];
	wlr_matrix_multiply(gl_matrix, renderer->projection, matrix);

	// TODO: investigate why matrix is flipped prior to this cmd
	// wlr_matrix_multiply(gl_matrix, flip_180, gl_matrix);

	wlr_matrix_transpose(gl_matrix, gl_matrix);

	if (color[3] == 1.0 && !radius) {
		glDisable(GL_BLEND);
	} else {
		glEnable(GL_BLEND);
	}

	struct corner_shader shader = renderer->shaders.corner;

	glUseProgram(shader.program);

	glUniformMatrix3fv(shader.proj, 1, GL_FALSE, gl_matrix);
	glUniform4f(shader.color, color[0], color[1], color[2], color[3]);

	glUniform1f(shader.is_top_left, corner_location == TOP_LEFT);
	glUniform1f(shader.is_top_right, corner_location == TOP_RIGHT);
	glUniform1f(shader.is_bottom_left, corner_location == BOTTOM_LEFT);
	glUniform1f(shader.is_bottom_right, corner_location == BOTTOM_RIGHT);

	glUniform2f(shader.position, box->x, box->y);
	glUniform1f(shader.radius, radius);
	glUniform2f(shader.half_size, box->width / 2.0, box->height / 2.0);
	glUniform1f(shader.half_thickness, border_thickness / 2.0);

	glVertexAttribPointer(shader.pos_attrib, 2, GL_FLOAT, GL_FALSE,
			0, verts);

	glEnableVertexAttribArray(shader.pos_attrib);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(shader.pos_attrib);
}

void fx_render_stencil_mask(struct fx_renderer *renderer, const struct wlr_box *box,
		const float matrix[static 9], int corner_radius) {
	if (box->width == 0 || box->height == 0) {
		return;
	}
	assert(box->width > 0 && box->height > 0);

	// TODO: just pass gl_matrix?
	float gl_matrix[9];
	wlr_matrix_multiply(gl_matrix, renderer->projection, matrix);

	// TODO: investigate why matrix is flipped prior to this cmd
	// wlr_matrix_multiply(gl_matrix, flip_180, gl_matrix);

	wlr_matrix_transpose(gl_matrix, gl_matrix);

	glEnable(GL_BLEND);

	struct stencil_mask_shader shader = renderer->shaders.stencil_mask;

	glUseProgram(shader.program);

	glUniformMatrix3fv(shader.proj, 1, GL_FALSE, gl_matrix);

	glUniform2f(shader.half_size, box->width * 0.5, box->height * 0.5);
	glUniform2f(shader.position, box->x, box->y);
	glUniform1f(shader.radius, corner_radius);

	glVertexAttribPointer(shader.pos_attrib, 2, GL_FLOAT, GL_FALSE,
			0, verts);

	glEnableVertexAttribArray(shader.pos_attrib);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(shader.pos_attrib);
}

// TODO: alpha input arg?
void fx_render_box_shadow(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float matrix[static 9], int corner_radius,
		float blur_sigma) {
	if (box->width == 0 || box->height == 0) {
		return;
	}
	assert(box->width > 0 && box->height > 0);

	float gl_matrix[9];
	wlr_matrix_multiply(gl_matrix, renderer->projection, matrix);

	// TODO: investigate why matrix is flipped prior to this cmd
	// wlr_matrix_multiply(gl_matrix, flip_180, gl_matrix);

	wlr_matrix_transpose(gl_matrix, gl_matrix);

	// Init stencil work
	struct wlr_box inner_box;
	memcpy(&inner_box, box, sizeof(struct wlr_box));
	inner_box.x += blur_sigma;
	inner_box.y += blur_sigma;
	inner_box.width -= blur_sigma * 2;
	inner_box.height -= blur_sigma * 2;

	fx_renderer_stencil_mask_init();
	// Draw the rounded rect as a mask
	fx_render_stencil_mask(renderer, &inner_box, matrix, corner_radius);
	fx_renderer_stencil_mask_close(false);

	// blending will practically always be needed (unless we have a madman
	// who uses opaque shadows with zero sigma), so just enable it
	glEnable(GL_BLEND);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	struct box_shadow_shader shader = renderer->shaders.box_shadow;

	glUseProgram(shader.program);

	glUniformMatrix3fv(shader.proj, 1, GL_FALSE, gl_matrix);
	glUniform4f(shader.color, color[0], color[1], color[2], color[3]);
	glUniform1f(shader.blur_sigma, blur_sigma);
	glUniform1f(shader.corner_radius, corner_radius);

	glUniform2f(shader.size, box->width, box->height);
	glUniform2f(shader.position, box->x, box->y);

	glVertexAttribPointer(shader.pos_attrib, 2, GL_FLOAT, GL_FALSE,
			0, verts);

	glEnableVertexAttribArray(shader.pos_attrib);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(shader.pos_attrib);

	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	fx_renderer_stencil_mask_fini();
}

void fx_render_blur(struct fx_renderer *renderer, const float matrix[static 9],
		struct fx_framebuffer **buffer, struct blur_shader *shader,
		const struct wlr_box *box, int blur_radius) {
	glDisable(GL_BLEND);
	glDisable(GL_STENCIL_TEST);

	glActiveTexture(GL_TEXTURE0);

	glBindTexture((*buffer)->texture.target, (*buffer)->texture.id);

	glTexParameteri((*buffer)->texture.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glUseProgram(shader->program);

	// OpenGL ES 2 requires the glUniformMatrix3fv transpose parameter to be set
	// to GL_FALSE
	float gl_matrix[9];
	wlr_matrix_transpose(gl_matrix, matrix);
	glUniformMatrix3fv(shader->proj, 1, GL_FALSE, gl_matrix);

	glUniform1i(shader->tex, 0);
	glUniform1f(shader->radius, blur_radius);

	if (shader == &renderer->shaders.blur1) {
		glUniform2f(shader->halfpixel, 0.5f / (renderer->viewport_width / 2.0f), 0.5f / (renderer->viewport_height / 2.0f));
	} else {
		glUniform2f(shader->halfpixel, 0.5f / (renderer->viewport_width * 2.0f), 0.5f / (renderer->viewport_height * 2.0f));
	}

	glVertexAttribPointer(shader->pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(shader->tex_attrib, 2, GL_FLOAT, GL_FALSE, 0, verts);

	glEnableVertexAttribArray(shader->pos_attrib);
	glEnableVertexAttribArray(shader->tex_attrib);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(shader->pos_attrib);
	glDisableVertexAttribArray(shader->tex_attrib);

}
