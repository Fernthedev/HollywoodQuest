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

DECLARE_CLASS_CODEGEN(Hollywood, AudioCapture, UnityEngine::MonoBehaviour,

    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(void, OnAudioFilterRead, Array<float>* data, int audioChannels);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);

    public:
        int SAMPLE_RATE = 48000;

        void Save();

        void OpenFile(const std::string& filename);

        std::ofstream writer;

        bool IsRendering() const {
            return Rendering;
        }

    private:
        const int HEADER_SIZE = 44;
        const short BITS_PER_SAMPLE = 16;

        int channels = 2;

        void AddHeader();

        void Write(Array<float>* audioData);
        
        bool Rendering = false;
)