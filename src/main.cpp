#include "main.hpp"

#include "java.hpp"
#include "beatsaber-hook/shared/api.hpp"
#include "custom-types/shared/register.hpp"

bool syncTimes = false;

float startGameTime = 0;
float currentGameTime = 0;
long startDspClock = 0;
int sampleRate = 0;

void IssuePluginEvent(void (*function)(int), int id) {
    static auto icall = i2c::resolve_icall<void, void*, int>("UnityEngine.GL::GLIssuePluginEvent");
    icall((void*) function, id);
}

static modloader::ModInfo modInfo = {MOD_ID, VERSION, 0};

extern "C" void setup(CModInfo* info) {
    *info = modInfo.to_c();
    Paper::Logger::RegisterFileContextId(MOD_ID);
    logger.info("Completed setup!");
}

extern "C" void late_load() {
    custom_types::Register::AutoRegister();
    Hollywood::InstallHook();
    Hollywood::LoadClassAsset();
}
