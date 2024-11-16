#version 310 es

precision mediump float;

uniform sampler2D inputTex;

layout (location = 0) in vec2 texCoords;

out vec4 out_rgba;

void main() {
    const float gamma = 1.0 / 2.2;
    vec3 color = texture(inputTex, texCoords).rgb;
    color = pow(color, vec3(gamma));
    out_rgba = vec4(color, 1.0f);
}
