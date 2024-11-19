#pragma once

#include <media/NdkMediaCodec.h>

#include <functional>

namespace Hollywood {
    AMediaCodec* CreateEncoder(int width, int height, int bitrate, int fps = -1, char const* mime = "video/avc");
    void EncodeLoop(AMediaCodec* encoder, std::function<void(uint8_t*, size_t)> onOutputUnit, bool* stop);
}
