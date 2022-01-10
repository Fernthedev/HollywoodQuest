#version 310 es

// Shader from xyonico, thank you very much!

precision mediump float;

uniform sampler2D cameraTexture;

layout (location = 0) in vec2 texCoords;

out vec4 out_rgba;

void main()
{
    // TODO: Fix conversion. Does NOT work
    const float gamma = 1.0 / 2.2;
    vec3 rgb = texture(cameraTexture, texCoords).rgb;
    rgb = pow(rgb, vec3(gamma));


    mat4 RGBtoYUV = mat4(
        0.257,  0.439, -0.148, 0.0,
        0.504, -0.368, -0.291, 0.0,
        0.098, -0.071,  0.439, 0.0,
        0.0625, 0.500,  0.500, 1.0
    );
    vec4 yuv = RGBtoYUV * vec4(rgb, 1.0);

    out_rgba = yuv;

//    vec3 color = texture(cameraTexture, texCoords).rgb;
//
//    out_rgba = vec4(rgba, 1.0);
//    mat4 RGBtoYUV = mat4(
//    0.257,  0.439, -0.148, 0.0,
//    0.504, -0.368, -0.291, 0.0,
//    0.098, -0.071,  0.439, 0.0,
//    0.0625, 0.500,  0.500, 1.0
//    );
//
//    // YUV transform
//    vec3 colorYUV = (RGBtoYUV * vec4(color, 1.0)).rgb;
//
//    out_rgba = vec4(colorYUV, 1.0);

//    vec3 rgba = texture(cameraTexture, texCoords).rgb;

//    vec4 yuva = vec4(0.0);
//    yuva.x = rgba.r * 0.299 + rgba.g * 0.587 + rgba.b * 0.114;
//    yuva.y = rgba.r * -0.169 + rgba.g * -0.331 + rgba.b * 0.5 + 0.5;
//    yuva.z = rgba.r * 0.5 + rgba.g * -0.419 + rgba.b * -0.081 + 0.5;
//    yuva.w = 1.0;
//
//    out_rgba = yuva;
}