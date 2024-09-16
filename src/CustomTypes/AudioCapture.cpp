#include "CustomTypes/AudioCapture.hpp"

#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/Time.hpp"
#include "main.hpp"

using namespace Hollywood;

DEFINE_TYPE(Hollywood, AudioCapture);

void AudioWriter::OpenFile(std::string const& filename) {
    SAMPLE_RATE = UnityEngine::AudioSettings::get_outputSampleRate();

    if (writer.is_open())
        writer.close();

    logger.info("Audio file {}", filename.c_str());
    writer.open(filename, std::ios::binary);
    // write space for header
    char zero[HEADER_SIZE] = {0};
    writer.write(zero, HEADER_SIZE);
}

void AudioWriter::Write(ArrayW<float> audioData) {
    for (int i = 0; i < audioData.size(); i++) {
        // write the short to the stream
        short value = short(audioData[i] * 32767);
        writer.write(reinterpret_cast<char const*>(&value), sizeof(short));
    }
}

void AudioWriter::AddHeader() {
    long samples = (int(writer.tellp()) - HEADER_SIZE) / (BITS_PER_SAMPLE / 8);

    // go back to start of file
    writer.seekp(0);

    int intValue;
    short shortValue;

    intValue = 0x46464952;
    writer.write(reinterpret_cast<char const*>(&intValue), sizeof(int));  // "RIFF" in ASCII

    // write the number of bytes in the entire file
    intValue = (int) (HEADER_SIZE + (samples * BITS_PER_SAMPLE * channels / 8)) - 8;
    writer.write(reinterpret_cast<char const*>(&intValue), sizeof(int));

    intValue = 0x45564157;
    writer.write(reinterpret_cast<char const*>(&intValue), sizeof(int));  // "WAVE" in ASCII
    intValue = 0x20746d66;
    writer.write(reinterpret_cast<char const*>(&intValue), sizeof(int));  // "fmt " in ASCII
    intValue = 16;
    writer.write(reinterpret_cast<char const*>(&intValue), sizeof(int));

    // write the format tag. 1 = PCM
    shortValue = short(1);
    writer.write(reinterpret_cast<char const*>(&shortValue), sizeof(short));

    // write the number of channels.
    shortValue = short(channels);
    writer.write(reinterpret_cast<char const*>(&shortValue), sizeof(short));

    // write the sample rate. The number of audio samples per second
    writer.write(reinterpret_cast<char const*>(&SAMPLE_RATE), sizeof(int));

    intValue = SAMPLE_RATE * channels * (BITS_PER_SAMPLE / 8);
    writer.write(reinterpret_cast<char const*>(&intValue), sizeof(int));
    shortValue = (short) (channels * (BITS_PER_SAMPLE / 8));
    writer.write(reinterpret_cast<char const*>(&shortValue), sizeof(short));

    // 16 bits per sample
    writer.write(reinterpret_cast<char const*>(&BITS_PER_SAMPLE), sizeof(short));

    // "data" in ASCII. Start the data chunk.
    intValue = 0x61746164;
    writer.write(reinterpret_cast<char const*>(&intValue), sizeof(int));

    // write the number of bytes in the data portion
    intValue = (int) (samples * BITS_PER_SAMPLE * channels / 8);
    writer.write(reinterpret_cast<char const*>(&intValue), sizeof(int));

    // close the file stream
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
    startDspClock = UnityEngine::AudioSettings::get_dspTime() * sampleRate;
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
