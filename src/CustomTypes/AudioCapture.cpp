#include "main.hpp"
#include "CustomTypes/AudioCapture.hpp"

#include "UnityEngine/AudioSettings.hpp"

using namespace Hollywood;

DEFINE_TYPE(Hollywood, AudioCapture);

void AudioWriter::OpenFile(const std::string& filename) {
    SAMPLE_RATE = UnityEngine::AudioSettings::get_outputSampleRate();

    if(writer.is_open())
        writer.close();

    HLogger.fmtLog<Paper::LogLevel::INF>("Audio file {}", filename.c_str());
    writer.open(filename, std::ios::binary);
    // write space for header
    char zero[HEADER_SIZE] = {0};
    writer.write(zero, HEADER_SIZE);
}

void AudioWriter::Write(Array<float>* audioData) {
    for (int i = 0; i < audioData->Length(); i++) {
        // write the short to the stream
        short value = short(audioData->values[i] * 32767);
        writer.write(reinterpret_cast<const char*>(&value), sizeof(short));
    }
}

void AudioWriter::AddHeader() {
    long samples = (int(writer.tellp()) - HEADER_SIZE) / (BITS_PER_SAMPLE / 8);

    // go back to start of file
    writer.seekp(0);

    int intValue;
    short shortValue;

    intValue = 0x46464952;
    writer.write(reinterpret_cast<const char*>(&intValue), sizeof(int)); // "RIFF" in ASCII

    // write the number of bytes in the entire file
    intValue = (int)(HEADER_SIZE + (samples * BITS_PER_SAMPLE * channels / 8)) - 8;
    writer.write(reinterpret_cast<const char*>(&intValue), sizeof(int));

    intValue = 0x45564157;
    writer.write(reinterpret_cast<const char*>(&intValue), sizeof(int)); // "WAVE" in ASCII
    intValue = 0x20746d66;
    writer.write(reinterpret_cast<const char*>(&intValue), sizeof(int)); // "fmt " in ASCII
    intValue = 16;
    writer.write(reinterpret_cast<const char*>(&intValue), sizeof(int));

    // write the format tag. 1 = PCM
    shortValue = short(1);
    writer.write(reinterpret_cast<const char*>(&shortValue), sizeof(short));

    // write the number of channels.
    shortValue = short(channels);
    writer.write(reinterpret_cast<const char*>(&shortValue), sizeof(short));

    // write the sample rate. The number of audio samples per second
    writer.write(reinterpret_cast<const char*>(&SAMPLE_RATE), sizeof(int));

    intValue = SAMPLE_RATE * channels * (BITS_PER_SAMPLE / 8);
    writer.write(reinterpret_cast<const char*>(&intValue), sizeof(int));
    shortValue = (short)(channels * (BITS_PER_SAMPLE / 8));
    writer.write(reinterpret_cast<const char*>(&shortValue), sizeof(short));

    // 16 bits per sample
    writer.write(reinterpret_cast<const char*>(&BITS_PER_SAMPLE), sizeof(short));

    // "data" in ASCII. Start the data chunk.
    intValue = 0x61746164;
    writer.write(reinterpret_cast<const char*>(&intValue), sizeof(int));

    // write the number of bytes in the data portion
    intValue = (int)(samples * BITS_PER_SAMPLE * channels / 8);
    writer.write(reinterpret_cast<const char*>(&intValue), sizeof(int));

    // close the file stream
    writer.close();
}

void AudioWriter::SetChannels(int num) {
    channels = num;
}


void AudioCapture::OpenFile(const std::string& filename) {
    Rendering = true;
    writer.OpenFile(filename);
}

void AudioCapture::Save() {
    Rendering = false;
    HLogger.fmtLog<Paper::LogLevel::INF>("Closing audio file and adding header");
    writer.AddHeader();
}

void AudioCapture::OnAudioFilterRead(Array<float>* data, int audioChannels) {
    // HLogger.fmtLog<Paper::LogLevel::INF>("Got data");
    if(Rendering) {
        // store the number of channels we are rendering
        if(audioChannels > 0)
            writer.SetChannels(audioChannels);
        // store the data stream
        writer.Write(data);
    }
}

void AudioCapture::OnDestroy() {
    Save();
}
