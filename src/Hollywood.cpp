#include "Hollywood.hpp"

#include "CustomTypes/AudioCapture.hpp"
#include "CustomTypes/CameraCapture.hpp"
#include "UnityEngine/AudioListener.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/FilterMode.hpp"
#include "UnityEngine/GL.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Matrix4x4.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/RenderTextureFormat.hpp"
#include "UnityEngine/RenderTextureReadWrite.hpp"
#include "UnityEngine/StereoTargetEyeMask.hpp"
#include "UnityEngine/TextureWrapMode.hpp"
#include "UnityEngine/Vector3.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"
#include "custom-types/shared/register.hpp"
#include "main.hpp"
#include "mux.hpp"

inline UnityEngine::Matrix4x4 MatrixTranslate(UnityEngine::Vector3 const& vector) {
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

Hollywood::CameraCapture* Hollywood::SetCameraCapture(UnityEngine::Camera* camera, CameraRecordingSettings const& recordingSettings) {

    camera->stereoTargetEye = UnityEngine::StereoTargetEyeMask::None;

    // Idk what this does
    camera->orthographic = false;

    camera->fieldOfView = recordingSettings.fov;

    // Force it to render into texture
    camera->forceIntoRenderTexture = true;

    // Set aspect ratio accordingly
    camera->aspect = float(recordingSettings.width) / float(recordingSettings.height);

    static auto set_pixelRect =
        il2cpp_utils::resolve_icall<void, UnityEngine::Camera*, UnityEngine::Rect&>("UnityEngine.Camera::set_pixelRect_Injected");

    auto pixelRect = UnityEngine::Rect(0, 0, (float) recordingSettings.width, (float) recordingSettings.height);
    set_pixelRect(camera, pixelRect);

    auto texture = UnityEngine::RenderTexture::New_ctor(
        recordingSettings.width,
        recordingSettings.height,
        24,
        (UnityEngine::RenderTextureFormat) UnityEngine::RenderTextureFormat::Default,
        (UnityEngine::RenderTextureReadWrite) UnityEngine::RenderTextureReadWrite::Default
    );
    texture->wrapMode = UnityEngine::TextureWrapMode::Clamp;
    texture->filterMode = UnityEngine::FilterMode::Bilinear;
    texture->Create();
    UnityEngine::RenderTexture::set_active(texture);
    UnityEngine::Object::DontDestroyOnLoad(texture);
    auto cameraCapture = camera->gameObject->AddComponent<Hollywood::CameraCapture*>();
    cameraCapture->readOnlyTexture = texture;
    camera->targetTexture = texture;
    camera->aspect = float(recordingSettings.width) / float(recordingSettings.height);

    // destroy texture with camera?

    static auto set_cullingMatrix =
        il2cpp_utils::resolve_icall<void, UnityEngine::Camera*, UnityEngine::Matrix4x4>("UnityEngine.Camera::set_cullingMatrix_Injected");

    auto forwardMult = UnityEngine::Vector3::op_Multiply(UnityEngine::Vector3::get_forward(), -49999.5);
    auto mat = UnityEngine::Matrix4x4::Ortho(-99999, 99999, -99999, 99999, 0.001, 99999);
    mat = UnityEngine::Matrix4x4::op_Multiply(mat, MatrixTranslate(forwardMult));
    mat = UnityEngine::Matrix4x4::op_Multiply(mat, camera->worldToCameraMatrix);
    set_cullingMatrix(camera, mat);

    return cameraCapture;
}

Hollywood::AudioCapture* Hollywood::SetAudioCapture(UnityEngine::AudioListener* listener) {
    return listener->get_gameObject()->AddComponent<Hollywood::AudioCapture*>();
}

Hollywood::AudioCapture* Hollywood::SetAudioCapture(UnityEngine::AudioSource* source) {
    return source->get_gameObject()->AddComponent<Hollywood::AudioCapture*>();
}

void Hollywood::MuxFilesSync(std::string_view sourceMp4, std::string_view sourceWav, std::string_view outputMp4) {
    Muxer::muxFiles(sourceMp4, sourceWav, outputMp4);
    Muxer::cleanup();
}
