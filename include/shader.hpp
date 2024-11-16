#pragma once

#include <GLES3/gl3.h>

namespace Hollywood {
    struct Shader {
        GLuint id = -1;

        Shader() = delete;
        Shader(char const* vertex, char const* fragment);

        bool Use();
        bool Draw(int width, int height, int texture, int vbo);
    };
}
