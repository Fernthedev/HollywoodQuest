#include "CustomTypes/CameraCapture.hpp"

#include "UnityEngine/Camera.hpp"
#include "UnityEngine/CameraClearFlags.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/DepthTextureMode.hpp"
#include "UnityEngine/FilterMode.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Graphics.hpp"
#include "UnityEngine/HideFlags.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/StereoTargetEyeMask.hpp"
#include "UnityEngine/TextureWrapMode.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "main.hpp"
#include "render/mediacodec_encoder.hpp"

using namespace custom_types::Helpers;
using namespace Hollywood;
using namespace UnityEngine;

DEFINE_TYPE(Hollywood, CameraCapture);

void CameraCapture::Init(CameraRecordingSettings const& settings) {
    capture = std::make_unique<MediaCodecEncoder>(settings.width, settings.height, settings.fps, settings.bitrate, settings.filePath);

    capture->Init();
    this->recordingSettings = settings;
    startTime = std::chrono::high_resolution_clock::now();
}

int CameraCapture::remainingReadRequests() {
    return requests.size();
}

int CameraCapture::remainingFramesToRender() {
    return capture->approximateFramesToRender();
}

uint64_t CameraCapture::getCurrentFrameId() const {
    // time = 600 ms
    // 60 FPS = 1 / 60 = 16ms
    // frame  = 600 / 16 = 37
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime) /
           std::chrono::milliseconds((uint64_t) std::round(1.0 / capture->getFpsRate() * 1000));
}

void CameraCapture::ctor() {
    INVOKE_CTOR();
    requests = RequestList();
    logger.info("Making video capture");
}

// https://github.com/Alabate/AsyncGPUReadbackPlugin/blob/e8d5e52a9adba24bc0f652c39076404e4671e367/UnityExampleProject/Assets/Scripts/UsePlugin.cs#L13
void CameraCapture::Update() {
    auto it = requests.begin();
    while (it != requests.end()) {
        if (HandleFrame(*it))
            it = requests.erase(it);
        else
            it++;
    }

    if (!(capture->isInitialized() && readOnlyTexture && readOnlyTexture->m_CachedPtr.m_value))
        return;

    if (makeRequests)
        MakeRequest(readOnlyTexture);
}

void CameraCapture::OnDestroy() {
    logger.info("Camera Capture is being destroyed, finishing the capture");

    if (waitForPendingFrames) {
        while (requests.empty()) {
            auto it = requests.begin();
            while (it != requests.end()) {
                if (HandleFrame(*it))
                    it = requests.erase(it);
                else
                    it++;
            }
            SleepFrametime();
        }
    }

    for (auto& req : requests) {
        req->Dispose();
    }
    requests.clear();
    capture.reset();  // force delete of video capture

    if (readOnlyTexture)
        UnityEngine::Object::Destroy(readOnlyTexture);
}

void CameraCapture::MakeRequest(UnityEngine::RenderTexture* target) {
    auto request = AsyncGPUReadbackPlugin::Request(target, this->recordingSettings.width, this->recordingSettings.height);
    request->frameId = getCurrentFrameId();
    requests.push_back(request);
}

void CameraCapture::SleepFrametime() {
    // * 90 because 90%
    std::this_thread::sleep_for(std::chrono::microseconds(uint32_t(1.0f / capture->getFpsRate() * 90)));
}

bool CameraCapture::HandleFrame(AsyncGPUReadbackPlugin::AsyncGPUReadbackPluginRequest* req) {
    bool remove = false;

    if (capture->isInitialized()) {
        req->Update();

        // TODO: Should we lock on waiting for the request to be done? Would that cause the OpenGL thread to freeze?
        // This doesn't freeze OpenGL. Should this still be done though?
        while (!req->HasError() && !req->IsDone() && requests.size() >= maxRequestsAllowedInQueue) {
            req->Update();
            std::this_thread::yield();
            // wait for the next frame or so
            SleepFrametime();
        }

        // This is to avoid having a frame queue so big that you run out of memory.
        if (req->IsDone() && !req->HasError() && maxFramesAllowedInQueue > 0) {
            while (capture->approximateFramesToRender() >= maxFramesAllowedInQueue) {
                std::this_thread::yield();
                SleepFrametime();
            }
        }

        if (req->HasError()) {
            req->Dispose();
            remove = true;
        } else if (req->IsDone()) {
            size_t length;
            rgba* buffer;
            req->GetRawData(buffer, length);

            capture->queueFrame(buffer);

            req->Dispose();
            remove = true;
        }
    } else {
        req->Dispose();
        remove = true;
    }

    return remove;
}
