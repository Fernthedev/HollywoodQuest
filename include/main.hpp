#pragma once

#include "paper2_scotland2/shared/logger.hpp"

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
