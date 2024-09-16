#include "main.hpp"

auto logger = Paper::Logger::WithContext<MOD_ID>();

bool syncTimes = false;

float startGameTime = 0;
float currentGameTime = 0;
long startDspClock = 0;
int sampleRate = 0;
