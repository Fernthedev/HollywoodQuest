#include "render/video_recorder.hpp"

#include "modloader/shared/modloader.hpp"

extern "C" {
#include "libavcodec/mediacodec.h"
#include "libavcodec/jni.h"
}

#include "UnityEngine/Time.hpp"

// https://ffmpeg.org/doxygen/trunk/examples.html

#pragma region FFMPEG_Debugging
void* iterate_data = NULL;

const char* Encoder_GetNextCodecName(bool onlyEncoders = true)
{
    AVCodec* current_codec = const_cast<AVCodec *>(av_codec_iterate(&iterate_data));
    while (current_codec != NULL)
    {
        if (onlyEncoders && !av_codec_is_encoder(current_codec))
        {
            current_codec = const_cast<AVCodec *>(av_codec_iterate(&iterate_data));
            continue;
        }
        return current_codec->name;
    }
    return "";
}

const char* Encoder_GetFirstCodecName()
{
    iterate_data = NULL;
    return Encoder_GetNextCodecName();
}

std::vector<std::string> GetCodecs()
{
    std::vector<std::string> l;

    auto s = std::string(Encoder_GetFirstCodecName());
    while (!s.empty())
    {
        l.push_back(s);
        s = std::string(Encoder_GetNextCodecName());
    }

    return l;
}
#pragma endregion

#pragma region Initialization
VideoCapture::VideoCapture(uint32_t width, uint32_t height, uint32_t fpsRate,
                           int bitrate, bool stabilizeFPS, std::string_view encodeSpeed,
                           std::string_view filepath,
                           std::string_view encoderStr,
                           AVPixelFormat pxlFormat)
        : AbstractVideoEncoder(width, height, fpsRate),
          bitrate(bitrate * 1000),
          stabilizeFPS(stabilizeFPS),
          encodeSpeed(encodeSpeed),
          encoderStr(encoderStr),
          filename(filepath),
          pxlFormat(pxlFormat) {
    HLogger.fmtLog<Paper::LogLevel::INF>("Setting up video at path {}", this->filename.c_str());
}

void VideoCapture::Init() {
    frameCounter = 0;
    int ret;

    for (auto& s : GetCodecs()) {
        HLogger.fmtLog<Paper::LogLevel::INF>("codec: {}", s.c_str());
    }

    HLogger.fmtLog<Paper::LogLevel::INF>("Attempting to use {}", std::string(encoderStr).c_str());
    codec = avcodec_find_encoder_by_name(std::string(encoderStr).c_str());

    // codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        HLogger.fmtLog<Paper::LogLevel::INF>("Codec not found");
        return;
    }

    c = avcodec_alloc_context3(codec);
    if (!c)
    {
        HLogger.fmtLog<Paper::LogLevel::INF>("Could not allocate video codec context\n");
        return;
    }

    c->bit_rate = bitrate * 1000;
    c->width = width;
    c->height = height;
    c->time_base = (AVRational){1, (int) fpsRate};
    c->framerate = (AVRational){(int) fpsRate, 1};

    c->gop_size = 10;
    c->max_b_frames = 2;
    c->pix_fmt = pxlFormat;
    // c->pix_fmt = AV_PIX_FMT_YUV420P;

    if (codec->id == AV_CODEC_ID_H264) {
        av_opt_set(c->priv_data, "preset", encodeSpeed.c_str(), 0);
        av_opt_set_int(c->priv_data, "crf", 18, 0);
        // av_opt_set(c->priv_data, "tune", "zerolatency", 0);
    }

    // https://stackoverflow.com/questions/55186822/whats-ffmpeg-doing-with-avcodec-send-packet
    // set codec to automatically determine how many threads suits best for the decoding/encoding job
    c->thread_count = 0;

    if (codec->capabilities | AV_CODEC_CAP_FRAME_THREADS)
        c->thread_type = FF_THREAD_FRAME;
    else if (codec->capabilities | AV_CODEC_CAP_SLICE_THREADS)
        c->thread_type = FF_THREAD_SLICE;
    else
        c->thread_count = 1; //don't use multithreading

    ret = avcodec_open2(c, codec, NULL);
    if (ret < 0)
    {
        HLogger.fmtLog<Paper::LogLevel::INF>("Could not open codec: {}\n", av_err2str(ret));
        return;
    }

    HLogger.fmtLog<Paper::LogLevel::INF>("Successfully opened codec");

    f = std::ofstream(filename);
    if (!f)
    {
        HLogger.fmtLog<Paper::LogLevel::INF>("Could not open {}\n", filename.c_str());
        return;
    }

    frame = av_frame_alloc();
    if (!frame)
    {
        HLogger.fmtLog<Paper::LogLevel::INF>("Could not allocate video frame\n");
        return;
    }
    frame->format = c->pix_fmt;
    frame->width = width;
    frame->height = height;

    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0)
    {
        HLogger.fmtLog<Paper::LogLevel::INF>("Could not allocate the video frame data\n");
        return;
    }

    // swsCtx = sws_getContext(c->width, c->height, AV_PIX_FMT_RGB24, c->width, c->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, 0, 0, 0);

    initialized = true;
    HLogger.fmtLog<Paper::LogLevel::INF>("Finished initializing video at path {}", filename.c_str());


    emptyFrame = new rgb24[width * height];

    encodingThread = std::thread(&VideoCapture::encodeFramesThreadLoop, this);

    for (int i = 0; i < threadPoolCount; i++)
        queueThreads.emplace_back(std::thread(&VideoCapture::enqueueFramesThreadLoop, this));
}

#pragma endregion

#pragma region encode

void VideoCapture::enqueueFramesThreadLoop() {
    HLogger.fmtLog<Paper::LogLevel::INF>("Starting encoding thread");

    moodycamel::ConsumerToken token(framebuffers);

    while (initialized) {
        QueueContent frameData = {};

        // Block instead?
        if (!framebuffers.try_dequeue(token, frameData)) {
            f.flush();
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::microseconds (100));
            continue;
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        this->AddFrame(frameData.first, frameData.second);
        auto currentTime = std::chrono::high_resolution_clock::now();
        int64_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();

        // HLogger.fmtLog<Paper::LogLevel::INF>("Took %lldms to add and encode frame", (long long) duration);

        free(frameData.first);
    }
    HLogger.fmtLog<Paper::LogLevel::INF>("Ending encoding thread");
}

void VideoCapture::encodeFramesThreadLoop() {
    auto pkt = av_packet_alloc();
    if (!pkt) {
        HLogger.fmtLog<Paper::LogLevel::ERR>("UNABLE TO ALLOCATE PACKET IN ENCODE THREAD");
        return;
    }

    while (initialized || remainingFramesCount > 0) {
        if (remainingFramesCount == 0) {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::microseconds (100));
        }

        Encode(c, pkt, f);
    }

    // cleanup
    //DELAYED FRAMES
    SendFrame(c, NULL);
    Encode(c, pkt, f);

    uint8_t endcode[] = { 0, 0, 1, 0xb7 };

    if (codec->id == AV_CODEC_ID_MPEG1VIDEO || codec->id == AV_CODEC_ID_MPEG2VIDEO)
        f.write(reinterpret_cast<char const *>(endcode), sizeof(endcode));


    f.close();

    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);
}

void VideoCapture::Encode(AVCodecContext *enc_ctx, AVPacket *pkt, std::ofstream &outfile) {
    int ret = 0;
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            outfile.flush();
            return;
        }
        else if (ret < 0)
        {
            HLogger.fmtLog<Paper::LogLevel::INF>("Error during encoding\n");
            return;
        }
        outfile.write(reinterpret_cast<const char *>(pkt->data), pkt->size);

        remainingFramesCount--;
        av_packet_unref(pkt);
    }
}

void VideoCapture::SendFrame(AVCodecContext *enc_ctx, AVFrame *frame) {
    int ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0)
    {
        HLogger.fmtLog<Paper::LogLevel::INF>("Error sending a frame for encoding\n");
        return;
    }
    remainingFramesCount++;
}

void VideoCapture::AddFrame(rgb24 *data, std::optional<int64_t> frameTime) {
    if(!initialized) return;

    if(startTime == 0) {
        // TODO: Use system time
        static auto get_time = il2cpp_utils::il2cpp_type_check::FPtrWrapper<&UnityEngine::Time::get_time>::get();
        startTime = get_time();
        HLogger.fmtLog<Paper::LogLevel::INF>("Video global time start is {}", startTime);
    }

    int framesToWrite = 1;

    // if (stabilizeFPS) {
    //     framesToWrite = std::max(0, int(TotalLength() / (1.0f / float(fps))) - frameCounter);
    //     HLogger.fmtLog<Paper::LogLevel::INF>("Frames to write: %i, equation is int(%f / (1 / %i)) - %i", framesToWrite, TotalLength(), fps, frameCounter);
    // }

    if(framesToWrite == 0) return;

    frameCounter += framesToWrite;

    int ret;

    /* make sure the frame data is writable */
    ret = av_frame_make_writable(frame);
    if (ret < 0)
    {
        HLogger.fmtLog<Paper::LogLevel::INF>("Could not make the frame writable: {}", av_err2str(ret));
        return;
    }

    // int inLinesize[1] = {3 * c->width};
    // sws_scale(swsCtx, (const uint8_t *const *)&data, inLinesize, 0, c->height, frame->data, frame->linesize);

    frame->data[0] = (uint8_t*) data;
    if (stabilizeFPS) {
        frame->pts = TotalLength();
    } else {
        frame->pts = frameTime.value_or((int) ((1.0f / (float) fpsRate) * (float) frameCounter));
    }
    /* encode the image */
    SendFrame(c, frame);


    frame->data[0] = reinterpret_cast<uint8_t *>(emptyFrame);
//  iterating slow?  for(auto & i : frame->data) i = nullptr;
}

#pragma endregion





void VideoCapture::queueFrame(rgb24* queuedFrame, std::optional<uint64_t> timeOfFrame) {
    if(!initialized)
        throw std::runtime_error("Video capture is not initialized");

    while(!framebuffers.enqueue({queuedFrame, timeOfFrame})) {
        std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::microseconds (60));
    }
//    HLogger.fmtLog<Paper::LogLevel::INF>("Frame queue: %zu", flippedframebuffers.size_approx());
}


#pragma region Finish
void VideoCapture::Finish()
{
    if(!initialized) {
        HLogger.fmtLog<Paper::LogLevel::INF>("Attempted to finish video capture when capture wasn't initialized, returning");
        return;
    }


    // sws_freeContext(swsCtx);

    initialized = false;
}

VideoCapture::~VideoCapture()
{
    if(initialized) Finish();

    for (auto& t : queueThreads)
        if (t.joinable())
            t.join();

    if (encodingThread.joinable())
        encodingThread.join();

    delete[] emptyFrame;

    HLogger.fmtLog<Paper::LogLevel::INF>("Deleting video capture {}", fmt::ptr(this));

    QueueContent frame;
    while (framebuffers.try_dequeue(frame)) {
        free(frame.first);
    }
}

#pragma endregion