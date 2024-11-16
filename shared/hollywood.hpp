#pragma once

#include "CustomTypes/AudioCapture.hpp"
#include "CustomTypes/CameraCapture.hpp"

namespace Hollywood {
    void Init();
    void SetSyncTimes(bool value);
    void SetScreenOn(bool value);
    void SetMuteSpeakers(bool value);
    void MuxFilesSync(std::string_view sourceMp4, std::string_view sourceWav, std::string_view outputMp4);
}
