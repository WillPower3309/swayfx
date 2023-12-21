precision         mediump float;
varying vec2      v_texcoord;
uniform sampler2D tex;

uniform float     noise;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec4 pixColor = texture2D(tex, v_texcoord);

    float noiseHash   = hash(v_texcoord);
    float noiseAmount = (mod(noiseHash, 1.0) - 0.5);
    pixColor.rgb += noiseAmount * noise;

    gl_FragColor = pixColor;
}
