#pragma once

#include "CustomTypes/CameraCapture.hpp"
#include "CustomTypes/AudioCapture.hpp"

namespace UnityEngine {
    class Camera;
    class AudioListener;
    class AudioSource;
}

namespace Hollywood {
    struct CameraRecordingSettings;

    Hollywood::CameraCapture* SetCameraCapture(UnityEngine::Camera* camera, CameraRecordingSettings const& recordingSettings);
    Hollywood::AudioCapture* SetAudioCapture(UnityEngine::AudioListener* listener);
    Hollywood::AudioCapture* SetAudioCapture(UnityEngine::AudioSource* source);

    Hollywood::CameraCapture * SetCameraCapture(UnityEngine::Camera* camera, CameraRecordingSettings const& recordingSettings);
    Hollywood::AudioCapture * SetAudioCapture(UnityEngine::AudioListener* listener);
}
