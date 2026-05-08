#pragma once

#include <string_view>
#include <GLES3/gl3.h>

namespace Hollywood {
    struct Shader {
        GLuint id = -1;

        Shader() = delete;
        Shader(std::string_view vertex, std::string_view fragment);

        bool Use();
        bool Draw(int width, int height, int texture, int vbo);
    };
}
