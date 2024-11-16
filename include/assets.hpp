#pragma once

#include <cstdint>

#define DECLARE_FILE(name)                       \
    extern "C" uint8_t _binary_##name##_start[]; \
    extern "C" uint8_t _binary_##name##_end[];   \
    const Asset name {_binary_##name##_start, _binary_##name##_end};

namespace IncludedAssets {
    struct Asset {
        Asset(uint8_t* start, uint8_t* end) : data((char*) start), len(end - start) { *(end - 1) = 0; }
        char const* const data;
        int const len;
    };
    DECLARE_FILE(classes_dex)
    DECLARE_FILE(gamma_fs_glsl)
    DECLARE_FILE(gamma_vs_glsl)
}

#undef DECLARE_FILE
