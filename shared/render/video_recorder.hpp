#pragma once

#include "main.hpp"
#include "CustomTypes/AsyncGPUReadbackPluginRequest.hpp"
#include "queue/readerwriterqueue.h"
#include "AbstractEncoder.hpp"
#include "UnityEngine/Time.hpp"

#include <iostream>
#include <fstream>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include <ctime>
#include "libavutil/opt.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}

enum class Encoder {
// TODO: Android does NOT have MediaCodec encoder
//    MEDIACODEC_GPU,
    LIBX264_YUV_444_CPU,
    LIBX264_RGB_CPU
};

constexpr std::string_view encoderName(Encoder enc) {
    switch (enc) {
//        case Encoder::MEDIACODEC_GPU:
//            return "h264_mediacodec";
        case Encoder::LIBX264_RGB_CPU:
            return "libx264rgb";
        case Encoder::LIBX264_YUV_444_CPU:
            return "libx264";
    }
}

constexpr AVPixelFormat pixelFormat(Encoder enc) {
    // AV_PIX_FMT_RGB24
    switch (enc) {
//        case Encoder::MEDIACODEC_GPU:
//            return AV_PIX_FMT_YUV444P;
        case Encoder::LIBX264_RGB_CPU:
            return AV_PIX_FMT_RGB24;
        case Encoder::LIBX264_YUV_444_CPU:
            return AV_PIX_FMT_YUV444P;
    }
}

// TODO: Rename to FFMPEGVideoEncoder?
class VideoCapture : public Hollywood::AbstractVideoEncoder {
public:
    VideoCapture(uint32_t width, uint32_t height, uint32_t fpsRate,
                 int bitrate, bool stabilizeFPS, std::string_view encodeSpeed,
                 std::string_view filepath,
                 std::string_view encoderStr = encoderName(Encoder::LIBX264_RGB_CPU),
                 AVPixelFormat pxlFormat = pixelFormat(Encoder::LIBX264_RGB_CPU));

    void Init() override;



    void queueFrame(rgb24* queuedFrame, std::optional<float> timeOfFrame) override;

    void Finish();

    int FrameCount()
    {
        return frameCounter;
    };

    bool IsInitialized()
    {
        return initialized;
    };

    float RecordingLength()
    {
        return float(frameCounter) * (1.0f / float(fpsRate));
    };

    float TotalLength() const
    {
        return UnityEngine::Time::get_time() - startTime;
    };


    ~VideoCapture() override;


    size_t approximateFramesToRender() override {
        return framebuffers.size_approx();
    }

private:
    const AVCodec *codec;
    AVCodecContext *c = NULL;
    SwsContext *swsCtx = nullptr;
    AVFrame *frame;
    AVPacket *pkt;
    AVFormatContext *ofctx = nullptr;
    AVOutputFormat *oformat = nullptr;

    Encoder encoder;
    float startTime = 0;

    int frameCounter = 0;
    const uint32_t bitrate;
    const bool stabilizeFPS;
    const std::string encodeSpeed;
    const std::string encoderStr; // encoder for FFMPEG to use
    const std::string filename;
    const AVPixelFormat pxlFormat;
    std::ofstream f;

    using QueueContent = rgb24*;
    moodycamel::ReaderWriterQueue<QueueContent> framebuffers;

//    std::list<rgb24*> framebuffers;
    std::thread encodingThread;
    rgb24* emptyFrame; // constant used to set the frame data to null

    void Encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt, std::ofstream& outfile, int framesToWrite);

    void AddFrame(rgb24 *data);
    void encodeFramesThreadLoop();
};