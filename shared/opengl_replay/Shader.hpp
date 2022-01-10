// Stolen from https://learnopengl.com/Getting-started/Shaders

#pragma once

#include <GLES3/gl3.h> // include glad to get all the required OpenGL headers

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include "shaders/gamma_convert_vs.glsl.hpp"
#include "shaders/gamma_rgb_convert_fs.glsl.hpp"
#include "shaders/gamma_yuv_convert_fs.glsl.hpp"

class Shader
{
public:
    // the program ID
    unsigned int Shader_ID;

    // constructor reads and builds the shader
    Shader(const char* vertexCode, const char* fragmentCode);

    static Shader fromFile(const char* vextexPath, const char* fragmentPath);
    // use/activate the shader
    void use() const;
    // utility uniform functions
    void setBool(const std::string &name, bool value) const;
    void setInt(const std::string &name, int value) const;
    void setFloat(const std::string &name, float value) const;

    static Shader shaderRGBGammaConvert() {
        return {gamma_convert_vs_glsl, gamma_rgb_convert_fs_glsl};
    }

    static Shader shaderYUVGammaConvert() {
        return {gamma_convert_vs_glsl, gamma_yuv_convert_fs_glsl};
    }
};

