// #pragma once

#include "main.hpp"
#include "../CustomTypes/AsyncGPUReadbackPluginRequest.hpp"
#include "../queue/readerwriterqueue.h"
#include "AbstractEncoder.hpp"
#include "UnityEngine/Time.hpp"

#include "media/NdkMediaCodec.h"
#include "media/NdkMediaFormat.h"
#include "media/NdkMediaMuxer.h"

#include <iostream>
#include <fstream>

class MediaCodecEncoder : public Hollywood::AbstractVideoEncoder {
public:
    MediaCodecEncoder(uint32_t width, uint32_t height, uint32_t fpsRate,
        int bitrate, std::string_view filepath);

    uint32_t threadPoolCount = 2;

    void Init() override;

    void queueFrame(rgb24* queuedFrame, std::optional<uint64_t> timeOfFrame) override;

    void Finish();

    int FrameCount() {
        return frameCounter;
    };

    bool IsInitialized() {
        return initialized;
    };

    float RecordingLength() {
        return float(frameCounter) * (1.0f / float(fpsRate));
    };

    float TotalLength() const {
        return UnityEngine::Time::get_time() - startTime;
    };

    size_t approximateFramesToRender() override {
        return framesProcessing;
    }

    ~MediaCodecEncoder() override;

private:
    float startTime = 0;
    int frameCounter = 0;
    const uint32_t bitrate;
    const std::string filename;
    FILE* f;

    AMediaCodec* encoder;
    AMediaMuxer* muxer;
    bool muxerStarted = false;
    bool isRunning = false;
    int trackIndex = -1;
    int framesProcessing = 0;

    AMediaCodecBufferInfo bufferInfo;

    void drainEncoder(bool endOfStream);
    void releaseEncoder();
    long long computePresentationTimeNsec();
};
