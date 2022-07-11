#include <dlfcn.h>
#include "main.hpp"
#include "beatsaber-hook/shared/utils/utils.h"
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "modloader/shared/modloader.hpp"
#include "beatsaber-hook/shared/utils/typedefs.h"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"
#include "beatsaber-hook/shared/config/rapidjson-utils.hpp"
#include "beatsaber-hook/shared/config/config-utils.hpp"


#include <limits>
#include <tuple>

#include "custom-types/shared/coroutine.hpp"
#include "custom-types/shared/register.hpp"

#include "Hollywood.hpp"


#include "GlobalNamespace/MainSettingsModelSO.hpp"
#include "GlobalNamespace/PauseController.hpp"
#include "GlobalNamespace/HapticFeedbackController.hpp"
#include "GlobalNamespace/ResultsViewController.hpp"
#include "GlobalNamespace/LightManager.hpp"
#include "GlobalNamespace/BoolSO.hpp"

#include "UnityEngine/StereoTargetEyeMask.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/AudioListener.hpp"
#include "UnityEngine/CameraClearFlags.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/TextureWrapMode.hpp"
#include "UnityEngine/FilterMode.hpp"
#include "UnityEngine/HideFlags.hpp"
#include "UnityEngine/DepthTextureMode.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/Matrix4x4.hpp"

#include "CustomTypes/CameraCapture.hpp"
#include "CustomTypes/AudioCapture.hpp"
#include "UnityEngine/Time.hpp"

using namespace System::Threading;
using namespace GlobalNamespace;

Paper::ConstLoggerContext<10> HLogger = Paper::Logger::WithContext<"Hollywood">();

Configuration& getConfig() {
    static Configuration config(modInfo);
    config.Load();
    return config;
}





// Used in other files through extern
UnityEngine::RenderTexture* texture;



extern "C" void setup(ModInfo& info) {
    info.id = MOD_ID;
    info.version = VERSION;
    modInfo = info;
}

extern "C" void load() {
    HLogger.fmtLog<Paper::LogLevel::INF>("Starting Hollywood test");
    Hollywood::initialize();

}
