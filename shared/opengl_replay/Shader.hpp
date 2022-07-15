// Stolen from https://learnopengl.com/Getting-started/Shaders

#pragma once

#include <GLES3/gl3.h> // include glad to get all the required OpenGL headers

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

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
};

