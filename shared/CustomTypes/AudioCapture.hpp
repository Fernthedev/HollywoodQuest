#pragma once

#include <fstream>

#include "UnityEngine/MonoBehaviour.hpp"
#include "custom-types/shared/macros.hpp"

namespace Hollywood {
    struct AudioWriter {
        void OpenFile(std::string const& filename);
        void Write(ArrayW<float> audioData);
        void AddHeader();
        void SetChannels(int num);

        static inline int const HEADER_SIZE = 44;
        static inline short const BITS_PER_SAMPLE = 16;

       private:
        int channels = 2;
        int SAMPLE_RATE = 48000;
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

    bool IsRendering() const { return rendering; }

   private:
    AudioWriter writer = {};
    bool rendering = false;
};
