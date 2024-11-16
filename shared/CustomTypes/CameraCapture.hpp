#pragma once

#include <media/NdkMediaCodec.h>

#include "UnityEngine/Camera.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/RenderTexture.hpp"
#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(Hollywood, CameraCapture, UnityEngine::MonoBehaviour,
    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(void, Init, int width, int height, int fps, int bitrate, float fov);
    DECLARE_INSTANCE_METHOD(void, Stop);
    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, OnPostRender);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);

    DECLARE_INSTANCE_FIELD(UnityEngine::Camera*, camera);
    DECLARE_INSTANCE_FIELD(UnityEngine::RenderTexture*, texture);

   public:
    std::function<void(uint8_t*, size_t)> onOutputUnit;

   private:
    AMediaCodec* encoder;
    ANativeWindow* window;
    float fpsDelta;
    float lastFrameTime;

    int dataId = -1;

    bool stopThread = false;
    std::thread thread;
)
