// Stolen from https://learnopengl.com/Getting-started/Shaders

#pragma once

#include <GLES3/gl3.h>  // include to get all the required OpenGL headers

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

class Shader {
   public:
    // the program ID
    unsigned int Shader_ID;

    // constructor reads and builds the shader
    Shader(char const* vertexCode, char const* fragmentCode);

    static Shader fromFile(char const* vextexPath, char const* fragmentPath);
    // use/activate the shader
    void use() const;
    // utility uniform functions
    void setBool(std::string const& name, bool value) const;
    void setInt(std::string const& name, int value) const;
    void setFloat(std::string const& name, float value) const;
};
