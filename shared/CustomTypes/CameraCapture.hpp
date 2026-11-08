#pragma once

#include <media/NdkMediaCodec.h>

#include "UnityEngine/Camera.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/RenderTexture.hpp"
#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(Hollywood, CameraCapture, UnityEngine::MonoBehaviour) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(bool, CanInit);
    DECLARE_INSTANCE_METHOD(bool, CanStart);

    DECLARE_INSTANCE_METHOD(void, Awake);
    DECLARE_INSTANCE_METHOD(bool, Init, int width, int height, int fps, float fov);
    DECLARE_INSTANCE_METHOD(bool, StartEncoder, int bitrate, bool hevc = false);
    DECLARE_INSTANCE_METHOD(bool, StartCapture, void* target);  // must be ANativeWindow*
    DECLARE_INSTANCE_METHOD(bool, StartInternal);
    DECLARE_INSTANCE_METHOD(void, UpdateFPS, int fps);
    DECLARE_INSTANCE_METHOD(void, UpdateFOV, float fov);
    DECLARE_INSTANCE_METHOD(void, Stop);
    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);
    DECLARE_INSTANCE_METHOD(float, GetRenderTime);

    DECLARE_INSTANCE_FIELD(UnityEngine::Camera*, camera);
    DECLARE_INSTANCE_FIELD(UnityEngine::RenderTexture*, texture);

   public:
    std::function<void(uint8_t*, size_t)> onOutputUnit;

   private:
    AMediaCodec* encoder;
    ANativeWindow* window;
    double fpsDelta;
    float startTime;
    float fpsChangeOffset;
    int frames;

    short dataId = -1;

    bool stopThread = false;
    std::thread thread;
};
