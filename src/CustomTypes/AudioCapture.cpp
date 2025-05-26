#include "CustomTypes/AudioCapture.hpp"

#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/Time.hpp"
#include "hollywood.hpp"
#include "main.hpp"

using namespace Hollywood;

DEFINE_TYPE(Hollywood, AudioCapture);

template <class T>
static inline void WriteStream(std::ofstream& writer, T value) {
    writer.write(reinterpret_cast<char const*>(&value), sizeof(T));
}

void AudioWriter::OpenFile(std::string const& filename) {
    SAMPLE_RATE = UnityEngine::AudioSettings::get_outputSampleRate();

    if (writer.is_open())
        writer.close();

    logger.info("Audio file {}", filename);
    writer.open(filename, std::ios::binary);
    // write space for header
    char zero[HEADER_SIZE] = {0};
    writer.write(zero, HEADER_SIZE);
}

void AudioWriter::Write(ArrayW<float> audioData) {
    for (int i = 0; i < audioData.size(); i++) {
        // scale the data (from -1 to 1 according to unity) then write
        WriteStream<short>(writer, audioData[i] * std::numeric_limits<short>::max());
    }
}

void AudioWriter::AddHeader() {
    long size = (long) writer.tellp();

    // go back to start of file
    writer.seekp(0);

    WriteStream<int>(writer, 0x46464952);  // "RIFF" in ASCII

    // number of bytes in the entire file
    WriteStream<int>(writer, size);

    WriteStream<int>(writer, 0x45564157);  // "WAVE" in ASCII
    WriteStream<int>(writer, 0x20746d66);  // "fmt " in ASCII
    WriteStream<int>(writer, 16);

    // audio format: 1 = PCM
    WriteStream<short>(writer, 1);

    WriteStream<short>(writer, channels);
    WriteStream<int>(writer, SAMPLE_RATE);

    int byteRate = SAMPLE_RATE * channels * (BITS_PER_SAMPLE / 8);
    WriteStream<int>(writer, byteRate);

    short blockAlign = channels * (BITS_PER_SAMPLE / 8);
    WriteStream<short>(writer, channels);

    WriteStream<short>(writer, BITS_PER_SAMPLE);

    WriteStream<int>(writer, 0x61746164);  // "data" in ASCII

    // number of bytes in the data portion
    WriteStream<int>(writer, size - HEADER_SIZE);

    writer.close();
}

void AudioWriter::SetChannels(int num) {
    channels = num;
}

void AudioCapture::OpenFile(std::string const& filename) {
    rendering = true;
    writer.OpenFile(filename);

    sampleRate = UnityEngine::AudioSettings::get_outputSampleRate();
    startGameTime = currentGameTime = UnityEngine::Time::get_time();
    startDspClock = GetDSPClock();
    logger.debug("unity dsp time {} vs internal {}", UnityEngine::AudioSettings::get_dspTime() * sampleRate, GetDSPClock());
}

void AudioCapture::Save() {
    rendering = false;
    logger.info("Closing audio file and adding header");
    writer.AddHeader();
}

void AudioCapture::Update() {
    currentGameTime = UnityEngine::Time::get_time();
}

void AudioCapture::OnAudioFilterRead(ArrayW<float> data, int audioChannels) {
    if (!rendering)
        return;

    if (audioChannels > 0)
        writer.SetChannels(audioChannels);
    writer.Write(data);
}

void AudioCapture::OnDestroy() {
    Save();
}
