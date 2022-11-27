// Writeup: https://madebyevan.com/shaders/fast-rounded-rectangle-shadows/

precision mediump float;
varying vec2 v_texcoord;

uniform vec3 color;
uniform vec2 position;
uniform vec2 size;
uniform float blur_sigma;
uniform float alpha;

// approximates the error function, needed for the gaussian integral
vec4 erf(vec4 x) {
    vec4 s = sign(x), a = abs(x);
    x = 1.0 + (0.278393 + (0.230389 + 0.078108 * (a * a)) * a) * a;
    x *= x;
    return s - s / (x * x);
}

// return the mask for the shadow of a box from lower to upper
float box_shadow(vec2 lower, vec2 upper, vec2 point, float sigma) {
    vec4 query = vec4(point - lower, upper - point);
    vec4 integral = 0.5 + 0.5 * erf(query * (sqrt(0.5) / sigma));
    return (integral.z - integral.x) * (integral.w - integral.y);
}

// per-pixel "random" number between 0 and 1
float random() {
    return fract(sin(dot(vec2(12.9898, 78.233), gl_FragCoord.xy)) * 43758.5453);
}

void main() {
    float frag_alpha = alpha * box_shadow(position, position + size, gl_FragCoord.xy, blur_sigma);

    // dither the alpha to break up color bands
    frag_alpha += (random() - 0.5) / 128.0;

    gl_FragColor = vec4(color, frag_alpha);
}
