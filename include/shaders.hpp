#pragma once

#include "opengl_replay/Shader.hpp"
#include "shaders/gamma_convert_vs.glsl.hpp"
#include "shaders/gamma_rgb_convert_fs.glsl.hpp"

static Shader shaderRGBGammaConvert() {
    return {gamma_convert_vs_glsl, gamma_rgb_convert_fs_glsl};
}
