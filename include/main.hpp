#pragma once

#include "beatsaber-hook/shared/utils/logging.hpp"

constexpr auto logger = Paper::ConstLoggerContext(MOD_ID);

extern bool syncTimes;

extern float startGameTime;
extern float currentGameTime;
extern long startDspClock;
extern int sampleRate;

void IssuePluginEvent(void (*function)(int), int id);
