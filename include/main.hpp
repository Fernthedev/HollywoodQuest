#pragma once

#include "paper2_scotland2/shared/logger.hpp"

#include <media/NdkMediaError.h>

constexpr auto logger = Paper::ConstLoggerContext(MOD_ID);

extern bool syncTimes;

extern float startGameTime;
extern float currentGameTime;
extern long startDspClock;
extern int sampleRate;

void IssuePluginEvent(void (*function)(int), int id);

namespace Hollywood {
    void InstallHook();
}

char const* MediaErrorString(media_status_t err);
