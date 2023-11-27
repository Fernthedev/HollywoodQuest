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

// I do not have the patience to fix deprecated warnings when it works

namespace Muxer {
    AVFormatContext* videoContext;
    AVFormatContext* audioContext;
    AVCodecContext* audioCodecContext;
    AVFormatContext* outputContext;
    AVCodecContext* outputVideoCodecContext;
    AVCodecContext* outputAudioCodecContext;
    SwrContext* audioSwr;
    AVAudioFifo* audioFifo;
    AVFrame* audioFrame;
    uint8_t** audioSamples;

#define FREE(method, var, pref) if(var) { method(pref var); var = nullptr; }

    void cleanupTemporaries() {
        if(audioSamples)
            av_freep(&audioSamples[0]);
        FREE(av_freep, audioSamples, &);
        FREE(av_frame_free, audioFrame, &);
    }

    void cleanup() {
        FREE(avformat_free_context, videoContext,);
        FREE(avformat_free_context, audioContext,);
        FREE(avformat_free_context, outputContext,);
        FREE(avcodec_free_context, audioCodecContext, &);
        // close instead of free because they were allocated by the muxer streams
        FREE(avcodec_close, outputVideoCodecContext,);
        FREE(avcodec_close, outputAudioCodecContext,);

        FREE(swr_free, audioSwr, &);
        FREE(av_audio_fifo_free, audioFifo,);
        FREE(av_frame_free, audioFrame, &);
        HLogger.fmtLog<Paper::LogLevel::INF>("Finished muxing cleanup");
    }

#undef FREE

    // simple muxer that copies the source codec and encodes the audio
    // since the formats are fixed, it skips the majority of fallbacks and error handling
    void muxFiles(std::string_view sourceMp4, std::string_view sourceWav, std::string_view outputMp4) {
        HLogger.fmtLog<Paper::LogLevel::INF>("Muxing initializing");

        // av_log_set_callback(*[](void* ptr, int level, const char* fmt, __va_list args) {
        //     std::string msg = string_vformat(fmt, args);
        //     HLogger.fmtLog<Paper::LogLevel::INF>("FFMPEG [{}] {}", level, msg);
        // });


        // open video and deduce its information
        int err = avformat_open_input(&videoContext, sourceMp4.data(), nullptr, nullptr);
        if(err < 0) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Video open error: {}", av_err2str(err));
            return;
        }
        err = avformat_find_stream_info(videoContext, nullptr);
        if(err < 0) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Video stream info error: {}", av_err2str(err));
            return;
        }
        HLogger.fmtLog<Paper::LogLevel::DBG>("Using input video codec {}", avcodec_get_name(videoContext->streams[0]->codecpar->codec_id));

        int fps = videoContext->streams[0]->r_frame_rate.num / videoContext->streams[0]->r_frame_rate.den;
        HLogger.fmtLog<Paper::LogLevel::DBG>("Found fps {} bitrate {}", fps, videoContext->streams[0]->codecpar->bit_rate);


        // open audio and deduce its information
        err = avformat_open_input(&audioContext, sourceWav.data(), nullptr, nullptr);
        if(err < 0) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Audio open error: {}", av_err2str(err));
            return;
        }
        err = avformat_find_stream_info(audioContext, nullptr);
        if(err < 0) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Audio stream info error: {}", av_err2str(err));
            return;
        }
        AVStream* audioStream = audioContext->streams[0];
        audioStream->codecpar->channel_layout = av_get_default_channel_layout(audioStream->codecpar->channels);


        // create audio codec context for transcoder input
        AVCodec* audioCodec = avcodec_find_decoder(audioStream->codecpar->codec_id);
        if(!audioCodec) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Failed to make audio codec");
            return;
        }
        HLogger.fmtLog<Paper::LogLevel::DBG>("Using input audio codec {} ({})", audioCodec->name, audioCodec->long_name);
        audioCodecContext = avcodec_alloc_context3(audioCodec);

        err = avcodec_parameters_to_context(audioCodecContext, audioStream->codecpar);
        // err = avcodec_parameters_from_context(audioStream->codecpar, audioCodecContext);
        if(err < 0) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Audio codec params error: {}", av_err2str(err));
            return;
        }

        err = avcodec_open2(audioCodecContext, audioCodec, nullptr);
        if(err < 0) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Audio codec open error: {}", av_err2str(err));
            return;
        }
        audioCodecContext->pkt_timebase = audioStream->time_base;


        // allocate output and get defaults for its encoding
        err = avformat_alloc_output_context2(&outputContext, nullptr, nullptr, outputMp4.data());
        if(err < 0) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Output context error: {}", av_err2str(err));
            return;
        }


        // create video stream with input codec
        // const AVCodec* videoCodec = videoContext->streams[0]->codec->codec;
        AVStream* outputVideoStream = avformat_new_stream(outputContext, nullptr);
        if(!outputVideoStream) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Failed to make video stream");
            return;
        }
        err = avcodec_parameters_copy(outputVideoStream->codecpar, videoContext->streams[0]->codecpar);
        if(err < 0) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Output param copy: {}", av_err2str(err));
            return;
        }
        outputVideoStream->r_frame_rate.den = 1;
        outputVideoStream->r_frame_rate.num = fps;
        outputVideoStream->avg_frame_rate.den = 1;
        outputVideoStream->avg_frame_rate.num = fps;
        outputVideoStream->codecpar->bit_rate = videoContext->streams[0]->codecpar->bit_rate;
        // outputVideoStream->time_base.num = 1;
        // outputVideoStream->time_base.den = 1000;
        // avcodec_parameters_to_context(outputVideoStream->codec, outputVideoStream->codecpar);
        // outputVideoCodecContext = outputVideoStream->codec; // needs to be freed later
        // avcodec_open2(outputVideoCodecContext, nullptr, nullptr);
        HLogger.fmtLog<Paper::LogLevel::DBG>("Using output video codec {}", avcodec_get_name(outputVideoStream->codecpar->codec_id));


        // create audio stream with default mp4 codec (will require transcoding)
        audioCodec = avcodec_find_encoder(outputContext->oformat->audio_codec);
        if(!audioCodec) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Failed to find audio encoder");
            return;
        }
        HLogger.fmtLog<Paper::LogLevel::DBG>("Using output audio codec {} ({})", audioCodec->name, audioCodec->long_name);
        AVStream* outputAudioStream = avformat_new_stream(outputContext, audioCodec);
        if(!outputAudioStream) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Failed to make audio stream");
            return;
        }
        // configure some values not set by new_stream
        outputAudioCodecContext = outputAudioStream->codec;
        outputAudioCodecContext->channel_layout = av_get_channel_layout("stereo");
        outputAudioCodecContext->channels = av_get_channel_layout_nb_channels(outputAudioCodecContext->channel_layout);
        outputAudioCodecContext->sample_fmt = audioCodec->sample_fmts[0];
        outputAudioCodecContext->sample_rate = audioCodecContext->sample_rate;
        outputAudioCodecContext->frame_size = audioCodecContext->frame_size;
        outputAudioCodecContext->bit_rate = audioCodecContext->bit_rate;

        err = avcodec_open2(outputAudioCodecContext, audioCodec, nullptr);
        if(err < 0) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Audio codec open error: {}", av_err2str(err));
            return;
        }

        err = avcodec_parameters_from_context(outputAudioStream->codecpar, outputAudioCodecContext);
        if(err < 0) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Output audio codec params error: {}", av_err2str(err));
            return;
        }

        if (outputContext->oformat->flags & AVFMT_GLOBALHEADER)
            outputAudioCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        outputAudioStream->time_base.den = audioCodecContext->sample_rate;
        outputAudioStream->time_base.num = 1;


        // create transcoder resampler
        audioSwr = swr_alloc_set_opts(
            nullptr,
            outputAudioCodecContext->channel_layout,
            outputAudioCodecContext->sample_fmt,
            outputAudioCodecContext->sample_rate,
            audioCodecContext->channel_layout,
            audioCodecContext->sample_fmt,
            audioCodecContext->sample_rate,
            0,
            nullptr
        );
        if(!audioSwr) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Failed to make audio swr");
            return;
        }

        err = swr_init(audioSwr);
        if(err < 0) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Swr init error: {}", av_err2str(err));
            return;
        }

        // initialize an FIFO buffer (not sure this is necessary, it's in case the frame sizes differ)
        audioFifo = av_audio_fifo_alloc(outputAudioCodecContext->sample_fmt, outputAudioCodecContext->channels, outputAudioCodecContext->frame_size);
        if(!audioFifo) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Failed to make audio fifo");
            return;
        }


        // open output file
        err = avio_open2(&outputContext->pb, outputMp4.data(), AVIO_FLAG_WRITE, nullptr, nullptr);
        if(err < 0) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Output open error: {}", av_err2str(err));
            return;
        }
        err = avformat_write_header(outputContext, NULL);
        if(err < 0) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Write header error: {}", av_err2str(err));
            return;
        }

        // write all frames from video and audio, stopping audio at video end
        double audioTime = 0, videoTime = 0;
        bool audioFinished = false;
        int64_t audioPts = 0;

        AVPacket packet, outputPacket;
        int audioFrameSize = outputAudioCodecContext->frame_size;

        HLogger.fmtLog<Paper::LogLevel::INF>("Beginning muxing");
        HLogger.fmtLog<Paper::LogLevel::DBG>("Video stream: {} Audio stream: {}", outputVideoStream->index, outputAudioStream->index);

        while(true) {
            if(audioFinished || audioTime > videoTime) {
                err = av_read_frame(videoContext, &packet);
                if(err == AVERROR_EOF) {
                    av_packet_unref(&packet);
                    break;
                }
                if(err < 0) {
                    HLogger.fmtLog<Paper::LogLevel::ERR>("Read video frame error: {}", av_err2str(err));
                    av_packet_unref(&packet);
                    continue;
                }

                av_init_packet(&outputPacket);

                err = av_packet_ref(&outputPacket, &packet);
                if(err < 0) {
                    HLogger.fmtLog<Paper::LogLevel::ERR>("Packet copy props error: {}", av_err2str(err));
                    av_packet_unref(&outputPacket);
                    av_packet_unref(&packet);
                    continue;
                }
                // outputPacket.data = packet.data;
                // outputPacket.size = packet.size;
                // make sure stream index is correct, can't just copy from the input
                outputPacket.stream_index = outputVideoStream->index;

                av_interleaved_write_frame(outputContext, &outputPacket);

                videoTime = packet.pts * (outputVideoStream->time_base.num / (double) outputVideoStream->time_base.den);

                av_packet_unref(&packet);
            }
            else if(!audioFinished) {
                // transcode frames until we have an output frame in the buffer
                while(av_audio_fifo_size(audioFifo) < audioFrameSize) {
                    cleanupTemporaries();

                    // allocate a frame and a samples array to store audio
                    audioFrame = av_frame_alloc();
                    if(!audioFrame) {
                        HLogger.fmtLog<Paper::LogLevel::ERR>("Failed to make audio frame");
                        continue;
                    }

                    err = av_read_frame(audioContext, &packet);
                    if(err == AVERROR_EOF) {
                        audioFinished = true;
                        av_packet_unref(&packet);
                        break;
                    }
                    if(err < 0) {
                        HLogger.fmtLog<Paper::LogLevel::ERR>("Read audio frame error: {}", av_err2str(err));
                        av_packet_unref(&packet);
                        continue;
                    }

                    // send the frame to the decoder
                    err = avcodec_send_packet(audioCodecContext, &packet);
                    if(err < 0) {
                        HLogger.fmtLog<Paper::LogLevel::ERR>("Send audio packet error: {}", av_err2str(err));
                        av_packet_unref(&packet);
                        continue;
                    }

                    // get the decoded frame back
                    err = avcodec_receive_frame(audioCodecContext, audioFrame);
                    av_packet_unref(&packet);

                    if(err == AVERROR(EAGAIN))
                        continue;
                    // can this happen if the earlier EOF didn't?
                    if(err == AVERROR_EOF) {
                        audioFinished = true;
                        break;
                    }
                    if(err < 0) {
                        HLogger.fmtLog<Paper::LogLevel::ERR>("Receive audio frame error: {}", av_err2str(err));
                        continue;
                    }

                    err = av_samples_alloc_array_and_samples(&audioSamples, nullptr,
                        outputAudioCodecContext->channels,
                        audioFrame->nb_samples,
                        outputAudioCodecContext->sample_fmt, 0);
                    if(err < 0) {
                        HLogger.fmtLog<Paper::LogLevel::ERR>("Samples array alloc error: {}", av_err2str(err));
                        continue;
                    }

                    // get converted samples
                    err = swr_convert(audioSwr, audioSamples, audioFrame->nb_samples, (const uint8_t**) audioFrame->extended_data, audioFrame->nb_samples);
                    if(err < 0) {
                        HLogger.fmtLog<Paper::LogLevel::ERR>("Swr convert error: {}", av_err2str(err));
                        continue;
                    }

                    // realloc buffer to size + new frame size
                    err = av_audio_fifo_realloc(audioFifo, av_audio_fifo_size(audioFifo) + audioFrame->nb_samples);
                    if(err < 0) {
                        HLogger.fmtLog<Paper::LogLevel::ERR>("Fifo realloc error: {}", av_err2str(err));
                        continue;
                    }

                    // write converted frame to fifo
                    err = av_audio_fifo_write(audioFifo, (void**) audioSamples, audioFrame->nb_samples);
                    if(err < 0) {
                        HLogger.fmtLog<Paper::LogLevel::ERR>("Fifo write error: {}", av_err2str(err));
                        continue;
                    }
                }
                cleanupTemporaries();

                // write all frames in the buffer
                while(av_audio_fifo_size(audioFifo) >= audioFrameSize) {
                    cleanupTemporaries();

                    audioFrame = av_frame_alloc();
                    if(!audioFrame) {
                        HLogger.fmtLog<Paper::LogLevel::ERR>("Failed to make audio frame");
                        continue;
                    }
                    // add frame metadata
                    audioFrame->nb_samples = audioFrameSize;
                    audioFrame->channel_layout = outputAudioCodecContext->channel_layout;
                    audioFrame->channels = outputAudioCodecContext->channels;
                    audioFrame->format = outputAudioCodecContext->sample_fmt;
                    audioFrame->sample_rate = outputAudioCodecContext->sample_rate;

                    err = av_frame_get_buffer(audioFrame, 0);
                    if(err < 0) {
                        HLogger.fmtLog<Paper::LogLevel::ERR>("Audio get buffer error: {}", av_err2str(err));
                        continue;
                    }

                    // get a frame from the fifo buffer
                    err = av_audio_fifo_read(audioFifo, (void**) audioFrame->data, audioFrameSize);
                    if(err < 0) {
                        HLogger.fmtLog<Paper::LogLevel::ERR>("Fifo read error: {}", av_err2str(err));
                        continue;
                    }

                    // update pts (timestamp)
                    audioFrame->pts = audioPts;
                    audioPts += audioFrameSize;

                    // send the frame to the encoder
                    err = avcodec_send_frame(outputAudioCodecContext, audioFrame);
                    if(err < 0 && err != AVERROR_EOF) {
                        HLogger.fmtLog<Paper::LogLevel::ERR>("Send audio frame error: {}", av_err2str(err));
                        return;
                    }

                    av_init_packet(&outputPacket);

                    // get the encoded frame
                    err = avcodec_receive_packet(outputAudioCodecContext, &outputPacket);
                    if(err == AVERROR(EAGAIN)) {
                        av_packet_unref(&outputPacket);
                        continue;
                    }
                    if(err == AVERROR_EOF) {
                        av_packet_unref(&outputPacket);
                        break;
                    }
                    if(err < 0) {
                        HLogger.fmtLog<Paper::LogLevel::ERR>("Receive audio packet error: {}", av_err2str(err));
                        av_packet_unref(&outputPacket);
                        continue;
                    }
                    outputPacket.stream_index = outputAudioStream->index;

                    av_interleaved_write_frame(outputContext, &outputPacket);

                    audioTime = audioPts / (double) outputAudioCodecContext->sample_rate;
                }
                cleanupTemporaries();
            }
        }
        err = av_write_trailer(outputContext);
        if(err < 0) {
            HLogger.fmtLog<Paper::LogLevel::ERR>("Write trailer error: {}", av_err2str(err));
            return;
        }

        HLogger.fmtLog<Paper::LogLevel::INF>("Finished muxing");
    }
}
