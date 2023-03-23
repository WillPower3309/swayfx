precision mediump float;
varying vec2 v_texcoord;
uniform sampler2D tex;

void main() {
    vec4 color = texture2D(tex, v_texcoord);
    /* gl_FragColor = vec4(1.0 - color.rgb, color.a); */
    gl_FragColor = vec4(color.rgb, 1.0);
}

