#include "main.hpp"

#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

auto logger = Paper::Logger::WithContext<MOD_ID>();

bool syncTimes = false;

float startGameTime = 0;
float currentGameTime = 0;
long startDspClock = 0;
int sampleRate = 0;

void IssuePluginEvent(void (*function)(int), int id) {
    static auto icall = il2cpp_utils::resolve_icall<void, void*, int>("UnityEngine.GL::GLIssuePluginEvent");
    icall((void*) function, id);
}
