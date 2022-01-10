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

constexpr const CameraRecordingSettings recordingSettings {
    .width = 1920,
    .height = 1080,
    .fps = 45,
    .bitrate = 5000,
    .movieModeRendering = true
};
const int FOV = 90;

Logger& loggingFunction() {
    static auto logging = new Logger(modInfo, LoggerOptions(false, true));
    return *logging;
}

Configuration& getConfig() {
    static Configuration config(modInfo);
    config.Load();
    return config;
}


MAKE_HOOK_MATCH(PauseController_get_canPause, &GlobalNamespace::PauseController::get_canPause, bool, PauseController* self) {
    #ifdef DO_FPS_RECORD
    Replay::CameraCapture* cameraCapture = GetFirstEnabledComponent<Replay::CameraCapture*>();
    if(cameraCapture != nullptr) {
        return false;
    }
    #endif

    return PauseController_get_canPause(self);
}

template<class T>
T GetFirstEnabledComponent() {
    ArrayW<T> trs = UnityEngine::Resources::FindObjectsOfTypeAll<T>();
    if (!trs) return nullptr;

    for (auto item: trs) {
        if (!item)
            continue;

        UnityEngine::GameObject *go = item->get_gameObject();
        // log(to_utf8(csstrtostr(UnityEngine::Transform::GetName(trs->values[i]))));
        if (go && go->get_activeInHierarchy()) {
            return item;
        }
    }

    return nullptr;
}

UnityEngine::Matrix4x4 MatrixTranslate(UnityEngine::Vector3 vector) {
    UnityEngine::Matrix4x4 result;
    result.m00 = 1;
    result.m01 = 0;
    result.m02 = 0;
    result.m03 = vector.x;
    result.m10 = 0;
    result.m11 = 1;
    result.m12 = 0;
    result.m13 = vector.y;
    result.m20 = 0;
    result.m21 = 0;
    result.m22 = 1;
    result.m23 = vector.z;
    result.m30 = 0;
    result.m31 = 0;
    result.m32 = 0;
    result.m33 = 1;
    return result;
}

// Used in other files through extern
UnityEngine::RenderTexture* texture;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnreachableCode"
MAKE_HOOK_MATCH(LightManager_OnWillRenderObject, &LightManager::OnWillRenderObject, void, LightManager* self) {
    UnityEngine::GameObject *cameraGO = UnityEngine::Camera::get_main()->get_gameObject();


    // Camera magic to make it work with rendering
    auto *audioCapture = GetFirstEnabledComponent<Hollywood::AudioCapture *>();
    if (audioCapture == nullptr || audioCapture->get_gameObject() == nullptr) {
        log("Adding Audio Capture component to the AudioListener");
        audioCapture = GetFirstEnabledComponent<UnityEngine::AudioListener *>()->get_gameObject()->AddComponent<Hollywood::AudioCapture *>();
        audioCapture->OpenFile("sdcard/audio.wav");
    }


    static UnityEngine::GameObject *cameraGameObject = nullptr;
    UnityEngine::Camera *camera = nullptr;



    if (recordingSettings.movieModeRendering) {
        log("Going to set time delta");
        UnityEngine::Time::set_captureDeltaTime(1.0f / recordingSettings.fps);
        log("Set the delta time");
    }

    // log("getting camera");
    if (!cameraGameObject) {
        // Set to 60 hz
        // setRefreshRate(std::make_optional(60.0f));

        auto mainCamera = UnityEngine::Camera::get_main();

        mainCamera->set_stereoTargetEye(UnityEngine::StereoTargetEyeMask::None);

        // Idk what this does
        mainCamera->set_orthographic(false);

        mainCamera->set_fieldOfView(FOV);

        // Force it to render into texture
        mainCamera->set_forceIntoRenderTexture(true);

        cameraGameObject = UnityEngine::Object::Instantiate(mainCamera->get_gameObject());
        camera = cameraGameObject->GetComponent<UnityEngine::Camera *>();
        UnityEngine::Object::DontDestroyOnLoad(cameraGameObject);

        // Remove children
        while (cameraGameObject->get_transform()->get_childCount() > 0)
            UnityEngine::Object::DestroyImmediate(cameraGameObject->get_transform()->GetChild(0)->get_gameObject());
        UnityEngine::Object::DestroyImmediate(
                cameraGameObject->GetComponent(il2cpp_utils::newcsstr("CameraRenderCallbacksManager")));
        UnityEngine::Object::DestroyImmediate(cameraGameObject->GetComponent(il2cpp_utils::newcsstr("AudioListener")));
        UnityEngine::Object::DestroyImmediate(cameraGameObject->GetComponent(il2cpp_utils::newcsstr("MeshCollider")));


        camera->set_clearFlags(mainCamera->get_clearFlags());
        camera->set_nearClipPlane(mainCamera->get_nearClipPlane());
        camera->set_farClipPlane(mainCamera->get_farClipPlane());
        camera->set_cullingMask(mainCamera->get_cullingMask());
        // Makes the camera render before the main
        // Prioritize Hollywood camera
        camera->set_depth(mainCamera->get_depth() - 1);
        camera->set_backgroundColor(mainCamera->get_backgroundColor());
        camera->set_hideFlags(mainCamera->get_hideFlags());
        camera->set_depthTextureMode(mainCamera->get_depthTextureMode());

        // Set aspect ratio accordingly
        camera->set_aspect(float(recordingSettings.width) / float(recordingSettings.height));

        using setPixelRectType = function_ptr_t<void, UnityEngine::Camera *, UnityEngine::Rect &>;
        auto set_cullingMatrix = *reinterpret_cast<setPixelRectType>(il2cpp_functions::resolve_icall("UnityEngine.Camera::set_pixelRect_Injected"));

        auto pixelRect = UnityEngine::Rect(0, 0, (float) recordingSettings.width, (float) recordingSettings.height);
        set_cullingMatrix(camera, pixelRect);

        camera->set_projectionMatrix(mainCamera->get_projectionMatrix());

        cameraGameObject->get_transform()->set_eulerAngles(UnityEngine::Vector3(0.0f, 0.0f, 0.0f));
        cameraGameObject->get_transform()->set_position(UnityEngine::Vector3(0.0f, 2.0f, 0.0f));
        texture = UnityEngine::RenderTexture::New_ctor(recordingSettings.width, recordingSettings.height, 24,
                                                       (UnityEngine::RenderTextureFormat) UnityEngine::RenderTextureFormat::Default,
                                                       (UnityEngine::RenderTextureReadWrite) UnityEngine::RenderTextureReadWrite::Default);
        texture->set_wrapMode(UnityEngine::TextureWrapMode::Clamp);
        texture->set_filterMode(UnityEngine::FilterMode::Bilinear);
        texture->Create();
        UnityEngine::RenderTexture::set_active(texture);
        UnityEngine::Object::DontDestroyOnLoad(texture);
        cameraGameObject->AddComponent<Hollywood::CameraCapture *>();
    }


    camera = cameraGO->GetComponent<UnityEngine::Camera *>();


    if (texture->m_CachedPtr.m_value == nullptr) {
        texture = UnityEngine::RenderTexture::New_ctor(recordingSettings.width, recordingSettings.height, 24,
                                                       (UnityEngine::RenderTextureFormat) UnityEngine::RenderTextureFormat::Default,
                                                       (UnityEngine::RenderTextureReadWrite) UnityEngine::RenderTextureReadWrite::Default);
        texture->set_wrapMode(UnityEngine::TextureWrapMode::Clamp);
        texture->set_filterMode(UnityEngine::FilterMode::Bilinear);
        texture->Create();
        log("Texture was null, remade it");
    }
    camera->set_targetTexture(texture);
    camera->set_aspect(float(recordingSettings.width) / float(recordingSettings.height));


    using cullingMatrixType = function_ptr_t<void, UnityEngine::Camera *, UnityEngine::Matrix4x4>;
    auto set_cullingMatrix = *reinterpret_cast<cullingMatrixType>(il2cpp_functions::resolve_icall(
            "UnityEngine.Camera::set_cullingMatrix_Injected"));

    set_cullingMatrix(camera, UnityEngine::Matrix4x4::Ortho(-99999, 99999, -99999, 99999, 0.001f, 99999) *
                              MatrixTranslate(UnityEngine::Vector3::get_forward() * -99999 / 2) *
                              camera->get_worldToCameraMatrix());


    LightManager_OnWillRenderObject(self);
}
#pragma clang diagnostic pop

MAKE_HOOK_MATCH(HapticFeedbackController_Rumble, &HapticFeedbackController::PlayHapticFeedback, void, HapticFeedbackController* self, UnityEngine::XR::XRNode node, Libraries::HM::HMLib::VR::HapticPresetSO* hapticPreset) {
    // Disable vibration
    return;
}

MAKE_HOOK_MATCH(ResultsViewController_Init, &ResultsViewController::Init,void, ResultsViewController* self, GlobalNamespace::LevelCompletionResults* levelCompletionResults, IDifficultyBeatmap* difficultyBeatmap, bool practice, bool newHighScore) {
    log("ResultsViewController_Init");

    auto* audioCapture = GetFirstEnabledComponent<Hollywood::AudioCapture*>();
    if(audioCapture != nullptr) UnityEngine::Object::Destroy(audioCapture);

    auto* cameraCapture = GetFirstEnabledComponent<Hollywood::CameraCapture*>();
    if(cameraCapture != nullptr) UnityEngine::Object::Destroy(cameraCapture);

    UnityEngine::Time::set_captureDeltaTime(0.0f);


    ResultsViewController_Init(self, levelCompletionResults, difficultyBeatmap, practice, newHighScore);
}

//Temporary probably
MAKE_HOOK_MATCH(MainSettingsModel_Load, &GlobalNamespace::MainSettingsModelSO::Load, void, GlobalNamespace::MainSettingsModelSO* instance, bool forced)
{
    MainSettingsModel_Load(instance, forced);
    BoolSO* distortionsEnabled = (BoolSO*)UnityEngine::ScriptableObject::CreateInstance(csTypeOf(BoolSO*));
    distortionsEnabled->value = true;
    instance->screenDisplacementEffectsEnabled = distortionsEnabled ;
}

extern "C" void setup(ModInfo& info) {
    info.id = ID;
    info.version = VERSION;
    modInfo = info;
}

extern "C" void load() {
    loggingFunction().error("Starting Hollywood test");
    Hollywood::initialize();

    log("Installing hooks...");
    INSTALL_HOOK(loggingFunction(), PauseController_get_canPause);
    INSTALL_HOOK(loggingFunction(), LightManager_OnWillRenderObject);
    INSTALL_HOOK(loggingFunction(), HapticFeedbackController_Rumble);
    INSTALL_HOOK(loggingFunction(), ResultsViewController_Init);
    log("Installed all hooks!");
}
