#pragma once
 
#include "AsyncGPUReadbackPluginRequest.hpp"
#include "../render/AbstractEncoder.hpp"

#include <string>
#include <deque>
 
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/RenderTexture.hpp"
 
#include "System/Collections/Generic/List_1.hpp"
 
#include "custom-types/shared/macros.hpp"
#include "custom-types/shared/coroutine.hpp"
#include "beatsaber-hook/shared/utils/gc-alloc.hpp"

#include <experimental/coroutine>
#include <memory>

namespace Hollywood {
    using RequestList = std::deque<AsyncGPUReadbackPlugin::AsyncGPUReadbackPluginRequest *, gc_allocator<AsyncGPUReadbackPlugin::AsyncGPUReadbackPluginRequest *>>;

// Default recommended settings
    struct CameraRecordingSettings {
        int width = 1920;
        int height = 1080;
        int fps = 60;
        int bitrate = 8000;
        bool movieModeRendering = true;
        float fov = 90;
        std::string filePath = "/sdcard/video.h264";
        std::string encodeSpeed = "faster";
    };
}

DECLARE_CLASS_CODEGEN(Hollywood, CameraCapture, UnityEngine::MonoBehaviour,
public:
    // The texture to read
    DECLARE_INSTANCE_FIELD(UnityEngine::RenderTexture*, readOnlyTexture);

    void Init(CameraRecordingSettings const& settings);

    CameraRecordingSettings const& getRecordingSettings() const {
        return recordingSettings;
    }

    /// Max frames allowed in the render queue
    uint32_t maxFramesAllowedInQueue = 10;
    uint32_t maxRequestsAllowedInQueue = 10;

    /// If true, it is allowed to make requests to render frames
    /// If false, it will not make requests but will continue processing remaining requests
    DECLARE_INSTANCE_FIELD_DEFAULT(bool, makeRequests, true);

    /// If true, will process remaining frames in destructor
    DECLARE_INSTANCE_FIELD_DEFAULT(bool, waitForPendingFrames, false);

    /// Returns amount of requests remaining to be processed
    DECLARE_INSTANCE_METHOD(int, remainingReadRequests);
    /// Returns amount of frames remaining to be rendered
    DECLARE_INSTANCE_METHOD(int, remainingFramesToRender);

private:
    std::unique_ptr<Hollywood::AbstractVideoEncoder> capture;
    int frameRequestCount = 0;
    std::chrono::steady_clock::time_point startTime;

    uint64_t getCurrentFrameId() const;

    CameraRecordingSettings recordingSettings;

    RequestList requests;
 
    DECLARE_CTOR(ctor);
    DECLARE_DTOR(dtor);

    DECLARE_INSTANCE_METHOD(void, MakeRequest, UnityEngine::RenderTexture* target);

    DECLARE_INSTANCE_METHOD(void, SleepFrametime);

    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(bool, HandleFrame, AsyncGPUReadbackPlugin::AsyncGPUReadbackPluginRequest* req);

    DECLARE_INSTANCE_METHOD(void, RequestFrame);

    DECLARE_INSTANCE_METHOD(void, OnPostRender);

    // Return true if done
    DECLARE_INSTANCE_METHOD(UnityEngine::RenderTexture *, GetProperTexture);
//    void OnPostRender();

    // We do this to avoid compile error
    void OnRenderImage(UnityEngine::RenderTexture *source, UnityEngine::RenderTexture *destination);
    // UNCOMMENT LATER
//    DECLARE_INSTANCE_METHOD(void, OnRenderImage, UnityEngine::RenderTexture* source, UnityEngine::RenderTexture* destination);
)