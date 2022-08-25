#ifndef _SWAY_SHADERS_H
#define _SWAY_SHADERS_H

// Colored quads
const GLchar quad_vertex_src[] =
"uniform mat3 proj;\n"
"uniform vec4 color;\n"
"attribute vec2 pos;\n"
"attribute vec2 texcoord;\n"
"varying vec4 v_color;\n"
"varying vec2 v_texcoord;\n"
"\n"
"void main() {\n"
"	gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);\n"
"	v_color = color;\n"
"	v_texcoord = texcoord;\n"
"}\n";

const GLchar quad_fragment_src[] =
"precision mediump float;\n"
"varying vec4 v_color;\n"
"varying vec2 v_texcoord;\n"
"\n"
"void main() {\n"
"	gl_FragColor = v_color;\n"
"}\n";

// Textured quads
const GLchar tex_vertex_src[] =
"uniform mat3 proj;\n"
"attribute vec2 pos;\n"
"attribute vec2 texcoord;\n"
"varying vec2 v_texcoord;\n"
"\n"
"void main() {\n"
"	gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);\n"
"	v_texcoord = texcoord;\n"
"}\n";

const GLchar tex_fragment_src_rgba[] =
"precision mediump float;\n"
"varying vec2 v_texcoord;\n"
"uniform sampler2D tex;\n"
"uniform float alpha;\n"
"\n"
"uniform vec2 topLeft;\n"
"uniform vec2 bottomRight;\n"
"uniform vec2 fullSize;\n"
"uniform float radius;\n"
"\n"
"uniform int discardOpaque;\n"
"\n"
"void main() {\n"
"	vec4 pixColor = texture2D(tex, v_texcoord);\n"
"\n"
"	if (discardOpaque == 1 && pixColor[3] * alpha == 1.0) {\n"
"		discard;\n"
"		return;\n"
"	}\n"
"\n"
"	vec2 pixCoord = fullSize * v_texcoord;\n"
"\n"
"	if (pixCoord[0] < topLeft[0]) {\n"
"		// we're close left\n"
"		if (pixCoord[1] < topLeft[1]) {\n"
			// top
"			if (distance(topLeft, pixCoord) > radius) {\n"
"				discard;\n"
"				return;\n"
"			}\n"
"		} else if (pixCoord[1] > bottomRight[1]) {\n"
			// bottom
"			if (distance(vec2(topLeft[0], bottomRight[1]), pixCoord) > radius) {\n"
"				discard;\n"
"				return;\n"
"			}\n"
"		}\n"
"	}\n"
"	else if (pixCoord[0] > bottomRight[0]) {\n"
		// we're close right
"		if (pixCoord[1] < topLeft[1]) {\n"
			// top
"			if (distance(vec2(bottomRight[0], topLeft[1]), pixCoord) > radius) {\n"
"				discard;\n"
"				return;\n"
"			}\n"
"		} else if (pixCoord[1] > bottomRight[1]) {\n"
			// bottom
"			if (distance(bottomRight, pixCoord) > radius) {\n"
"				discard;\n"
"				return;\n"
"			}\n"
"		}\n"
"	}\n"
"\n"
"	gl_FragColor = pixColor * alpha;\n"
"}\n";

const GLchar tex_fragment_src_rgbx[] =
"precision mediump float;\n"
"varying vec2 v_texcoord;\n"
"uniform sampler2D tex;\n"
"uniform float alpha;\n"
"\n"
"uniform vec2 topLeft;\n"
"uniform vec2 bottomRight;\n"
"uniform vec2 fullSize;\n"
"uniform float radius;\n"
"\n"
"uniform int discardOpaque;\n"
"\n"
"void main() {\n"
"\n"
"	if (discardOpaque == 1 && alpha == 1.0) {\n"
"		discard;\n"
"		return;\n"
"	}\n"
"\n"
"	vec2 pixCoord = fullSize * v_texcoord;\n"
"\n"
"	if (pixCoord[0] < topLeft[0]) {\n"
		// we're close left
"		if (pixCoord[1] < topLeft[1]) {\n"
			// top
"			if (distance(topLeft, pixCoord) > radius) {\n"
"				discard;\n"
"				return;\n"
"			}\n"
"		} else if (pixCoord[1] > bottomRight[1]) {\n"
			// bottom
"			if (distance(vec2(topLeft[0], bottomRight[1]), pixCoord) > radius) {\n"
"				discard;\n"
"				return;\n"
"			}\n"
"		}\n"
"	}\n"
"	else if (pixCoord[0] > bottomRight[0]) {\n"
		// we're close right
"		if (pixCoord[1] < topLeft[1]) {\n"
			// top
"			if (distance(vec2(bottomRight[0], topLeft[1]), pixCoord) > radius) {\n"
"				discard;\n"
"				return;\n"
"			}\n"
"		} else if (pixCoord[1] > bottomRight[1]) {\n"
			// bottom
"			if (distance(bottomRight, pixCoord) > radius) {\n"
"				discard;\n"
"				return;\n"
"			}\n"
"       }\n"
"   }\n"
"\n"
"	gl_FragColor = vec4(texture2D(tex, v_texcoord).rgb, 1.0) * alpha;\n"
"}\n";

const GLchar tex_fragment_src_external[] =
"#extension GL_OES_EGL_image_external : require\n\n"
"precision mediump float;\n"
"varying vec2 v_texcoord;\n"
"uniform samplerExternalOES texture0;\n"
"uniform float alpha;\n"
"\n"
"uniform vec2 topLeft;\n"
"uniform vec2 bottomRight;\n"
"uniform vec2 fullSize;\n"
"uniform float radius;\n"
"\n"
"uniform int discardOpaque;\n"
"\n"
"void main() {\n"
"\n"
"	vec4 pixColor = texture2D(texture0, v_texcoord);\n"
"\n"
"	if (discardOpaque == 1 && pixColor[3] * alpha == 1.0) {\n"
"		discard;\n"
"		return;\n"
"	}\n"
"\n"
"	vec2 pixCoord = fullSize * v_texcoord;\n"
"\n"
"	if (pixCoord[0] < topLeft[0]) {\n"
		// we're close left
"		if (pixCoord[1] < topLeft[1]) {\n"
			// top
"			if (distance(topLeft, pixCoord) > radius) {\n"
"				discard;\n"
"				return;\n"
"			}\n"
"		} else if (pixCoord[1] > bottomRight[1]) {\n"
			// bottom
"			if (distance(vec2(topLeft[0], bottomRight[1]), pixCoord) > radius) {\n"
"				discard;\n"
"				return;\n"
"			}\n"
"		}\n"
"	}\n"
"	else if (pixCoord[0] > bottomRight[0]) {\n"
		// we're close right
"		if (pixCoord[1] < topLeft[1]) {\n"
			// top
"			if (distance(vec2(bottomRight[0], topLeft[1]), pixCoord) > radius) {\n"
"				discard;\n"
"				return;\n"
"			}\n"
"		} else if (pixCoord[1] > bottomRight[1]) {\n"
			// bottom
"			if (distance(bottomRight, pixCoord) > radius) {\n"
"				discard;\n"
"				return;\n"
"			}\n"
"		}\n"
"	}\n"
"\n"
"	gl_FragColor = pixColor * alpha;\n"
"}\n";

const GLchar frag_blur_1[] =
"precision mediump float;\n"
"varying mediump vec2 v_texcoord; // is in 0-1\n"
"uniform sampler2D tex;\n"
"\n"
"uniform float radius;\n"
"uniform vec2 halfpixel;\n"
"\n"
"void main() {\n"
"	vec2 uv = v_texcoord * 2.0;\n"
"\n"
"    vec4 sum = texture2D(tex, uv) * 4.0;\n"
"    sum += texture2D(tex, uv - halfpixel.xy * radius);\n"
"    sum += texture2D(tex, uv + halfpixel.xy * radius);\n"
"    sum += texture2D(tex, uv + vec2(halfpixel.x, -halfpixel.y) * radius);\n"
"    sum += texture2D(tex, uv - vec2(halfpixel.x, -halfpixel.y) * radius);\n"
"    gl_FragColor = sum / 8.0;\n"
"}\n";

const GLchar frag_blur_2[] =
"precision mediump float;\n"
"varying mediump vec2 v_texcoord; // is in 0-1\n"
"uniform sampler2D tex;\n"
"\n"
"uniform float radius;\n"
"uniform vec2 halfpixel;\n"
"\n"
"void main() {\n"
"	vec2 uv = v_texcoord / 2.0;\n"
"\n"
"    vec4 sum = texture2D(tex, uv + vec2(-halfpixel.x * 2.0, 0.0) * radius);\n"
"\n"
"    sum += texture2D(tex, uv + vec2(-halfpixel.x, halfpixel.y) * radius) * 2.0;\n"
"    sum += texture2D(tex, uv + vec2(0.0, halfpixel.y * 2.0) * radius);\n"
"    sum += texture2D(tex, uv + vec2(halfpixel.x, halfpixel.y) * radius) * 2.0;\n"
"    sum += texture2D(tex, uv + vec2(halfpixel.x * 2.0, 0.0) * radius);\n"
"    sum += texture2D(tex, uv + vec2(halfpixel.x, -halfpixel.y) * radius) * 2.0;\n"
"    sum += texture2D(tex, uv + vec2(0.0, -halfpixel.y * 2.0) * radius);\n"
"    sum += texture2D(tex, uv + vec2(-halfpixel.x, -halfpixel.y) * radius) * 2.0;\n"
"\n"
"    gl_FragColor = sum / 12.0;\n"
"}\n";

#endif
<<<<<<< HEAD
=======

>>>>>>> origin/master
