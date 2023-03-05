#pragma once

#include "UnityEngine/MonoBehaviour.hpp"

#include "custom-types/shared/macros.hpp"

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <string>

namespace Hollywood {
    struct AudioWriter {
        void OpenFile(const std::string& filename);
        void Write(Array<float>* audioData);
        void AddHeader();
        void SetChannels(int num);

        static inline const int HEADER_SIZE = 44;
        static inline const short BITS_PER_SAMPLE = 16;
        private:
            int channels = 2;
            int SAMPLE_RATE = 48000;
            std::ofstream writer;
    };
}

DECLARE_CLASS_CODEGEN(Hollywood, AudioCapture, UnityEngine::MonoBehaviour,

    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(void, OnAudioFilterRead, Array<float>* data, int audioChannels);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);

    public:
        void OpenFile(const std::string& filename);

        void Save();

        bool IsRendering() const {
            return Rendering;
        }

    private:
        AudioWriter writer = {};
        bool Rendering = false;
)
