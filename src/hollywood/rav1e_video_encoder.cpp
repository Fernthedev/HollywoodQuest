#ifdef USE_AV1
#include "render/rav1e_video_encoder.hpp"

#include <utility>

#include "rav1e/rav1e.h"

#include "main.hpp"

using namespace Hollywood;

Rav1eVideoEncoder::Rav1eVideoEncoder(const uint32_t width, const uint32_t height, const uint32_t fpsRate,
                                     std::string filename, const uint32_t bitrate)
                                     : AbstractVideoEncoder(width, height,
                                                            fpsRate),
                                                            filename(std::move(filename)),
                                                            bitrate(bitrate) {}
void Rav1eVideoEncoder::Init() {
    if (context || initialized) {
        HLogger.fmtLog<Paper::LogLevel::INF>("Initialized already, throwing");
        throw std::runtime_error("Context is already initialized");
    }

    HLogger.fmtLog<Paper::LogLevel::INF>("Initializing rav1e video encoder");

    outfile = std::ofstream(filename);
    if (!outfile)
    {
        HLogger.fmtLog<Paper::LogLevel::INF>("Could not open {}", filename.c_str());
        return;
    }

    auto config = rav1e_config_default();

    HandleError(config);

    // * Available keys and values
    // * - "width": width of the frame, default 640
    // * - "height": height of the frame, default 480
    // * - "speed": 0-10, default 6
    // * - "threads": maximum number of threads to be used
    // * - "tune": "psnr"-"psychovisual", default "psychovisual"
    // * - "quantizer": 0-255, default 100
    // * - "tiles": total number of tiles desired (0 denotes auto), default 0
    // * - "tile_rows": number of tiles horizontally (must be a power of two, overridden by tiles if present), default 0
    // * - "tile_cols": number of tiles vertically (must be a power of two, overridden by tiles if present), default 0
    // * - "min_quantizer": minimum allowed base quantizer to use in bitrate mode, default 0
    // * - "bitrate": target bitrate for the bitrate mode (required for two pass mode), default 0
    // * - "key_frame_interval": maximum interval between two keyframes, default 240
    // * - "min_key_frame_interval": minimum interval between two keyframes, default 12
    // * - "switch_frame_interval": interval between switch frames, default 0
    // * - "reservoir_frame_delay": number of temporal units over which to distribute the reservoir usage, default None
    // * - "rdo_lookahead_frames": number of frames to read ahead for the RDO lookahead computation, default 40
    // * - "low_latency": flag to enable low latency mode, default false
    // * - "enable_timing_info": flag to enable signaling timing info in the bitstream, default false
    // * - "still_picture": flag for still picture mode, default false
    HandleError(rav1e_config_parse_int(config, "height", height));
    HandleError(rav1e_config_parse_int(config, "width", width));
    HandleError(rav1e_config_parse_int(config, "speed", 9));
    HandleError(rav1e_config_parse_int(config, "threads", 3));
    HandleError(rav1e_config_parse_int(config, "rdo_lookahead_frames", 2));
    HandleError(rav1e_config_parse_int(config, "key_frame_interval", 2));
    HandleError(rav1e_config_parse_int(config, "bitrate", bitrate * 1000));


    auto aspectRatio = RaRational{width, height};
    auto timeRatio = RaRational{1, fpsRate};

    RaChromaSampling subsampling = RA_CHROMA_SAMPLING_CS444;
    RaChromaSamplePosition chromaPos = RA_CHROMA_SAMPLE_POSITION_UNKNOWN;
    RaPixelRange pixelRange = RA_PIXEL_RANGE_FULL;

    RaColorPrimaries colorPrimaries = RA_COLOR_PRIMARIES_BT709;

    // https://github.com/kornelski/cavif-rs/blob/b0a2d76db91b941ec713e8a1345940223a26f48b/ravif/src/av1encoder.rs#L156-L158
    RaTransferCharacteristics transfer = RA_TRANSFER_CHARACTERISTICS_SRGB;
    RaMatrixCoefficients matrix = RA_MATRIX_COEFFICIENTS_IDENTITY; // https://github.com/kornelski/cavif-rs/blob/b0a2d76db91b941ec713e8a1345940223a26f48b/ravif/src/av1encoder.rs#L152
    uint8_t bit_depth = 8; // 8 bits per pixel, 1 byte (256 color)

    HandleError(rav1e_config_set_color_description(config, matrix, colorPrimaries, transfer));
    HandleError(rav1e_config_set_pixel_format(config, bit_depth, subsampling, chromaPos, pixelRange));

    rav1e_config_set_sample_aspect_ratio(config, aspectRatio);
    rav1e_config_set_time_base(config, timeRatio);

    HandleError(context = rav1e_context_new(config));
    HandleError(frame = rav1e_frame_new(context));

    rav1e_config_unref(config);

    initialized = true;

    auto sequenceHeader = rav1e_container_sequence_header(context);
    outfile.write(reinterpret_cast<const char *>(sequenceHeader->data), sequenceHeader->len);
    rav1e_data_unref(sequenceHeader);

    HLogger.fmtLog<Paper::LogLevel::INF>("Finished rav1e setup");

    encodingThread = std::thread(&Rav1eVideoEncoder::EncodeLoop, this);
}

void Rav1eVideoEncoder::queueFrame(rgb24 *data, std::optional<float> timeOfFrame) {
    if(!initialized)
        throw std::runtime_error("Video capture is not initialized");

    while(!framebuffers.enqueue(data)) {
        std::this_thread::yield();
    }
}

size_t Rav1eVideoEncoder::approximateFramesToRender() {
    return 0;
}

void Rav1eVideoEncoder::Encode(rgb24* data) {
#define CLEAN Finish(); CRASH_UNLESS(false);

    HLogger.fmtLog<Paper::LogLevel::INF>("Encoding!");
    // https://github.com/kornelski/cavif-rs/blob/b0a2d76db91b941ec713e8a1345940223a26f48b/ravif/src/av1encoder.rs#L103


    // use heap array or stack array?
    auto len = width * height;
    uint8_t* yPlane = new uint8_t[len];
    uint8_t* uPlane = new uint8_t[len];
    uint8_t* vPlane = new uint8_t[len];

    for (int i = 0; i < calculateFrameSize(width, height) / sizeof(rgb24); i++) {
        auto frameData = data[i];
        yPlane[i] = (frameData.g);
        uPlane[i] = (frameData.b);
        vPlane[i] = (frameData.r);
    }

    int ret = 0;

    HLogger.fmtLog<Paper::LogLevel::INF>("Filling frame");
    try {
//        rav1e_frame_fill_plane(frame, 1, reinterpret_cast<uint8_t *>(data), calculateFrameSize(width, height), static_cast<ptrdiff_t>(3), 1); // stride is 3 because 3 data fields
        rav1e_frame_fill_plane(frame, 0, yPlane, len, static_cast<ptrdiff_t>(1),
                               1); // 0, 1 are just params for testing, NO IDEA WHAT TO DO WITH THIS
        rav1e_frame_fill_plane(frame, 1, uPlane, len, static_cast<ptrdiff_t>(1),
                               1); // 0, 1 are just params for testing, NO IDEA WHAT TO DO WITH THIS
        rav1e_frame_fill_plane(frame, 2, vPlane, len, static_cast<ptrdiff_t>(1),
                               1); // 0, 1 are just params for testing, NO IDEA WHAT TO DO WITH THIS


    delete[] yPlane;
    delete[] uPlane;
    delete[] vPlane;
    CheckError();
    HLogger.fmtLog<Paper::LogLevel::INF>("Frame filled {}", len * 3);

    HandleError(rav1e_send_frame(context, frame));
    HLogger.fmtLog<Paper::LogLevel::INF>("Frame sent");

    } catch (std::exception &e) {
        HLogger.fmtLog<Paper::LogLevel::INF>("Crash: {}", e.what());
        CRASH_UNLESS(false);
        throw e;
    }

    while (ret >= 0){
        RaPacket *p;
        ret = rav1e_receive_packet(context, &p);
        CheckError();
        if (ret < 0) {
            HLogger.fmtLog<Paper::LogLevel::INF>("Unable to receive packet {}", ret);
            CLEAN
        } else if (ret == RA_ENCODER_STATUS_SUCCESS) {
            HLogger.fmtLog<Paper::LogLevel::INF>("Write!");
            HLogger.fmtLog<Paper::LogLevel::INF>("Packet {}", p->len);
            HLogger.fmtLog<Paper::LogLevel::INF>("Data {} {}", fmt::ptr(p->data), p->len);

            outfile.write(reinterpret_cast<const char *>(p->data), p->len);
            outfile.flush();

            rav1e_packet_unref(p);
            break;
        } else if (ret == RA_ENCODER_STATUS_NEED_MORE_DATA) {
            HLogger.fmtLog<Paper::LogLevel::INF>("Need more frame data {}", p->input_frameno);

//            if (p->input_frameno > 0 && p->input_frameno % 10 == 0) {
//                HandleError(rav1e_send_frame(context, NULL)); // flush every 10 frames
//                continue;
//            }

            break;
//            if (ret < 0) {
//                HLogger.fmtLog<Paper::LogLevel::INF>("Unable to send frame %d", 0);
//                CLEAN
//            } else if (ret > 0) {
//                // Cannot happen in normal conditions
//                HLogger.fmtLog<Paper::LogLevel::INF>("Unable to append frame %d to the internal queue", 0);
//                CLEAN
//            }
        } else if (ret == RA_ENCODER_STATUS_LIMIT_REACHED) {
            HLogger.fmtLog<Paper::LogLevel::INF>("Limit reached");
            rav1e_packet_unref(p);
            break;
        } else if (ret > 0) {
            HLogger.fmtLog<Paper::LogLevel::INF>("Too many frames!");
            rav1e_packet_unref(p);
            break;
        }
    }

    rav1e_frame_unref(frame);
    frame = rav1e_frame_new(context);
    HLogger.fmtLog<Paper::LogLevel::INF>("Finished encoding");
#undef CLEAN
}

void Rav1eVideoEncoder::EncodeLoop() {
    HLogger.fmtLog<Paper::LogLevel::INF>("Starting encoding thread");

    while (initialized) {
        QueueContent frameData = nullptr;

        // Block instead?
        if (!framebuffers.try_dequeue(frameData)) {
            std::this_thread::yield();
            continue;
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        this->Encode(frameData);
        auto currentTime = std::chrono::high_resolution_clock::now();
        int64_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();

         HLogger.fmtLog<Paper::LogLevel::INF>("Took {} to add and encode frame", (long long) duration);

        free(frameData);
    }

    Finish();
    HLogger.fmtLog<Paper::LogLevel::INF>("Ending encoding thread");
}


#pragma region Finish
Rav1eVideoEncoder::~Rav1eVideoEncoder() {
    if(initialized) Finish();

    HLogger.fmtLog<Paper::LogLevel::INF>("Deleting video capture {}", fmt::ptr(this));

    QueueContent frame;
    while (framebuffers.try_dequeue(frame)) {
        free(frame);
    }
}


// TODO: Thread safe?
void Rav1eVideoEncoder::Finish()
{
    if(!initialized) {
        HLogger.fmtLog<Paper::LogLevel::INF>("Attempted to finish video capture when capture wasn't initialized, returning");
        return;
    }
    rav1e_send_frame(context, nullptr);

    // Wait until encoding finishes
    if (encodingThread.joinable())
        encodingThread.join();

    rav1e_frame_unref(frame);
    frame = nullptr;

    rav1e_context_unref(context);
    context = nullptr;

    // Flush video output, and finish
    outfile.flush();
    outfile.close();

    initialized = false;
}
#pragma endregion

void Rav1eVideoEncoder::HandleError(void *ptr) {
    if (!ptr) {
        HLogger.fmtLog<Paper::LogLevel::INF>("Rav1e error!");
        CheckError();
        throw std::runtime_error("Rav1e error!");
    }
}

void Rav1eVideoEncoder::HandleError(int i) {
    if (i < 0) {
        HLogger.fmtLog<Paper::LogLevel::INF>("Rav1e error!");
        CheckError();
        throw std::runtime_error("Rav1e error!");
    }
}

void Rav1eVideoEncoder::CheckError() {
    if (context) {
        auto code = rav1e_last_status(context);
        HLogger.fmtLog<Paper::LogLevel::INF>("Error: {}", rav1e_status_to_str(code));
    }
}
#endif