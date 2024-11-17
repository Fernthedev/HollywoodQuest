#include "mux.hpp"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/audio_fifo.h"
#include "libswresample/swresample.h"
}

#include "beatsaber-hook/shared/utils/utils-functions.h"

// https://stackoverflow.com/questions/16768794/muxing-from-audio-and-video-files-with-ffmpeg
// https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/transcode_aac.c

// maybe if ffmpeg had a decent api I wouldn't have to make so many macros
#define CLEANUP(function, var, ...) Cleanup var##Cleanup (__VA_ARGS__ var, function)
#define EARLY_CLEAN(var) var##Cleanup.clean()
#define CANCEL_CLEAN(var) var##Cleanup.cancel()

#define ASSERTR(action, ...) if (!(action)) { logger.error(__VA_ARGS__); return; }
#define ASSERTC(action, ...) if (!(action)) { logger.error(__VA_ARGS__); continue; }
#define AV_E_R(action, msg) { int err = action; ASSERTR(err >= 0, msg " error: {}", av_err2str(err)); }
#define AV_E_C(action, msg) { int err = action; ASSERTC(err >= 0, msg " error: {}", av_err2str(err)); }
#define NULLR(var, action, msg) auto var = action; ASSERTR(var, msg);
#define NULLC(var, action, msg) auto var = action; ASSERTC(var, msg);

namespace Muxer {
    template <class T>
    struct Cleanup {
        Cleanup() = delete;
        Cleanup(Cleanup const&) = delete;
        Cleanup(T* item, void (*cleanup)(T*)) : item(item), cleanup(cleanup){};

        ~Cleanup() { clean(); }

        void cancel() { item = nullptr; }

        void clean() {
            if (item)
                cleanup(item);
            cancel();
        }

       private:
        T* item;
        void (*cleanup)(T*);
    };

    // simple muxer that copies the source codec and encodes the audio
    // since the formats are fixed, it skips the majority of fallbacks and error handling
    void muxFiles(std::string_view video, std::string_view audio, std::string_view outputMp4) {
        logger.info("muxing initializing {} {} -> {}", video, audio, outputMp4);

        if (LIBAVFORMAT_VERSION_INT != avformat_version())
            logger.warn("avformat version mismatch! headers {} lib {}", LIBAVFORMAT_VERSION_INT, avformat_version());
        if (LIBAVCODEC_VERSION_INT != avcodec_version())
            logger.warn("avcodec version mismatch! headers {} lib {}", LIBAVCODEC_VERSION_INT, avcodec_version());
        if (LIBAVUTIL_VERSION_INT != avutil_version())
            logger.warn("avutil version mismatch! headers {} lib {}", LIBAVUTIL_VERSION_INT, avutil_version());
        if (LIBSWRESAMPLE_VERSION_INT != swresample_version())
            logger.warn("swresample version mismatch! headers {} lib {}", LIBSWRESAMPLE_VERSION_INT, swresample_version());

        AVFormatContext* inputVideoFormat = nullptr;
        AVFormatContext* inputAudioFormat = nullptr;
        AVFormatContext* outputFormat = nullptr;
        uint8_t** audioSamples = nullptr;

        av_log_set_callback(*[](void* ptr, int level, char const* fmt, va_list args) {
            if (level > AV_LOG_VERBOSE)
                return;
            va_list copy;
            va_copy(copy, args);
            int size = vsnprintf(nullptr, 0, fmt, copy) + 1;
            va_end(copy);
            char buffer[size];
            vsnprintf(buffer, size, fmt, args);
            logger.debug("FFMPEG [{}] {}", level, (char*) buffer);
        });

        // open video and get format details
        AV_E_R(avformat_open_input(&inputVideoFormat, video.data(), nullptr, nullptr), "video open");
        CLEANUP(avformat_close_input, inputVideoFormat, &);

        AV_E_R(avformat_find_stream_info(inputVideoFormat, nullptr), "get video stream info");

        ASSERTR(inputVideoFormat->nb_streams > 0, "no streams in video");
        auto inputVideoStream = inputVideoFormat->streams[0];

        // log some info for sanity checks
        logger.debug("found input video codec {}", avcodec_get_name(inputVideoStream->codecpar->codec_id));
        float fps = inputVideoStream->r_frame_rate.num / (float) inputVideoStream->r_frame_rate.den;
        logger.debug("found fps {} bitrate {}", fps, inputVideoStream->codecpar->bit_rate);

        // open audio and get format details
        AV_E_R(avformat_open_input(&inputAudioFormat, audio.data(), nullptr, nullptr), "audio open");
        CLEANUP(avformat_close_input, inputAudioFormat, &);

        AV_E_R(avformat_find_stream_info(inputAudioFormat, nullptr), "audio stream info");

        ASSERTR(inputAudioFormat->nb_streams > 0, "no streams in audio");
        auto inputAudioStream = inputAudioFormat->streams[0];

        logger.debug("found input audio codec {}", avcodec_get_name(inputAudioStream->codecpar->codec_id));
        logger.debug(
            "found channels {} sample rate {} bitrate {} (using {})",
            inputAudioStream->codecpar->channels,
            inputAudioStream->codecpar->sample_rate,
            inputAudioStream->codecpar->bit_rate,
            inputAudioFormat->bit_rate
        );

        // find codec to decode the input audio
        NULLR(inputAudioCodec, avcodec_find_decoder(inputAudioStream->codecpar->codec_id), "failed to find audio decoder");

        NULLR(inputAudioDecoder, avcodec_alloc_context3(inputAudioCodec), "failed to make audio decoder");
        CLEANUP(avcodec_free_context, inputAudioDecoder, &);

        AV_E_R(avcodec_parameters_to_context(inputAudioDecoder, inputAudioStream->codecpar), "audio params to ctx");

        AV_E_R(avcodec_open2(inputAudioDecoder, inputAudioCodec, nullptr), "audio decoder open");

        inputAudioDecoder->pkt_timebase = inputAudioStream->time_base;

        // allocate output and get defaults for its encoding
        AV_E_R(avformat_alloc_output_context2(&outputFormat, nullptr, nullptr, outputMp4.data()), "output context");
        CLEANUP(avformat_free_context, outputFormat);

        // create video stream in output with input codec
        NULLR(outputVideoStream, avformat_new_stream(outputFormat, inputVideoFormat->video_codec), "failed to make video stream");

        AV_E_R(avcodec_parameters_copy(outputVideoStream->codecpar, inputVideoStream->codecpar), "output video param copy");

        outputVideoStream->r_frame_rate.den = inputVideoStream->r_frame_rate.den;
        outputVideoStream->r_frame_rate.num = inputVideoStream->r_frame_rate.num;
        outputVideoStream->avg_frame_rate.den = inputVideoStream->avg_frame_rate.den;
        outputVideoStream->avg_frame_rate.num = inputVideoStream->avg_frame_rate.num;
        outputVideoStream->time_base = inputVideoStream->time_base;

        logger.debug("using output video codec {}", avcodec_get_name(outputVideoStream->codecpar->codec_id));

        // create audio stream with default mp4 codec (may require transcoding)
        NULLR(outputAudioCodec, avcodec_find_encoder(outputFormat->oformat->audio_codec), "failed to find audio encoder");

        NULLR(outputAudioStream, avformat_new_stream(outputFormat, outputAudioCodec), "failed to make audio stream");

        NULLR(outputAudioEncoder, avcodec_alloc_context3(outputAudioCodec), "failed to make audio encoder");
        CLEANUP(avcodec_free_context, outputAudioEncoder, &);

        // set some basic parameters
        outputAudioEncoder->channels = 2;
        outputAudioEncoder->channel_layout = av_get_default_channel_layout(outputAudioEncoder->channels);
        outputAudioEncoder->sample_fmt = outputAudioCodec->sample_fmts[0];
        outputAudioEncoder->sample_rate = inputAudioDecoder->sample_rate;  // if these differ then it's even more annoying
        outputAudioEncoder->bit_rate = inputAudioFormat->bit_rate;  // stream bitrate isn't detected properly. maybe needs an update

        if (outputFormat->oformat->flags & AVFMT_GLOBALHEADER)
            outputAudioEncoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        outputAudioStream->time_base.den = outputAudioEncoder->sample_rate;
        outputAudioStream->time_base.num = 1;

        AV_E_R(avcodec_open2(outputAudioEncoder, outputAudioCodec, nullptr), "audio encoder open");

        AV_E_R(avcodec_parameters_from_context(outputAudioStream->codecpar, outputAudioEncoder), "audio params from ctx");

        logger.debug("using output audio codec {}", avcodec_get_name(outputFormat->oformat->audio_codec));
        logger.debug(
            "using channels {} sample rate {} bitrate {}",
            outputAudioStream->codecpar->channels,
            outputAudioStream->codecpar->sample_rate,
            outputAudioStream->codecpar->bit_rate
        );

        // create transcoder resampler
        NULLR(
            audioSwr,
            swr_alloc_set_opts(
                nullptr,
                outputAudioEncoder->channel_layout,
                outputAudioEncoder->sample_fmt,
                outputAudioEncoder->sample_rate,
                inputAudioDecoder->channel_layout,
                inputAudioDecoder->sample_fmt,
                inputAudioDecoder->sample_rate,
                0,
                nullptr
            ),
            "failed to make audio swr"
        );
        CLEANUP(swr_free, audioSwr, &);

        AV_E_R(swr_init(audioSwr), "swr init");

        // initialize an FIFO buffer in case the frame sizes differ
        NULLR(
            audioFifo,
            av_audio_fifo_alloc(outputAudioEncoder->sample_fmt, outputAudioEncoder->channels, outputAudioEncoder->frame_size),
            "failed to make audio fifo"
        );
        CLEANUP(av_audio_fifo_free, audioFifo);

        // open output file
        AV_E_R(avio_open2(&outputFormat->pb, outputMp4.data(), AVIO_FLAG_WRITE, nullptr, nullptr), "output open");

        AV_E_R(avformat_write_header(outputFormat, nullptr), "write header");

        // write all frames from video and audio, stopping audio at video end
        double audioTime = 0, videoTime = 0;
        bool audioFinished = false;
        int64_t audioPts = 0;

        AVPacket packet, outputPacket;
        int audioFrameSize = outputAudioEncoder->frame_size;

        logger.info("setup done, beginning muxing");
        logger.debug("video stream idx {} audio stream idx {}", outputVideoStream->index, outputAudioStream->index);

        while (true) {
            if (audioFinished || audioTime > videoTime) {
                int err = av_read_frame(inputVideoFormat, &packet);
                if (err == AVERROR_EOF)
                    break;
                AV_E_C(err, "read video frame");
                CLEANUP(av_packet_unref, packet, &);

                // don't cleanup because av_interleaved_write_frame takes our ref
                av_init_packet(&outputPacket);
                CLEANUP(av_packet_unref, outputPacket, &);

                AV_E_C(av_packet_ref(&outputPacket, &packet), "packet copy props");

                // make sure stream index is correct
                outputPacket.stream_index = outputVideoStream->index;

                AV_E_C(av_interleaved_write_frame(outputFormat, &outputPacket), "write video frame");
                CANCEL_CLEAN(outputPacket);

                videoTime = packet.pts * (outputVideoStream->time_base.num / (double) outputVideoStream->time_base.den);
            } else if (!audioFinished) {
                // transcode frames until we have an output frame in the buffer
                while (av_audio_fifo_size(audioFifo) < audioFrameSize) {
                    // allocate a frame and a samples array to store audio
                    NULLC(audioFrame, av_frame_alloc(), "failed to make audio frame");
                    CLEANUP(av_frame_free, audioFrame, &);

                    int err = av_read_frame(inputAudioFormat, &packet);
                    if (err == AVERROR_EOF) {
                        audioFinished = true;
                        break;
                    }
                    AV_E_C(err, "read audio frame");
                    CLEANUP(av_packet_unref, packet, &);

                    // send the frame to the decoder
                    AV_E_C(avcodec_send_packet(inputAudioDecoder, &packet), "send audio packet");
                    EARLY_CLEAN(packet);

                    // get the decoded frame back
                    err = avcodec_receive_frame(inputAudioDecoder, audioFrame);
                    // needs more input
                    if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
                        continue;
                    AV_E_C(err, "receive audio frame");

                    // create array for samples
                    AV_E_C(
                        av_samples_alloc_array_and_samples(
                            &audioSamples, nullptr, outputAudioEncoder->channels, audioFrame->nb_samples, outputAudioEncoder->sample_fmt, 0
                        ),
                        "alloc audio samples"
                    );
                    CLEANUP(
                        *[](uint8_t** samples) {
                            av_freep(&samples[0]);
                            av_free(samples);
                        },
                        audioSamples
                    );

                    // convert the samples into the array
                    AV_E_C(
                        swr_convert(
                            audioSwr, audioSamples, audioFrame->nb_samples, (uint8_t const**) audioFrame->extended_data, audioFrame->nb_samples
                        ),
                        "swr sample convert"
                    );

                    // write converted frame to fifo
                    AV_E_C(av_audio_fifo_realloc(audioFifo, av_audio_fifo_size(audioFifo) + audioFrame->nb_samples), "fifo realloc");

                    AV_E_C(av_audio_fifo_write(audioFifo, (void**) audioSamples, audioFrame->nb_samples), "fifo write");
                }
                // write all frames in the buffer
                while (av_audio_fifo_size(audioFifo) >= audioFrameSize) {
                    NULLC(audioFrame, av_frame_alloc(), "failed to make audio frame");
                    CLEANUP(av_frame_free, audioFrame, &);

                    // write info to the frams
                    audioFrame->nb_samples = audioFrameSize;
                    audioFrame->channels = outputAudioEncoder->channels;
                    audioFrame->channel_layout = outputAudioEncoder->channel_layout;
                    audioFrame->format = outputAudioEncoder->sample_fmt;
                    audioFrame->sample_rate = outputAudioEncoder->sample_rate;

                    // write the converted samples to the frame
                    AV_E_C(av_frame_get_buffer(audioFrame, 0), "allocate audio frame buffer");

                    AV_E_C(av_audio_fifo_read(audioFifo, (void**) audioFrame->data, audioFrameSize), "fifo read");

                    // update pts (timestamp)
                    audioFrame->pts = audioPts;
                    audioPts += audioFrameSize;

                    // send the frame to the encoder
                    AV_E_C(avcodec_send_frame(outputAudioEncoder, audioFrame), "send audio frame");

                    av_init_packet(&outputPacket);
                    CLEANUP(av_packet_unref, outputPacket, &);

                    // get the encoded frame
                    int err = avcodec_receive_packet(outputAudioEncoder, &outputPacket);
                    // needs more input
                    if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
                        continue;
                    AV_E_C(err, "receive audio packet");

                    outputPacket.stream_index = outputAudioStream->index;

                    av_interleaved_write_frame(outputFormat, &outputPacket);
                    CANCEL_CLEAN(outputPacket);

                    audioTime = audioPts / (double) outputAudioEncoder->sample_rate;
                }
            }
        }
        logger.debug("found video time {} audio time {}", videoTime, audioTime);

        AV_E_R(av_write_trailer(outputFormat), "write trailer");

        logger.info("finished muxing");
    }
}
