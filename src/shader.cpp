#include "shader.hpp"

#include "egl.hpp"
#include "main.hpp"

static bool CheckCompileError(GLuint shader, char const* name) {
    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);

    if (isCompiled == GL_FALSE) {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

        // maxLength includes the NULL character
        std::vector<GLchar> errorLog(maxLength);
        glGetShaderInfoLog(shader, maxLength, &maxLength, errorLog.data());

        glDeleteShader(shader);

        logger.error("Unable to create shader {}:", name);
        logger.error("{}", errorLog.data());
        return true;
    }

    return false;
}

Hollywood::Shader::Shader(char const* vertex, char const* fragment) {
    logger.info("Creating shader program");

    GLuint vid = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(vid, 1, &vertex, 0);
    glCompileShader(vid);
    GL_ERR_CHECK("vertex shader");

    if (CheckCompileError(vid, "vertex"))
        return;

    GLuint fid = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(fid, 1, &fragment, 0);
    glCompileShader(fid);
    GL_ERR_CHECK("fragment shader");

    // make sure shaders are deleted correctly
    if (!CheckCompileError(fid, "fragment")) {
        id = glCreateProgram();

        glAttachShader(id, vid);
        glAttachShader(id, fid);
        glLinkProgram(id);
        GL_ERR_CHECK("link shader");

        // delete the shaders as they're linked into our program now
        glDeleteShader(fid);
    }
    glDeleteShader(vid);
}

bool Hollywood::Shader::Use() {
    if (id == -1)
        return false;

    glUseProgram(id);
    GL_ERR_CHECK("use shader");

    return true;
}

bool Hollywood::Shader::Draw(int width, int height, int texture, int vbo) {
    if (id == -1)
        return false;

    glViewport(0, 0, width, height);
    GL_ERR_CHECK("viewport");
    glActiveTexture(GL_TEXTURE0);
    GL_ERR_CHECK("active tex");
    glBindTexture(GL_TEXTURE_2D, texture);
    GL_ERR_CHECK("bind tex");
    Use();
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    GL_ERR_CHECK("bind draw fbo");
    glBindVertexArray(vbo);
    GL_ERR_CHECK("bind vbo");
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GL_ERR_CHECK("draw");

    return true;
}
