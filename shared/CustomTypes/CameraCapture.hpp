#pragma once
 
#include "AsyncGPUReadbackPluginRequest.hpp"
#include "render/AbstractEncoder.hpp"

#include <string>
#include <deque>
 
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/RenderTexture.hpp"
 
#include "System/Collections/Generic/List_1.hpp"
 
#include "custom-types/shared/macros.hpp"
#include <experimental/coroutine>
#include "custom-types/shared/coroutine.hpp"

#include "beatsaber-hook/shared/utils/gc-alloc.hpp"

#include <memory>

namespace Hollywood {
    using RequestList = std::deque<AsyncGPUReadbackPlugin::AsyncGPUReadbackPluginRequest *, gc_allocator<AsyncGPUReadbackPlugin::AsyncGPUReadbackPluginRequest *>>;

// Default recommended settings
    struct CameraRecordingSettings {
        int width = 1920;
        int height = 1080;
        int fps = 60;
        int bitrate = 5000;
        bool movieModeRendering = true;
        float fov = 90;
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

    uint32_t maxFramesAllowedInQueue = 10;

private:
    std::unique_ptr<Hollywood::AbstractVideoEncoder> capture;
    int frameRequestCount = 0;

    CameraRecordingSettings recordingSettings;

    RequestList requests;
 
    DECLARE_CTOR(ctor);
    DECLARE_DTOR(dtor);



    DECLARE_INSTANCE_METHOD(void, Update);

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