#pragma once

#include "CustomTypes/CameraCapture.hpp"
#include "CustomTypes/AudioCapture.hpp"

namespace UnityEngine {
    class Camera;
    class AudioListener;
}

namespace Hollywood {
    struct CameraRecordingSettings;

    void initialize();

    Hollywood::CameraCapture * SetCameraCapture(UnityEngine::Camera* camera, CameraRecordingSettings const& recordingSettings);
    Hollywood::AudioCapture * SetAudioCapture(UnityEngine::AudioListener* listener);
}