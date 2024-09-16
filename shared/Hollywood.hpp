#pragma once

#include "CustomTypes/AudioCapture.hpp"
#include "CustomTypes/CameraCapture.hpp"

namespace UnityEngine {
    class Camera;
    class AudioListener;
    class AudioSource;
}

namespace Hollywood {
    void SetSyncTimes(bool value);

    Hollywood::CameraCapture* SetCameraCapture(UnityEngine::Camera* camera, CameraRecordingSettings const& recordingSettings);
    Hollywood::AudioCapture* SetAudioCapture(UnityEngine::AudioListener* listener);
    Hollywood::AudioCapture* SetAudioCapture(UnityEngine::AudioSource* source);

    void MuxFilesSync(std::string_view sourceMp4, std::string_view sourceWav, std::string_view outputMp4);
}
