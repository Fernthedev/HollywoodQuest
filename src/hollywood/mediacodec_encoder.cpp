
#include "render/mediacodec_encoder.hpp"

MediaCodecEncoder::MediaCodecEncoder(uint32_t width, uint32_t height, uint32_t fpsRate, int bitrate, std::string_view filepath)
    : AbstractVideoEncoder(width, height, fpsRate), bitrate(bitrate * 1000), filename(filepath) {}

MediaCodecEncoder::~MediaCodecEncoder() {
    Finish();
}

void MediaCodecEncoder::Init() {
    if(initialized)
        return;

    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, width);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, height);

    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "video/hevc"); // H.265 High Efficiency Video Coding
    // not available for the hardware encoder
    // AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, 12); // #12 COLOR_Format24bitBGR888 (RGB24)
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, 0x7fa30c07); // QOMX_COLOR_Format32bitRGBA8888
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);
    AMediaFormat_setFloat(format, AMEDIAFORMAT_KEY_FRAME_RATE, fpsRate);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 5);

    encoder = AMediaCodec_createEncoderByType("video/hevc");
    if(!encoder){
        HLogger.fmtLog<Paper::LogLevel::ERR>("Failed to create encoder");
        return;
    }

    media_status_t err = AMediaCodec_configure(encoder, format, NULL, NULL, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    if(err != AMEDIA_OK){
        HLogger.fmtLog<Paper::LogLevel::ERR>("Configure error: {}", (int) err);
        return;
    }

    err = AMediaCodec_start(encoder);
    if(err != AMEDIA_OK){
        HLogger.fmtLog<Paper::LogLevel::ERR>("Start error: {}", (int) err);
        return;
    }

    f = fopen(filename.c_str(), "w");
    if(!f) {
        HLogger.fmtLog<Paper::LogLevel::ERR>("Could not open {}", filename);
        return;
    }

    // Create a MediaMuxer.  We can't add the video track and start() the muxer here,
    // because our MediaFormat doesn't have the Magic Goodies. These can only be
    // obtained from the encoder after it has started processing data.

    // We're not actually interested in multiplexing audio (yet). We just want to convert
    // the raw H.264 elementary stream we get from MediaCodec into a .mp4 file.
    muxer = AMediaMuxer_new(fileno(f), AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
    if(!muxer){
        HLogger.fmtLog<Paper::LogLevel::ERR>("Failed to create muxer");
        return;
    }

    trackIndex = -1;
    muxerStarted = false;
    frameCounter = 0;
    framesProcessing = 0;
    initialized = true;

    HLogger.fmtLog<Paper::LogLevel::INF>("Finished initializing encoder at path {}", filename);
}

void MediaCodecEncoder::Finish() {
    HLogger.fmtLog<Paper::LogLevel::DBG>("Finishing recording");
    // Send the termination frame
    ssize_t inBufferIdx = AMediaCodec_dequeueInputBuffer(encoder, -1);
    size_t out_size;
    uint8_t* inBuffer = AMediaCodec_getInputBuffer(encoder, inBufferIdx, &out_size);
    int64_t presentationTimeNs = computePresentationTimeNsec();

    // send end-of-stream to encoder, and drain remaining output
    AMediaCodec_queueInputBuffer(encoder, inBufferIdx, 0, out_size, presentationTimeNs, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
    drainEncoder(true);

    releaseEncoder();

    fclose(f);
    f = nullptr;
}

void MediaCodecEncoder::queueFrame(rgb24* queuedFrame){
    if(!initialized)
        return;

    // Feed any pending encoder output into the muxer.
    // HLogger.fmtLog<Paper::LogLevel::DBG>("Queueing frame, frames: {}", framesProcessing);
    framesProcessing++;
    drainEncoder(false);

    int spins = 0;
    while(framesProcessing >= 10) {
        HLogger.fmtLog<Paper::LogLevel::WRN>("Too many frames queued, draining until <10");
        drainEncoder(false);
        spins++;
        if(spins > 1000) {
            HLogger.fmtLog<Paper::LogLevel::CRIT>("Failed to recover encoder");
            return;
        }
    }

    // Generate a new frame of input.

    // Get the index of the next available input buffer. An app will typically use this with
    // getInputBuffer() to get a pointer to the buffer, then copy the data to be encoded or decoded
    // into the buffer before passing it to the codec.
    ssize_t inBufferIdx = AMediaCodec_dequeueInputBuffer(encoder, -1);

    // Get an input buffer. The specified buffer index must have been previously obtained from
    // dequeueInputBuffer, and not yet queued.
    size_t out_size;
    uint8_t* inBuffer = AMediaCodec_getInputBuffer(encoder, inBufferIdx, &out_size);

    for(int i = 0; i < out_size / 4; i ++) {
        memcpy(inBuffer + (i*4), queuedFrame + i, 3);
        // inBuffer[(i*4) + 3] = 255;
    }
    // here we actually copy the data.
    // memcpy(inBuffer, queuedFrame, out_size);
    free(queuedFrame);

    // Send the specified buffer to the codec for processing.
    // int64_t presentationTimeNs = timestamp;
    int64_t presentationTimeNs = computePresentationTimeNsec();

    media_status_t status = AMediaCodec_queueInputBuffer(encoder, inBufferIdx, 0, out_size, presentationTimeNs, 0);
    if(status != AMEDIA_OK)
        HLogger.fmtLog<Paper::LogLevel::WRN>("Something went wrong while pushing frame to input buffer");

    // Submit it to the encoder. The eglSwapBuffers call will block if the input
    // is full, which would be bad if it stayed full until we dequeued an output
    // buffer (which we can't do, since we're stuck here). So long as we fully drain
    // the encoder before supplying additional input, the system guarantees that we
    // can supply another frame without blocking.
    // AMediaCodec_flush(encoder);
}

// Extracts all pending data from the encoder.

// If endOfStream is not set, this returns when there is no more data to drain.  If it
// is set, we send EOS to the encoder, and then iterate until we see EOS on the output.
// Calling this with endOfStream set should be done once, right before stopping the muxer.
void MediaCodecEncoder::drainEncoder(bool endOfStream) {

    if(endOfStream) {
        HLogger.fmtLog<Paper::LogLevel::INF>("Draining encoder to end of stream");
        // only API >= 26
        // Send an empty frame with the end-of-stream flag set.
        // AMediaCodec_signalEndOfInputStream();
        // Instead, we construct that frame manually.
    }

    while (true) {
        ssize_t encoderStatus = AMediaCodec_dequeueOutputBuffer(encoder, &bufferInfo, 1000);

        if(encoderStatus == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            // no output available yet (not expected with timeout -1)
            if(!endOfStream)
                return;

            HLogger.fmtLog<Paper::LogLevel::DBG>("Output frame not yet available");
        } else if(encoderStatus == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            // not expected for an encoder
            HLogger.fmtLog<Paper::LogLevel::ERR>("Unexpected output buffer change");
        } else if(encoderStatus == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            // should happen before receiving buffers, and should only happen once
            if(muxerStarted) {
                HLogger.fmtLog<Paper::LogLevel::ERR>("Format changed twice!");
            }

            auto newFormat = AMediaCodec_getOutputFormat(encoder);
            if(!newFormat) {
                HLogger.fmtLog<Paper::LogLevel::ERR>("Failed to get new format");
            }

            HLogger.fmtLog<Paper::LogLevel::DBG>("Format changed: {}", AMediaFormat_toString(newFormat));

            // now that we have the Magic Goodies, start the muxer
            trackIndex = AMediaMuxer_addTrack(muxer, newFormat);
            media_status_t err = AMediaMuxer_start(muxer);
            if(err != AMEDIA_OK) {
                HLogger.fmtLog<Paper::LogLevel::ERR>("Start error: {}", (int) err);
            }

            muxerStarted = true;
        } else if(encoderStatus < 0) {
            HLogger.fmtLog<Paper::LogLevel::WRN>("Unexpected result from dequeueOutputBuffer: {}", encoderStatus);
            // let's ignore it
        } else {

            // HLogger.fmtLog<Paper::LogLevel::DBG>("Output buffer idx {}", encoderStatus);

            size_t out_size;
            uint8_t* encodedData = AMediaCodec_getOutputBuffer(encoder, encoderStatus, &out_size);
            if(out_size <= 0) {
                HLogger.fmtLog<Paper::LogLevel::WRN>("Encoded data of size {}", out_size);
            }

            if(!encodedData) {
                HLogger.fmtLog<Paper::LogLevel::WRN>("Output buffer {} was null", encoderStatus);
            }

            bool codec_cfg = (bufferInfo.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) != 0;
            if(codec_cfg) {
                // The codec config data was pulled out and fed to the muxer when we got
                // the INFO_OUTPUT_FORMAT_CHANGED status. Ignore it.
                HLogger.fmtLog<Paper::LogLevel::DBG>("Ignoring BUFFER_FLAG_CODEC_CONFIG");
                bufferInfo.size = 0;
            }

            if(bufferInfo.size != 0) {
                if(!muxerStarted) {
                    HLogger.fmtLog<Paper::LogLevel::WRN>("Muxer hasn't started");
                }
                // HLogger.fmtLog<Paper::LogLevel::DBG>("Muxing frame");
                framesProcessing--;

                // adjust the ByteBuffer values to match BufferInfo (not needed?)
                // encodedData.position(mBufferInfo.offset);
                // encodedData.limit(mBufferInfo.offset + mBufferInfo.size);

                AMediaMuxer_writeSampleData(muxer, trackIndex, encodedData, &bufferInfo);
            } else if(!codec_cfg)
                HLogger.fmtLog<Paper::LogLevel::WRN>("Buffer info empty");

            AMediaCodec_releaseOutputBuffer(encoder, encoderStatus, false);

            if((bufferInfo.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0) {
                if(!endOfStream)
                    HLogger.fmtLog<Paper::LogLevel::WRN>("Unexpected end of stream");
                break;
            }
        }
    }
}

// Releases encoder resources.  May be called after partial / failed initialization.
void MediaCodecEncoder::releaseEncoder() {
    if(encoder)
        AMediaCodec_stop(encoder);

    if(muxer)
        AMediaMuxer_stop(muxer);

    if(encoder) {
        AMediaCodec_delete(encoder);
        encoder = nullptr;
    }

    if(muxer) {
        AMediaMuxer_delete(muxer);
        muxer = nullptr;
    }

    initialized = false;
}

// Generates the presentation time for frame N, in nanoseconds.
long long MediaCodecEncoder::computePresentationTimeNsec() {
    frameCounter++;
    return static_cast<long long>(frameCounter * 1000000ull / (double) fpsRate);
}
