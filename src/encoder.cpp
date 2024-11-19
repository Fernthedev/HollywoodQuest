#include "encoder.hpp"

#include "main.hpp"

AMediaCodec* Hollywood::CreateEncoder(int width, int height, int bitrate, int fps, char const* mime) {
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, width);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, height);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);

    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, 0x7f000789);  // COLOR_FormatSurface
    AMediaFormat_setFloat(format, AMEDIAFORMAT_KEY_FRAME_RATE, fps);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 30);

    auto encoder = AMediaCodec_createEncoderByType(mime);
    if (!encoder) {
        logger.error("Failed to create encoder");
        return nullptr;
    }

    char* name;
    AMediaCodec_getName(encoder, &name);
    logger.debug("Using encoder {}", name);
    AMediaCodec_releaseName(encoder, name);

    media_status_t err = AMediaCodec_configure(encoder, format, NULL, NULL, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    AMediaFormat_delete(format);

    if (err != AMEDIA_OK) {
        logger.error("Configure error: {}", (int) err);
        AMediaCodec_delete(encoder);
        return nullptr;
    }

    format = AMediaCodec_getInputFormat(encoder);
    logger.debug("Using input format {}", AMediaFormat_toString(format));
    AMediaFormat_delete(format);

    format = AMediaCodec_getOutputFormat(encoder);
    logger.debug("Using output format {}", AMediaFormat_toString(format));
    AMediaFormat_delete(format);

    return encoder;
}

void Hollywood::EncodeLoop(AMediaCodec* encoder, std::function<void(uint8_t*, size_t)> onOutputUnit, bool* stop) {
    AMediaCodecBufferInfo bufferInfo;

    logger.info("Starting encoding loop");

    while (!*stop) {
        int index = AMediaCodec_dequeueOutputBuffer(encoder, &bufferInfo, 1000);  // 1 ms

        if (index >= 0) {
            size_t outSize;
            uint8_t* data = AMediaCodec_getOutputBuffer(encoder, index, &outSize);

            // note: make sure to write the CODEC_CONFIG to have a valid raw .h264 file
            if (bufferInfo.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG)
                logger.debug("codec_config packet received");

            // not sure what outSize represents, but it seems wrong
            if (bufferInfo.size != 0) {
                // bufferInfo.offset seems to always be 0 for encoding
                onOutputUnit(data, bufferInfo.size);
            }

            AMediaCodec_releaseOutputBuffer(encoder, index, false);

            if (bufferInfo.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)
                break;
        }
    }

    logger.info("Encoding loop ended");
}
