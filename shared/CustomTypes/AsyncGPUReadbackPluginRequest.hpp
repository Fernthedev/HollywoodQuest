#pragma once

#include <cstdint>
#include "UnityEngine/RenderTexture.hpp"
#include "UnityEngine/Texture.hpp"
#include "custom-types/shared/macros.hpp"

#include "memory_pool.hpp"

struct rgba {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

using RGBAFrame = rgba[];
using FramePool = Hollywood::MemoryPool<RGBAFrame>;

DECLARE_CLASS_CODEGEN(AsyncGPUReadbackPlugin, AsyncGPUReadbackPluginRequest, Il2CppObject,
    DECLARE_INSTANCE_FIELD(bool, disposed);
    DECLARE_INSTANCE_FIELD(int, eventId);
    DECLARE_INSTANCE_FIELD(bool, tempTexture);
    DECLARE_INSTANCE_FIELD(UnityEngine::RenderTexture*, texture);

    DECLARE_CTOR(ctor);
    DECLARE_SIMPLE_DTOR();

    DECLARE_INSTANCE_METHOD(bool, IsDone);
    DECLARE_INSTANCE_METHOD(bool, HasError);

    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, Dispose);

   public:
    uint64_t frameId;  // optional associated data
    FramePool::Reference frameReference;  // handle frame ref
    void GetRawData(rgba*& buffer, size_t& length) const;
    ~AsyncGPUReadbackPluginRequest();
)

namespace AsyncGPUReadbackPlugin {
    AsyncGPUReadbackPluginRequest* Request(UnityEngine::RenderTexture* src, int width, int height, FramePool& framePool);

    using GLIssuePluginEvent = function_ptr_t<void, void*, int>;

    GLIssuePluginEvent GetGLIssuePluginEvent();
}
