#pragma once

#include <fstream>

#include "../limiter.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "custom-types/shared/macros.hpp"

namespace Hollywood {
    struct AudioWriter {
        void OpenFile(std::string const& filename);
        void Initialize(int channels, int sampleRate);
        void Write(ArrayW<float> audioData);
        void Close();

        static constexpr int HEADER_SIZE = 44;
        static constexpr short BITS_PER_SAMPLE = 16;

       private:
        bool initialized = false;
        int channels;
        int sampleRate;
        SimpleLimiter limiter;
        std::ofstream writer;
    };
}

DECLARE_CLASS_CODEGEN(Hollywood, AudioCapture, UnityEngine::MonoBehaviour) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, OnAudioFilterRead, ArrayW<float> data, int audioChannels);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);

   public:
    void OpenFile(std::string const& filename);
    void Save();

    void SetMuted(bool value) {
        mute = value;
    }
    bool IsMuted() const {
        return mute;
    }

    bool IsRendering() const {
        return rendering;
    }

   private:
    AudioWriter writer = {};
    bool rendering = false;
    bool mute = true;
};
