#pragma once

#include <GLES3/gl3.h>

#include <string_view>

namespace Hollywood {
    struct Shader {
        GLuint id = -1;

        Shader() = delete;
        Shader(std::string_view vertex, std::string_view fragment);

        bool Use();
        bool Draw(int width, int height, int texture, int vbo);
    };
}
