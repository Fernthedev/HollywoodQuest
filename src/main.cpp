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

#define CASE_STR(value) \
    case AMEDIA_##value: return #value

char const* MediaErrorString(media_status_t err) {
    switch (err) {
        CASE_STR(OK);
        // CASE_STR(ERROR_BASE);
        CASE_STR(ERROR_UNKNOWN);
        CASE_STR(ERROR_MALFORMED);
        CASE_STR(ERROR_UNSUPPORTED);
        CASE_STR(ERROR_INVALID_OBJECT);
        CASE_STR(ERROR_INVALID_PARAMETER);
        CASE_STR(ERROR_INVALID_OPERATION);
        CASE_STR(ERROR_END_OF_STREAM);
        CASE_STR(ERROR_IO);
        CASE_STR(ERROR_WOULD_BLOCK);
        CASE_STR(DRM_ERROR_BASE);
        CASE_STR(DRM_NOT_PROVISIONED);
        CASE_STR(DRM_RESOURCE_BUSY);
        CASE_STR(DRM_DEVICE_REVOKED);
        CASE_STR(DRM_SHORT_BUFFER);
        CASE_STR(DRM_SESSION_NOT_OPENED);
        CASE_STR(DRM_TAMPER_DETECTED);
        CASE_STR(DRM_VERIFY_FAILED);
        CASE_STR(DRM_NEED_KEY);
        CASE_STR(DRM_LICENSE_EXPIRED);
        CASE_STR(IMGREADER_ERROR_BASE);
        CASE_STR(IMGREADER_NO_BUFFER_AVAILABLE);
        CASE_STR(IMGREADER_MAX_IMAGES_ACQUIRED);
        CASE_STR(IMGREADER_CANNOT_LOCK_IMAGE);
        CASE_STR(IMGREADER_CANNOT_UNLOCK_IMAGE);
        CASE_STR(IMGREADER_IMAGE_NOT_LOCKED);
        case AMEDIACODEC_ERROR_INSUFFICIENT_RESOURCE:
            return "ERROR_INSUFFICIENT_RESOURCE";
        case AMEDIACODEC_ERROR_RECLAIMED:
            return "ERROR_RECLAIMED";
    }
    return "Unknown";
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
