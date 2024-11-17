#pragma once

#include "CustomTypes/AudioCapture.hpp"
#include "CustomTypes/CameraCapture.hpp"

namespace Hollywood {
    void Init();
    void SetSyncTimes(bool value);
    void SetScreenOn(bool value);
    void SetMuteSpeakers(bool value);
    void MuxFilesSync(std::string_view video, std::string_view audio, std::string_view outputMp4);
}
