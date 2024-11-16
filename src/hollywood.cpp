#include "hollywood.hpp"

#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/Time.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"
#include "custom-types/shared/register.hpp"
#include "java.hpp"
#include "main.hpp"
#include "mux.hpp"

// I'm pretty sure this is the same thread as OnAudioFilterRead, but if I sleep there, it locks up
MAKE_HOOK_NO_CATCH(fmod_output_mix, 0x0, int, char* output, void* p1, uint p2) {

    if (!syncTimes)
        return fmod_output_mix(output, p1, p2);

    char* system = *(char**) (output + 0x60);
    long dspClock = *(long*) (system + 0xc78);

    while (syncTimes) {
        long dspDelta = dspClock - startDspClock;
        float gameDelta = currentGameTime - startGameTime;
        long gameDeltaInSamples = gameDelta * sampleRate;
        // game time has pulled ahead, allow audio to run
        if (dspDelta < gameDeltaInSamples)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return fmod_output_mix(output, p1, p2);
}

void Hollywood::Init() {
    static bool initialized = false;
    if (initialized)
        return;

    custom_types::Register::AutoRegister();

    LoadClassAsset();
    // sendSurfaceCapture(nullptr, 0, 1, 30, false);

    logger.info("Installing audio mix hook...");
    uintptr_t libunity = baseAddr("libunity.so");
    uintptr_t fmod_output_mix_addr = findPattern(
        libunity, "ff 43 03 d1 a8 04 80 52 ed 33 04 6d eb 2b 05 6d e9 23 06 6d fc 6f 07 a9 fa 67 08 a9 f8 5f 09 a9 f6 57 0a a9 f4 4f", 0x2000000
    );
    logger.info("Found audio mix address: {}", fmod_output_mix_addr);
    INSTALL_HOOK_DIRECT(logger, fmod_output_mix, (void*) fmod_output_mix_addr);
    logger.info("Installed audio mix hook!");

    initialized = true;
}

void Hollywood::SetSyncTimes(bool value) {
    if (value) {
        sampleRate = UnityEngine::AudioSettings::get_outputSampleRate();
        startGameTime = currentGameTime = UnityEngine::Time::get_time();
        startDspClock = UnityEngine::AudioSettings::get_dspTime() * sampleRate;
    }
    syncTimes = value;
}

void Hollywood::SetScreenOn(bool value) {
    if (getenv("QuestRUE") == nullptr)
        setScreenOn(value);
}

void Hollywood::SetMuteSpeakers(bool value) {
    setMute(value);
}

void Hollywood::MuxFilesSync(std::string_view sourceMp4, std::string_view sourceWav, std::string_view outputMp4) {
    Muxer::muxFiles(sourceMp4, sourceWav, outputMp4);
    Muxer::cleanup();
}
