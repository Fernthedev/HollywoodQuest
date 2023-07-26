#pragma once

#include "../CustomTypes/AsyncGPUReadbackPluginRequest.hpp"

namespace Hollywood {

    class AbstractVideoEncoder {
    public:
        const uint32_t width;
        const uint32_t height;
        const uint32_t fpsRate;

        AbstractVideoEncoder(const uint32_t width, const uint32_t height, const uint32_t fpsRate) : width(width),
                                                                                                    height(height),
                                                                                                    fpsRate(fpsRate) {}

        /**
         * @brief Sends the frame to the encoder.
         * The implemented encoder is expected to own and free the data after.
         * This expects a malloc created pointer, not new
         *
         * @param data
         * @param timeOfFrame Depending on the type of encoder and mode,
         * this will be used to place the frame in the correct keyframe
         */
        virtual void queueFrame(rgb24* data) = 0; // implement in encoder
        virtual void Init() = 0; // todo: Encapsulate to force initialized variable to true?

        [[nodiscard]] bool isInitialized() const { return initialized; }


        // This aren't really needed but we'll keep them anyways
        [[nodiscard]] uint32_t getWidth() const {
            return width;
        }

        [[nodiscard]] uint32_t getHeight() const {
            return height;
        }

        [[nodiscard]] uint32_t getFpsRate() const {
            return fpsRate;
        }

        /**
         * @brief
         * @return Frames queued to render
         */
        virtual size_t approximateFramesToRender() = 0;

        virtual ~AbstractVideoEncoder() = default;

    protected:
        bool initialized = false;
    };

    // TODO: Audio recorder abstraction
}
