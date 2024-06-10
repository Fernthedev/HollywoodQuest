#include "opengl_replay/Shader.hpp"

#include "main.hpp"

Shader Shader::fromFile(char const* vertexPath, char const* fragmentPath) {
    // 1. retrieve the vertex/fragment source code from filePath
    std::string vertexCode;
    std::string fragmentCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;
    // ensure ifstream objects can throw exceptions:
    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        // open files
        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);
        std::stringstream vShaderStream, fShaderStream;
        // read file's buffer contents into streams
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();
        // close file handlers
        vShaderFile.close();
        fShaderFile.close();
        // convert stream into string
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    } catch (std::ifstream::failure& e) {
        logger.fmtThrowError("ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ {}", e.what());
    }
    return Shader(vertexCode.c_str(), fragmentCode.c_str());
}

void checkCompileErrors(unsigned int shader, char const* name) {
    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE) {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

        // The maxLength includes the NULL character
        std::vector<GLchar> errorLog(maxLength);
        glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);

        // Provide the infolog in whatever manor you deem best.
        // Exit with failure.
        glDeleteShader(shader);  // Don't leak the shader.
        std::string s = std::string(errorLog.begin(), errorLog.end());
        logger.fmtThrowError("Unable to create {} shader: {}", name, s);
    }
}

Shader::Shader(char const* vShaderCode, char const* fShaderCode) {
    // 2. compile shaders
    unsigned int vertex, fragment;
    // vertex shader
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    checkCompileErrors(vertex, "VERTEX");
    // fragment Shader
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    checkCompileErrors(fragment, "FRAGMENT");
    // shader Program
    Shader_ID = glCreateProgram();
    glAttachShader(Shader_ID, vertex);
    glAttachShader(Shader_ID, fragment);
    glLinkProgram(Shader_ID);
    // checkCompileErrors(ID, "PROGRAM");
    // delete the shaders as they're linked into our program now and no longer necessary
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

void Shader::use() const {
    glUseProgram(Shader_ID);
}

void Shader::setBool(std::string const& name, bool value) const {
    glUniform1i(glGetUniformLocation(Shader_ID, name.c_str()), (int) value);
}

void Shader::setInt(std::string const& name, int value) const {
    glUniform1i(glGetUniformLocation(Shader_ID, name.c_str()), value);
}

void Shader::setFloat(std::string const& name, float value) const {
    glUniform1f(glGetUniformLocation(Shader_ID, name.c_str()), value);
}
