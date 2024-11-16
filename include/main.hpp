#pragma once

#include "beatsaber-hook/shared/utils/logging.hpp"

extern Paper::ConstLoggerContext<sizeof(MOD_ID)> logger;

extern bool syncTimes;

extern float startGameTime;
extern float currentGameTime;
extern long startDspClock;
extern int sampleRate;

void IssuePluginEvent(void (*function)(int), int id);
