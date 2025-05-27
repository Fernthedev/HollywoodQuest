#include "CustomTypes/AudioCapture.hpp"

#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/Time.hpp"
#include "hollywood.hpp"
#include "main.hpp"

using namespace Hollywood;

DEFINE_TYPE(Hollywood, AudioCapture);

static SimpleLimiter limiter;

template <class T>
static inline void WriteStream(std::ofstream& writer, T value) {
    writer.write(reinterpret_cast<char const*>(&value), sizeof(T));
}

void AudioWriter::OpenFile(std::string const& filename) {
    if (writer.is_open())
        writer.close();

    logger.info("Opening file for audio: {}", filename);
    writer.open(filename, std::ios::binary);
    // write space for header
    char zero[HEADER_SIZE] = {0};
    writer.write(zero, HEADER_SIZE);
}

void AudioWriter::Initialize(int channelsIn, int sampleRateIn) {
    if (initialized)
        return;
    initialized = true;
    channels = channelsIn;
    sampleRate = sampleRateIn;
    limiter.init(channels, sampleRate);
}

void AudioWriter::Write(ArrayW<float> audioData) {
    if (!initialized || !writer.is_open())
        return;
    for (auto& value : audioData) {
        // limit the data to between -1 and 1 (unity says the data will be in this range already but it lies)
        float limited = limiter.process(value);
        WriteStream<short>(writer, limited * std::numeric_limits<short>::max());
    }
}

void AudioWriter::Close() {
    if (!initialized || !writer.is_open())
        return;

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
    WriteStream<int>(writer, sampleRate);

    int byteRate = sampleRate * channels * (BITS_PER_SAMPLE / 8);
    WriteStream<int>(writer, byteRate);

    short blockAlign = channels * (BITS_PER_SAMPLE / 8);
    WriteStream<short>(writer, channels);

    WriteStream<short>(writer, BITS_PER_SAMPLE);

    WriteStream<int>(writer, 0x61746164);  // "data" in ASCII

    // number of bytes in the data portion
    WriteStream<int>(writer, size - HEADER_SIZE);

    writer.close();
    initialized = false;
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
    writer.Close();
}

void AudioCapture::Update() {
    currentGameTime = UnityEngine::Time::get_time();
}

void AudioCapture::OnAudioFilterRead(ArrayW<float> data, int channels) {
    if (!rendering)
        return;

    if (channels > 0)
        writer.Initialize(channels, sampleRate);
    writer.Write(data);
}

void AudioCapture::OnDestroy() {
    Save();
}
