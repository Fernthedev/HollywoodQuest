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

struct RGBAFrame {
    RGBAFrame(RGBAFrame const&) = delete;

    RGBAFrame(uint32_t width, uint32_t height) {
        frame = new rgba[width * height]();
    }
    rgba* frame;

    RGBAFrame(RGBAFrame&& o) {
        this->frame = o.frame;
        o.frame = nullptr;
    };

    RGBAFrame& operator =(RGBAFrame&& o) {
        this->frame = o.frame;
        o.frame = nullptr;
        return *this;
    }


    ~RGBAFrame() {
        if (!frame) {
            return;
        }
        
        delete frame;
    }
};

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
