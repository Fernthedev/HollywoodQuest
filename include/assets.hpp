#pragma once

#include "metacore/shared/assets.hpp"

#define DECLARE_ASSET(name, binary)       \
    const IncludedAsset name {            \
        Externs::_binary_##binary##_start, \
        Externs::_binary_##binary##_end    \
    };

#define DECLARE_ASSET_NS(namespaze, name, binary) \
    namespace namespaze { DECLARE_ASSET(name, binary) }

namespace IncludedAssets {
    namespace Externs {
        extern "C" uint8_t _binary_classes_dex_start[];
        extern "C" uint8_t _binary_classes_dex_end[];
        extern "C" uint8_t _binary_gamma_fs_glsl_start[];
        extern "C" uint8_t _binary_gamma_fs_glsl_end[];
        extern "C" uint8_t _binary_gamma_vs_glsl_start[];
        extern "C" uint8_t _binary_gamma_vs_glsl_end[];
    }

    // classes.dex
    DECLARE_ASSET(classes_dex, classes_dex);
    // gamma_fs.glsl
    DECLARE_ASSET(gamma_fs_glsl, gamma_fs_glsl);
    // gamma_vs.glsl
    DECLARE_ASSET(gamma_vs_glsl, gamma_vs_glsl);
}
