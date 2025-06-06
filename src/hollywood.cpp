#include "hollywood.hpp"

#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/Time.hpp"
#include "hooks.hpp"
#include "java.hpp"
#include "main.hpp"
#include "mux.hpp"

static constexpr int TIMEOUT_LOOPS = 200;  // theoretically a bit over a second, given 5ms sleeps

static char* gOutput = nullptr;

static int timeout = 0;
static long currentGameDelta = 0;

// I'm pretty sure this is the same thread as OnAudioFilterRead, but if I sleep there, it locks up
MAKE_HOOK_NO_CATCH(fmod_output_mix, 0x0, int, char* output, void* p1, uint p2) {

    if (output != gOutput) {
        logger.debug("setting output to {}", fmt::ptr(output));
        gOutput = output;
    }

    if (!syncTimes)
        return fmod_output_mix(output, p1, p2);

    while (syncTimes) {
        long dspDelta = Hollywood::GetDSPClock() - startDspClock;
        float gameDelta = currentGameTime - startGameTime;
        long gameDeltaInSamples = gameDelta * sampleRate;
        // game time has pulled ahead, allow audio to run
        if (dspDelta < gameDeltaInSamples)
            break;
        // if the game delta hasn't updated for a while, exit
        if (gameDeltaInSamples != currentGameDelta) {
            currentGameDelta = gameDeltaInSamples;
            timeout = 0;
        }
        if (++timeout > TIMEOUT_LOOPS) {
            logger.warn("main thread timed out! allowing audio to continue...");
            break;
        }
        logger.debug(
            "sleeping audio thread ({})! dsp {} >= game {}", std::hash<std::thread::id>()(std::this_thread::get_id()), dspDelta, gameDeltaInSamples
        );
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return fmod_output_mix(output, p1, p2);
}

AUTO_INSTALL_FUNCTION(fmod_output_mix) {
    logger.info("Installing audio mix hook...");
    uintptr_t libunity = baseAddr("libunity.so");
    uintptr_t fmod_output_mix_addr = findPattern(
        libunity, "ff 43 03 d1 a8 04 80 52 ed 33 04 6d eb 2b 05 6d e9 23 06 6d fc 6f 07 a9 fa 67 08 a9 f8 5f 09 a9 f6 57 0a a9 f4 4f", 0x2000000
    );
    logger.info("Found audio mix address: {}", fmod_output_mix_addr);
    INSTALL_HOOK_DIRECT(logger, fmod_output_mix, (void*) fmod_output_mix_addr);
    logger.info("Installed audio mix hook!");
}

long Hollywood::GetDSPClock() {
    char* system = *(char**) (gOutput + 0x60);
    return *(long*) (system + 0xc78);
}

void Hollywood::SetSyncTimes(bool value) {
    if (value) {
        sampleRate = UnityEngine::AudioSettings::get_outputSampleRate();
        startGameTime = currentGameTime = UnityEngine::Time::get_time();
        startDspClock = GetDSPClock();
        logger.debug("unity dsp time {} vs internal {}", UnityEngine::AudioSettings::get_dspTime() * sampleRate, GetDSPClock());
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

void Hollywood::MuxFilesSync(std::string_view video, std::string_view audio, std::string_view outputMp4, double fps) {
    Muxer::muxFiles(video, audio, outputMp4, fps);
}
