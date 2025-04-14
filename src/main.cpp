#include "main.hpp"

#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "custom-types/shared/register.hpp"
#include "hooks.hpp"
#include "java.hpp"

bool syncTimes = false;

float startGameTime = 0;
float currentGameTime = 0;
long startDspClock = 0;
int sampleRate = 0;

void IssuePluginEvent(void (*function)(int), int id) {
    static auto icall = il2cpp_utils::resolve_icall<void, void*, int>("UnityEngine.GL::GLIssuePluginEvent");
    icall((void*) function, id);
}

static modloader::ModInfo modInfo = {MOD_ID, VERSION, 0};

extern "C" void setup(CModInfo* info) {
    *info = modInfo.to_c();
    Paper::Logger::RegisterFileContextId(MOD_ID);
    logger.info("Completed setup!");
}

extern "C" void late_load() {
    il2cpp_functions::Init();
    custom_types::Register::AutoRegister();
    Hooks::Install();
    Hollywood::LoadClassAsset();
}
