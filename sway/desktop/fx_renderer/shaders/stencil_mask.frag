precision mediump float;
varying vec2 v_texcoord;

uniform vec2 half_size;
uniform vec2 position;
uniform float radius;

void main() {
    vec2 q = abs(gl_FragCoord.xy - position - half_size) - half_size + radius;
    float dist = min(max(q.x,q.y), 0.0) + length(max(q, 0.0)) - radius;
    float smoothedAlpha = 1.0 - smoothstep(-1.0, 0.5, dist);
    gl_FragColor = mix(vec4(0.0), vec4(1.0), smoothedAlpha);

    if (gl_FragColor.a < 1.0) {
        discard;
    }
}
