#define SOURCE_QUAD_ROUND 1
#define SOURCE_QUAD_ROUND_TOP_LEFT 2
#define SOURCE_QUAD_ROUND_TOP_RIGHT 3
#define SOURCE_QUAD_ROUND_BOTTOM_RIGHT 4
#define SOURCE_QUAD_ROUND_BOTTOM_LEFT 5

#if !defined(SOURCE)
#error "Missing shader preamble"
#endif

precision mediump float;
varying vec4 v_color;
varying vec2 v_texcoord;

uniform vec2 size;
uniform vec2 position;
uniform float radius;

vec2 getCornerDist() {
#if SOURCE == SOURCE_QUAD_ROUND
    vec2 half_size = size * 0.5;
    return abs(gl_FragCoord.xy - position - half_size) - half_size + radius;
#elif SOURCE == SOURCE_QUAD_ROUND_TOP_LEFT
    return abs(gl_FragCoord.xy - position - size) - size + radius;
#elif SOURCE == SOURCE_QUAD_ROUND_TOP_RIGHT
    return abs(gl_FragCoord.xy - position - vec2(0, size.y)) - size + radius;
#elif SOURCE == SOURCE_QUAD_ROUND_BOTTOM_RIGHT
    return abs(gl_FragCoord.xy - position) - size + radius;
#elif SOURCE == SOURCE_QUAD_ROUND_BOTTOM_LEFT
    return abs(gl_FragCoord.xy - position - vec2(size.x, 0)) - size + radius;
#endif
}

void main() {
    vec2 q = getCornerDist();
    float dist = min(max(q.x,q.y), 0.0) + length(max(q, 0.0)) - radius;
    float smoothedAlpha = 1.0 - smoothstep(-1.0, 0.5, dist);
    gl_FragColor = mix(vec4(0), v_color, smoothedAlpha);
}
