/*
	The original wlr_renderer was heavily referenced in making this project
	https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/render/gles2
*/

#include <assert.h>
#include <GLES2/gl2.h>
#include <stdint.h>
#include <stdlib.h>
#include <wlr/backend.h>
#include <wlr/render/egl.h>
#include <wlr/render/gles2.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/util/box.h>
#include <wlr/util/region.h>

#include "log.h"
#include "pixman.h"
#include "sway/config.h"
#include "sway/desktop/fx_renderer.h"
#include "sway/output.h"
#include "sway/server.h"

// shaders
#include "blur1_frag_src.h"
#include "blur2_frag_src.h"
#include "box_shadow_frag_src.h"
#include "common_vert_src.h"
#include "corner_frag_src.h"
#include "quad_frag_src.h"
#include "quad_round_frag_src.h"
#include "tex_frag_src.h"

static const GLfloat verts[] = {
	1, 0, // top right
	0, 0, // top left
	1, 1, // bottom right
	0, 1, // bottom left
};

static const float transforms[][9] = {
	[WL_OUTPUT_TRANSFORM_NORMAL] = {
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_90] = {
		0.0f, 1.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_180] = {
		-1.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_270] = {
		0.0f, -1.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_FLIPPED] = {
		-1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_FLIPPED_90] = {
		0.0f, 1.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_FLIPPED_180] = {
		1.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
	[WL_OUTPUT_TRANSFORM_FLIPPED_270] = {
		0.0f, -1.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
};

void scissor_output(struct wlr_output *wlr_output,
		pixman_box32_t *rect) {
	struct sway_output *output = wlr_output->data;
	struct fx_renderer *renderer = output->renderer;
	assert(renderer);

	struct wlr_box box = {
		.x = rect->x1,
		.y = rect->y1,
		.width = rect->x2 - rect->x1,
		.height = rect->y2 - rect->y1,
	};

	int ow, oh;
	wlr_output_transformed_resolution(wlr_output, &ow, &oh);

	enum wl_output_transform transform =
		wlr_output_transform_invert(wlr_output->transform);
	wlr_box_transform(&box, &box, transform, ow, oh);

	fx_renderer_scissor(&box);
}

static int get_blur_size() {
	return pow(2, config->blur_params.num_passes) * config->blur_params.radius;
}

int fx_get_container_expanded_size(struct sway_container *con) {
	bool shadow_enabled = config->shadow_enabled;
	if (con) shadow_enabled = con->shadow_enabled;
	int shadow_sigma = shadow_enabled ? config->shadow_blur_sigma : 0;

	bool blur_enabled = config->blur_enabled;
	if (con) blur_enabled = con->blur_enabled;
	int blur_size = blur_enabled ? get_blur_size() : 0;
	// +1 as a margin of error
	return MAX(shadow_sigma, blur_size) + 1;
}

struct fx_texture fx_texture_from_texture(struct wlr_texture* texture) {
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

void fx_framebuffer_bind(struct fx_framebuffer *buffer, GLsizei width, GLsizei height) {
	glBindFramebuffer(GL_FRAMEBUFFER, buffer->fb);
	glViewport(0, 0, width, height);
}

void fx_framebuffer_create(struct wlr_output *output, struct fx_framebuffer *buffer,
		bool bind) {
	bool firstAlloc = false;
	int width, height;
	wlr_output_transformed_resolution(output, &width, &height);
	// Create a new framebuffer
	if (buffer->fb == (uint32_t) -1) {
		glGenFramebuffers(1, &buffer->fb);
		firstAlloc = true;
	}

	if (buffer->texture.id == 0) {
		firstAlloc = true;
		glGenTextures(1, &buffer->texture.id);
		glBindTexture(GL_TEXTURE_2D, buffer->texture.id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}

	if (firstAlloc
			|| buffer->texture.width != width
			|| buffer->texture.height != height) {
		glBindTexture(GL_TEXTURE_2D, buffer->texture.id);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		//
		glBindFramebuffer(GL_FRAMEBUFFER, buffer->fb);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
				buffer->texture.id, 0);
		buffer->texture.target = GL_TEXTURE_2D;
		buffer->texture.has_alpha = false;
		buffer->texture.width = width;
		buffer->texture.height = height;
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			sway_log(SWAY_ERROR, "Framebuffer incomplete, couldn't create! (FB status: %i)", status);
			return;
		}
		sway_log(SWAY_DEBUG, "Framebuffer created, status %i", status);
	}

	// Bind the default framebuffer
	glBindTexture(GL_TEXTURE_2D, 0);
	if (bind) {
		fx_framebuffer_bind(buffer, width, height);
	}
}

void fx_framebuffer_release(struct fx_framebuffer *buffer) {
	if (buffer->fb != (uint32_t) -1 && buffer->fb) {
		glDeleteFramebuffers(1, &buffer->fb);
	}
	buffer->fb= -1;

	if (buffer->texture.id) {
		glDeleteTextures(1, &buffer->texture.id);
	}
	buffer->texture.id = 0;
	buffer->texture.width = -1;
	buffer->texture.height = -1;
}

static void create_stencil_buffer(struct wlr_output* output, GLuint *buffer_id) {
	int width, height;
	wlr_output_transformed_resolution(output, &width, &height);

	if (*buffer_id == (uint32_t) -1) {
		glGenRenderbuffers(1, buffer_id);
		glBindRenderbuffer(GL_RENDERBUFFER, *buffer_id);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, width, height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
				GL_RENDERBUFFER, *buffer_id);
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			sway_log(SWAY_ERROR, "Stencilbuffer incomplete, couldn't create! (FB status: %i)", status);
			return;
		}
		sway_log(SWAY_DEBUG, "Stencilbuffer created, status %i", status);
	}
}

static void release_stencil_buffer(GLuint *buffer_id) {
	if (*buffer_id != (uint32_t)-1 && buffer_id) {
		glDeleteRenderbuffers(1, buffer_id);
	}
	*buffer_id = -1;
}

static void matrix_projection(float mat[static 9], int width, int height,
		enum wl_output_transform transform) {
	memset(mat, 0, sizeof(*mat) * 9);

	const float *t = transforms[transform];
	float x = 2.0f / width;
	float y = 2.0f / height;

	// Rotation + reflection
	mat[0] = x * t[0];
	mat[1] = x * t[1];
	mat[3] = y * -t[3];
	mat[4] = y * -t[4];

	// Translation
	mat[2] = -copysign(1.0f, mat[0] + mat[1]);
	mat[5] = -copysign(1.0f, mat[3] + mat[4]);

	// Identity
	mat[8] = 1.0f;
}

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

static bool link_tex_program(struct fx_renderer *renderer,
		struct gles2_tex_shader *shader, enum fx_tex_shader_source source) {
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

	return true;
}

static bool link_rounded_quad_program(struct fx_renderer *renderer,
		struct rounded_quad_shader *shader, enum fx_rounded_quad_shader_source source) {
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

struct fx_renderer *fx_renderer_create(struct wlr_egl *egl) {
	struct fx_renderer *renderer = calloc(1, sizeof(struct fx_renderer));
	if (renderer == NULL) {
		return NULL;
	}

	// TODO: wlr_egl_make_current or eglMakeCurrent?
	// TODO: assert instead of conditional statement?
	if (!eglMakeCurrent(wlr_egl_get_display(egl), EGL_NO_SURFACE, EGL_NO_SURFACE,
			wlr_egl_get_context(egl))) {
		sway_log(SWAY_ERROR, "GLES2 RENDERER: Could not make EGL current");
		return NULL;
	}
	// TODO: needed?
	renderer->egl = egl;

	renderer->main_buffer.fb = -1;

	renderer->blur_buffer.fb = -1;
	renderer->effects_buffer.fb = -1;
	renderer->effects_buffer_swapped.fb = -1;
	renderer->stencil_buffer_id = -1;

	renderer->blur_buffer_dirty = true;
	renderer->blur_optimize_should_render = false;

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

	// init shaders
	GLuint prog;

	// quad fragment shader
	prog = link_program(quad_frag_src);
	renderer->shaders.quad.program = prog;
	if (!renderer->shaders.quad.program) {
		goto error;
	}
	renderer->shaders.quad.proj = glGetUniformLocation(prog, "proj");
	renderer->shaders.quad.color = glGetUniformLocation(prog, "color");
	renderer->shaders.quad.pos_attrib = glGetAttribLocation(prog, "pos");

	// rounded quad fragment shaders
	if (!link_rounded_quad_program(renderer, &renderer->shaders.rounded_quad,
			SHADER_SOURCE_QUAD_ROUND)) {
		goto error;
	}
	if (!link_rounded_quad_program(renderer, &renderer->shaders.rounded_tl_quad,
			SHADER_SOURCE_QUAD_ROUND_TOP_LEFT)) {
		goto error;
	}
	if (!link_rounded_quad_program(renderer, &renderer->shaders.rounded_tr_quad,
			SHADER_SOURCE_QUAD_ROUND_TOP_RIGHT)) {
		goto error;
	}

	// Border corner shader
	prog = link_program(corner_frag_src);
	renderer->shaders.corner.program = prog;
	if (!renderer->shaders.corner.program) {
		goto error;
	}
	renderer->shaders.corner.proj = glGetUniformLocation(prog, "proj");
	renderer->shaders.corner.color = glGetUniformLocation(prog, "color");
	renderer->shaders.corner.pos_attrib = glGetAttribLocation(prog, "pos");
	renderer->shaders.corner.is_top_left = glGetUniformLocation(prog, "is_top_left");
	renderer->shaders.corner.is_top_right = glGetUniformLocation(prog, "is_top_right");
	renderer->shaders.corner.is_bottom_left = glGetUniformLocation(prog, "is_bottom_left");
	renderer->shaders.corner.is_bottom_right = glGetUniformLocation(prog, "is_bottom_right");
	renderer->shaders.corner.position = glGetUniformLocation(prog, "position");
	renderer->shaders.corner.radius = glGetUniformLocation(prog, "radius");
	renderer->shaders.corner.half_size = glGetUniformLocation(prog, "half_size");
	renderer->shaders.corner.half_thickness = glGetUniformLocation(prog, "half_thickness");

	// box shadow shader
	prog = link_program(box_shadow_frag_src);
	renderer->shaders.box_shadow.program = prog;
	if (!renderer->shaders.box_shadow.program) {
		goto error;
	}
	renderer->shaders.box_shadow.proj = glGetUniformLocation(prog, "proj");
	renderer->shaders.box_shadow.color = glGetUniformLocation(prog, "color");
	renderer->shaders.box_shadow.pos_attrib = glGetAttribLocation(prog, "pos");
	renderer->shaders.box_shadow.position = glGetUniformLocation(prog, "position");
	renderer->shaders.box_shadow.size = glGetUniformLocation(prog, "size");
	renderer->shaders.box_shadow.blur_sigma = glGetUniformLocation(prog, "blur_sigma");
	renderer->shaders.box_shadow.corner_radius = glGetUniformLocation(prog, "corner_radius");

	// Blur 1
	prog = link_program(blur1_frag_src, WLR_GLES2_SHADER_SOURCE_NOT_TEXTURE);
	renderer->shaders.blur1.program = prog;
	if (!renderer->shaders.blur1.program) {
		goto error;
	}
	renderer->shaders.blur1.proj = glGetUniformLocation(prog, "proj");
	renderer->shaders.blur1.tex = glGetUniformLocation(prog, "tex");
	renderer->shaders.blur1.pos_attrib = glGetAttribLocation(prog, "pos");
	renderer->shaders.blur1.tex_attrib = glGetAttribLocation(prog, "texcoord");
	renderer->shaders.blur1.radius = glGetUniformLocation(prog, "radius");
	renderer->shaders.blur1.halfpixel = glGetUniformLocation(prog, "halfpixel");

	// Blur 2
	prog = link_program(blur2_frag_src, WLR_GLES2_SHADER_SOURCE_NOT_TEXTURE);
	renderer->shaders.blur2.program = prog;
	if (!renderer->shaders.blur2.program) {
		goto error;
	}
	renderer->shaders.blur2.proj = glGetUniformLocation(prog, "proj");
	renderer->shaders.blur2.tex = glGetUniformLocation(prog, "tex");
	renderer->shaders.blur2.pos_attrib = glGetAttribLocation(prog, "pos");
	renderer->shaders.blur2.tex_attrib = glGetAttribLocation(prog, "texcoord");
	renderer->shaders.blur2.radius = glGetUniformLocation(prog, "radius");
	renderer->shaders.blur2.halfpixel = glGetUniformLocation(prog, "halfpixel");

	// fragment shaders
	if (!link_tex_program(renderer, &renderer->shaders.tex_rgba,
			SHADER_SOURCE_TEXTURE_RGBA)) {
		goto error;
	}
	if (!link_tex_program(renderer, &renderer->shaders.tex_rgbx,
			SHADER_SOURCE_TEXTURE_RGBX)) {
		goto error;
	}
	if (!link_tex_program(renderer, &renderer->shaders.tex_ext,
			SHADER_SOURCE_TEXTURE_EXTERNAL)) {
		goto error;
	}


	if (!eglMakeCurrent(wlr_egl_get_display(renderer->egl),
				EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
		sway_log(SWAY_ERROR, "GLES2 RENDERER: Could not unset current EGL");
		goto error;
	}

	sway_log(SWAY_INFO, "GLES2 RENDERER: Shaders Initialized Successfully");
	return renderer;

error:
	glDeleteProgram(renderer->shaders.quad.program);
	glDeleteProgram(renderer->shaders.rounded_quad.program);
	glDeleteProgram(renderer->shaders.rounded_tl_quad.program);
	glDeleteProgram(renderer->shaders.rounded_tr_quad.program);
	glDeleteProgram(renderer->shaders.corner.program);
	glDeleteProgram(renderer->shaders.box_shadow.program);
	glDeleteProgram(renderer->shaders.blur1.program);
	glDeleteProgram(renderer->shaders.blur2.program);
	glDeleteProgram(renderer->shaders.tex_rgba.program);
	glDeleteProgram(renderer->shaders.tex_rgbx.program);
	glDeleteProgram(renderer->shaders.tex_ext.program);

	if (!eglMakeCurrent(wlr_egl_get_display(renderer->egl),
				EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
		sway_log(SWAY_ERROR, "GLES2 RENDERER: Could not unset current EGL");
	}

	// TODO: more freeing?
	free(renderer);

	sway_log(SWAY_ERROR, "GLES2 RENDERER: Error Initializing Shaders");
	return NULL;
}

void fx_renderer_fini(struct fx_renderer *renderer) {
	fx_framebuffer_release(&renderer->main_buffer);
	fx_framebuffer_release(&renderer->blur_buffer);
	fx_framebuffer_release(&renderer->effects_buffer);
	fx_framebuffer_release(&renderer->effects_buffer_swapped);
	release_stencil_buffer(&renderer->stencil_buffer_id);
}

void fx_renderer_begin(struct fx_renderer *renderer, struct sway_output *sway_output,
		pixman_region32_t *original_damage) {
	struct wlr_output *output = sway_output->wlr_output;

	int width, height;
	wlr_output_transformed_resolution(output, &width, &height);

	renderer->sway_output = sway_output;
	renderer->original_damage = original_damage;
	// Store the wlr framebuffer
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &renderer->wlr_fb);

	// Create the main framebuffer
	fx_framebuffer_create(output, &renderer->main_buffer, true);
	// Create the stencil buffer and attach it to our main_buffer
	create_stencil_buffer(output, &renderer->stencil_buffer_id);

	// Create a new blur/effects framebuffers
	fx_framebuffer_create(output, &renderer->effects_buffer, false);
	fx_framebuffer_create(output, &renderer->effects_buffer_swapped, false);

	// refresh projection matrix
	matrix_projection(renderer->projection, width, height,
		WL_OUTPUT_TRANSFORM_FLIPPED_180);

	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	// Bind to our main framebuffer
	fx_framebuffer_bind(&renderer->main_buffer, width, height);
}

void fx_renderer_end(struct fx_renderer *renderer) {
	struct wlr_output *output = renderer->sway_output->wlr_output;

	// Draw the contents of our buffer into the wlr buffer
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->wlr_fb);

	float clear_color[] = {0.0f, 0.0f, 0.0f, 1.0f};
	if (pixman_region32_not_empty(renderer->original_damage)) {
		int nrects;
		pixman_box32_t *rects = pixman_region32_rectangles(renderer->original_damage, &nrects);
		for (int i = 0; i < nrects; ++i) {
			scissor_output(output, &rects[i]);
			fx_renderer_clear(clear_color);
		}
	}
	fx_render_whole_output(renderer, renderer->original_damage,
			&renderer->main_buffer.texture);
	fx_renderer_scissor(NULL);

	// Release the main buffer
	fx_framebuffer_release(&renderer->main_buffer);
	release_stencil_buffer(&renderer->stencil_buffer_id);
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

void fx_render_whole_output(struct fx_renderer *renderer, pixman_region32_t *original_damage,
		struct fx_texture *texture) {
	struct wlr_output *output = renderer->sway_output->wlr_output;
	int width, height;
	wlr_output_transformed_resolution(output, &width, &height);
	struct wlr_box monitor_box = {0, 0, width, height};

	float matrix[9];
	enum wl_output_transform transform =
		wlr_output_transform_invert(output->transform);
	wlr_matrix_project_box(matrix, &monitor_box, transform, 0.0,
		output->transform_matrix);

	pixman_region32_t damage;
	pixman_region32_init(&damage);
	pixman_region32_union_rect(&damage, &damage, monitor_box.x, monitor_box.y,
		monitor_box.width, monitor_box.height);
	pixman_region32_intersect(&damage, &damage, original_damage);
	bool damaged = pixman_region32_not_empty(&damage);
	if (!damaged) {
		goto damage_finish;
	}

	struct decoration_data deco_data = {
		.alpha = 1.0f,
		.dim = 0.0f,
		.dim_color = config->dim_inactive_colors.unfocused,
		.corner_radius = 0,
		.saturation = 1.0f,
		.has_titlebar = false,
		.blur = false,
	};

	int nrects;
	pixman_box32_t *rects = pixman_region32_rectangles(&damage, &nrects);
	for (int i = 0; i < nrects; ++i) {
		scissor_output(output, &rects[i]);
		fx_render_texture_with_matrix(renderer, texture, &monitor_box, matrix, deco_data);
	}

damage_finish:
	pixman_region32_fini(&damage);
}

bool fx_render_subtexture_with_matrix(struct fx_renderer *renderer, struct fx_texture *fx_texture,
		const struct wlr_fbox *src_box, const struct wlr_box *dst_box, const float matrix[static 9],
		struct decoration_data deco_data) {

	struct gles2_tex_shader *shader = NULL;

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

	glUseProgram(renderer->shaders.quad.program);

	glUniformMatrix3fv(renderer->shaders.quad.proj, 1, GL_FALSE, gl_matrix);
	glUniform4f(renderer->shaders.quad.color, color[0], color[1], color[2], color[3]);

	glVertexAttribPointer(renderer->shaders.quad.pos_attrib, 2, GL_FLOAT, GL_FALSE,
			0, verts);

	glEnableVertexAttribArray(renderer->shaders.quad.pos_attrib);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(renderer->shaders.quad.pos_attrib);
}

void fx_render_rounded_rect(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float projection[static 9],
		int radius, enum corner_location corner_location) {
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
		default:
			sway_log(SWAY_ERROR, "Invalid Corner Location. Aborting render");
			abort();
	}

	float matrix[9];
	wlr_matrix_project_box(matrix, box, WL_OUTPUT_TRANSFORM_NORMAL, 0, projection);

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
		const float color[static 4], const float projection[static 9],
		enum corner_location corner_location, int radius, int border_thickness) {
	if (border_thickness == 0 || box->width == 0 || box->height == 0) {
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

	if (color[3] == 1.0 && !radius) {
		glDisable(GL_BLEND);
	} else {
		glEnable(GL_BLEND);
	}

	glUseProgram(renderer->shaders.corner.program);

	glUniformMatrix3fv(renderer->shaders.corner.proj, 1, GL_FALSE, gl_matrix);
	glUniform4f(renderer->shaders.corner.color, color[0], color[1], color[2], color[3]);

	glUniform1f(renderer->shaders.corner.is_top_left, corner_location == TOP_LEFT);
	glUniform1f(renderer->shaders.corner.is_top_right, corner_location == TOP_RIGHT);
	glUniform1f(renderer->shaders.corner.is_bottom_left, corner_location == BOTTOM_LEFT);
	glUniform1f(renderer->shaders.corner.is_bottom_right, corner_location == BOTTOM_RIGHT);

	glUniform2f(renderer->shaders.corner.position, box->x, box->y);
	glUniform1f(renderer->shaders.corner.radius, radius);
	glUniform2f(renderer->shaders.corner.half_size, box->width / 2.0, box->height / 2.0);
	glUniform1f(renderer->shaders.corner.half_thickness, border_thickness / 2.0);

	glVertexAttribPointer(renderer->shaders.corner.pos_attrib, 2, GL_FLOAT, GL_FALSE,
			0, verts);

	glEnableVertexAttribArray(renderer->shaders.corner.pos_attrib);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(renderer->shaders.corner.pos_attrib);
}

// TODO: alpha input arg?
void fx_render_box_shadow(struct fx_renderer *renderer, const struct wlr_box *box,
		const float color[static 4], const float projection[static 9],
		int corner_radius, float blur_sigma) {
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

	// Init stencil work
	// NOTE: Alpha needs to be set to 1.0 to be able to discard any "empty" pixels
	const float col[4] = {0.0, 0.0, 0.0, 1.0};
	struct wlr_box inner_box;
	memcpy(&inner_box, box, sizeof(struct wlr_box));
	inner_box.x += blur_sigma;
	inner_box.y += blur_sigma;
	inner_box.width -= blur_sigma * 2;
	inner_box.height -= blur_sigma * 2;

	glEnable(GL_STENCIL_TEST);
	glClearStencil(0);
	glClear(GL_STENCIL_BUFFER_BIT);

	glStencilFunc(GL_ALWAYS, 1, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	// Disable writing to color buffer
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	// Draw the rounded rect as a mask
	fx_render_rounded_rect(renderer, &inner_box, col, projection, corner_radius, ALL);
	// Close the mask
	glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	// Reenable writing to color buffer
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	// blending will practically always be needed (unless we have a madman
	// who uses opaque shadows with zero sigma), so just enable it
	glEnable(GL_BLEND);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(renderer->shaders.box_shadow.program);

	glUniformMatrix3fv(renderer->shaders.box_shadow.proj, 1, GL_FALSE, gl_matrix);
	glUniform4f(renderer->shaders.box_shadow.color, color[0], color[1], color[2], color[3]);
	glUniform1f(renderer->shaders.box_shadow.blur_sigma, blur_sigma);
	glUniform1f(renderer->shaders.box_shadow.corner_radius, corner_radius);

	glUniform2f(renderer->shaders.box_shadow.size, box->width, box->height);
	glUniform2f(renderer->shaders.box_shadow.position, box->x, box->y);

	glVertexAttribPointer(renderer->shaders.box_shadow.pos_attrib, 2, GL_FLOAT, GL_FALSE,
			0, verts);

	glEnableVertexAttribArray(renderer->shaders.box_shadow.pos_attrib);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(renderer->shaders.box_shadow.pos_attrib);

	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glClearStencil(0);
	glClear(GL_STENCIL_BUFFER_BIT);
	glDisable(GL_STENCIL_TEST);
}

static void draw_blur(struct fx_renderer *renderer, struct sway_output *output,
		const float matrix[static 9], pixman_region32_t* damage,
		struct fx_framebuffer **buffer, struct blur_shader* shader,
		const struct wlr_box *box, int blur_radius) {
	int width, height;
	wlr_output_transformed_resolution(output->wlr_output, &width, &height);

	if (*buffer == &renderer->effects_buffer) {
		fx_framebuffer_bind(&renderer->effects_buffer_swapped, width, height);
	} else {
		fx_framebuffer_bind(&renderer->effects_buffer, width, height);
	}

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
		glUniform2f(shader->halfpixel,
				0.5f / (width / 2.0f),
				0.5f / (height / 2.0f));
	} else {
		glUniform2f(shader->halfpixel,
				0.5f / (width * 2.0f),
				0.5f / (height * 2.0f));
	}

	glVertexAttribPointer(shader->pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(shader->tex_attrib, 2, GL_FLOAT, GL_FALSE, 0, verts);

	glEnableVertexAttribArray(shader->pos_attrib);
	glEnableVertexAttribArray(shader->tex_attrib);

	if (pixman_region32_not_empty(damage)) {
		int rectsNum = 0;
		pixman_box32_t *rects = pixman_region32_rectangles(damage, &rectsNum);
		for (int i = 0; i < rectsNum; ++i) {
			const pixman_box32_t box = rects[i];
			struct wlr_box new_box = {box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1};
			fx_renderer_scissor(&new_box);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
	}

	glDisableVertexAttribArray(shader->pos_attrib);
	glDisableVertexAttribArray(shader->tex_attrib);

	if (*buffer != &renderer->effects_buffer) {
		*buffer = &renderer->effects_buffer;
	} else {
		*buffer = &renderer->effects_buffer_swapped;
	}
}

/** Renders the back_buffer blur onto `fx_renderer->blur_buffer` */
struct fx_framebuffer *fx_get_back_buffer_blur(struct fx_renderer *renderer, struct sway_output *output,
		pixman_region32_t *original_damage, const float _matrix[static 9], const float box_matrix[static 9],
		const struct wlr_box *box, struct decoration_data deco_data, struct blur_parameters blur_params) {
	int width, height;
	wlr_output_transformed_resolution(output->wlr_output, &width, &height);

	/* Blur */
	glDisable(GL_BLEND);
	glDisable(GL_STENCIL_TEST);

	const enum wl_output_transform transform = wlr_output_transform_invert(output->wlr_output->transform);
	float matrix[9];
	struct wlr_box monitor_box = { 0, 0, width, height };
	wlr_matrix_project_box(matrix, &monitor_box, transform, 0, output->wlr_output->transform_matrix);

	float gl_matrix[9];
	wlr_matrix_multiply(gl_matrix, renderer->projection, matrix);

	pixman_region32_t damage;
	pixman_region32_init(&damage);
	pixman_region32_copy(&damage, original_damage);
	wlr_region_transform(&damage, &damage, wlr_output_transform_invert(output->wlr_output->transform),
			width, height);
	wlr_region_expand(&damage, &damage, get_blur_size());

	// Initially blur main_buffer content into the effects_buffers
	struct fx_framebuffer *current_buffer = &renderer->main_buffer;

	// Bind to blur framebuffer
	fx_framebuffer_bind(&renderer->effects_buffer, width, height);
	glBindTexture(renderer->main_buffer.texture.target, renderer->main_buffer.texture.id);

	// damage region will be scaled, make a temp
	pixman_region32_t tempDamage;
	pixman_region32_init(&tempDamage);
	// When DOWNscaling, we make the region twice as small because it's the TARGET
	wlr_region_scale(&tempDamage, &damage, 0.5f);

	int blur_radius = blur_params.radius;
	int blur_passes = blur_params.num_passes;

	// First pass
	draw_blur(renderer, output, gl_matrix, &tempDamage, &current_buffer,
			&renderer->shaders.blur1, box, blur_radius);

	// Downscale
	for (int i = 1; i < blur_passes; ++i) {
		wlr_region_scale(&tempDamage, &damage, 1.0f / (1 << (i + 1)));
		draw_blur(renderer, output, gl_matrix, &tempDamage, &current_buffer,
				&renderer->shaders.blur1, box, blur_radius);
	}

	// Upscale
	for (int i = blur_passes - 1; i >= 0; --i) {
		// when upsampling we make the region twice as big
		wlr_region_scale(&tempDamage, &damage, 1.0f / (1 << i));
		draw_blur(renderer, output, gl_matrix, &tempDamage, &current_buffer,
				&renderer->shaders.blur2, box, blur_radius);
	}

	pixman_region32_fini(&tempDamage);
	pixman_region32_fini(&damage);

	// Bind back to the default buffer
	glBindTexture(renderer->effects_buffer.texture.target, 0);

	fx_framebuffer_bind(&renderer->main_buffer, width, height);

	glBindTexture(GL_TEXTURE_2D, 0);

	return current_buffer;
}
