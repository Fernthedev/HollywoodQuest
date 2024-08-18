#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include "UnityEngine/RenderTexture.hpp"
#include "UnityEngine/Texture.hpp"
#include "custom-types/shared/macros.hpp"


struct rgba {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct RGBAFrameData {
    rgba* frameData;

    RGBAFrameData(RGBAFrameData const&) = delete;
    RGBAFrameData(RGBAFrameData&&) = delete;
    RGBAFrameData() = delete;

    RGBAFrameData(uint32_t width, uint32_t height) { frameData = new rgba[width * height](); }

    ~RGBAFrameData() { delete[] frameData; }

    void lock() {
        locked = true;
    }

    void unlock() { locked = false; }

    [[nodiscard]] bool isLocked() const {
        return locked;
    }

   private:
        bool locked = false;

};

/// Allocates frames over time
/// reuses them
struct FramePool {
    FramePool(FramePool const&) = delete;
    FramePool(uint32_t width, uint32_t height, uint32_t initSize = 0) : width(width), height(height), frames(initSize, makeFrame()) {

    }

    using Frame = std::shared_ptr<RGBAFrameData>;

    uint32_t width;
    uint32_t height;
    std::vector<Frame> frames;

    Frame getFrame() {
        for (auto& frame : frames) {
            if (frame->isLocked()) {
                continue;
            }

            frame->lock();
            return frame;
        }

        auto frame = frames.emplace_back(makeFrame());
        frame->lock();
        return frame;
    }

    private:
    std::shared_ptr<RGBAFrameData> makeFrame() {
        return std::make_shared<RGBAFrameData>(width, height);
    } 
};



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
    FramePool::Frame frameReference;  // handle frame ref
    void GetRawData(rgba*& buffer, size_t& length) const;
    ~AsyncGPUReadbackPluginRequest();
)

namespace AsyncGPUReadbackPlugin {
    AsyncGPUReadbackPluginRequest* Request(UnityEngine::RenderTexture* src, int width, int height, FramePool& framePool);

    using GLIssuePluginEvent = function_ptr_t<void, void*, int>;

    GLIssuePluginEvent GetGLIssuePluginEvent();
}