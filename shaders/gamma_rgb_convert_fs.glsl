#version 310 es

// Shader from xyonico, thank you very much!

precision mediump float;

uniform sampler2D cameraTexture;

layout (location = 0) in vec2 texCoords;

out vec4 out_rgba;

void main()
{
    const float gamma = 1.0 / 2.2;
    vec3 color = texture(cameraTexture, texCoords).rgb;
    color = pow(color, vec3(gamma));

    out_rgba = vec4(color, 1.0f);
}