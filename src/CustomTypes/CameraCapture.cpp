#include "CustomTypes/CameraCapture.hpp"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>
#include <media/NdkMediaCodec.h>

#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/DepthTextureMode.hpp"
#include "UnityEngine/FilterMode.hpp"
#include "UnityEngine/Matrix4x4.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/RenderTextureFormat.hpp"
#include "UnityEngine/RenderTextureReadWrite.hpp"
#include "UnityEngine/StereoTargetEyeMask.hpp"
#include "UnityEngine/TextureWrapMode.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Vector3.hpp"
#include "assets.hpp"
#include "egl.hpp"
#include "encoder.hpp"
#include "main.hpp"
#include "shader.hpp"
#include "thread_map.hpp"

DEFINE_TYPE(Hollywood, CameraCapture);

using namespace UnityEngine;
using namespace Hollywood;

struct SharedData {
    int width;
    int height;
    GLuint texture;
    GLuint buffer;
    AMediaCodec* encoder;
    ANativeWindow* window;
    EGLSurface surface;

    SharedData(int width, int height, GLuint texture, AMediaCodec* encoder, ANativeWindow* window) :
        width(width),
        height(height),
        texture(texture),
        buffer(-1),
        encoder(encoder),
        window(window),
        surface(nullptr) {}
};

static ThreadMap<SharedData> dataMap;

static void PluginUpdate(int id) {
    auto data = dataMap.get(id);
    if (!data)
        return;

    // initialize on render thread
    static Hollywood::Shader gammaShader = {IncludedAssets::gamma_vs_glsl.data, IncludedAssets::gamma_fs_glsl.data};

    if (!data->surface) {
        data->surface = MakeSurface(data->window);
        glGenVertexArrays(1, &data->buffer);
    }

    EGLState state;
    MakeCurrent(data->surface);

    if (gammaShader.Draw(data->width, data->height, data->texture, data->buffer))
        SwapBuffers(data->surface);
}

static void PluginDestroy(int id) {
    auto data = dataMap.get(id);
    if (!data)
        return;

    if (data->surface) {
        DestroySurface(data->surface);
        glDeleteVertexArrays(1, &data->buffer);
    }

    if (data->window)
        ANativeWindow_release(data->window);
    if (data->encoder) {
        AMediaCodec_stop(data->encoder);
        AMediaCodec_delete(data->encoder);
    }

    dataMap.remove(id);
}

static RenderTexture* CreateCameraTexture(int width, int height) {
    auto texture = RenderTexture::New_ctor(width, height, 24, RenderTextureFormat::Default, RenderTextureReadWrite::Default);
    Object::DontDestroyOnLoad(texture);
    texture->wrapMode = TextureWrapMode::Clamp;
    texture->filterMode = FilterMode::Bilinear;
    texture->Create();
    return texture;
}

static Matrix4x4 MatrixTranslate(Vector3 const& vector) {
    Matrix4x4 result;
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

void CameraCapture::Awake() {
    camera = GetComponent<Camera*>();

    if (dataId == -1)
        camera->enabled = false;
}

void CameraCapture::Init(int width, int height, int fps, int bitrate, float fov, bool hevc) {
    if (!camera)
        Awake();
    if (!camera || dataId != -1)
        return;

    logger.info("Making video capture");
    logger.debug("size {}/{} fps {} bitrate {} fov {} hevc {}", width, height, fps, bitrate, fov, hevc);

    if (texture)
        Object::Destroy(texture);

    texture = CreateCameraTexture(width, height);
    camera->targetTexture = texture;

    camera->stereoTargetEye = StereoTargetEyeMask::None;
    camera->aspect = width / (float) height;
    camera->fieldOfView = fov;
    camera->pixelRect = {0, 0, (float) width, (float) height};
    camera->rect = {0, 0, 1, 1};

    auto forwardMult = Vector3::op_Multiply(Vector3::get_forward(), -49999.5);
    auto mat = Matrix4x4::Ortho(-99999, 99999, -99999, 99999, 0.001, 99999);
    mat = Matrix4x4::op_Multiply(mat, MatrixTranslate(forwardMult));
    mat = Matrix4x4::op_Multiply(mat, camera->worldToCameraMatrix);
    camera->cullingMatrix = mat;

    camera->enabled = false;

    encoder = CreateEncoder(width, height, bitrate, fps, hevc ? "video/hevc" : "video/avc");

    ANativeWindow* window;
    auto err = AMediaCodec_createInputSurface(encoder, &window);
    if (err != AMEDIA_OK) {
        logger.error("Failed to create input surface: {}", (int) err);
        AMediaCodec_delete(encoder);
        return;
    }

    err = AMediaCodec_start(encoder);
    if (err != AMEDIA_OK) {
        logger.error("Failed to start encoder: {}", (int) err);
        AMediaCodec_delete(encoder);
        return;
    }

    int texId = (uintptr_t) texture->GetNativeTexturePtr().m_value.convert();
    dataId = dataMap.add(width, height, texId, encoder, window);

    fpsDelta = fps > 0 ? 1 / (double) fps : -1;
    // bias against skipping frames if target fps is similar enough to unity fps
    startTime = Time::get_time() - fpsDelta / 2;
    frames = 0;

    stopThread = false;
    thread = std::thread(&EncodeLoop, encoder, onOutputUnit, &stopThread);

    sampleRate = AudioSettings::get_outputSampleRate();
    startGameTime = currentGameTime = Time::get_time();
    startDspClock = AudioSettings::get_dspTime() * sampleRate;
}

void CameraCapture::Stop() {
    stopThread = true;
    if (thread.joinable())
        thread.join();

    if (dataId != -1)
        IssuePluginEvent(&PluginDestroy, dataId);

    encoder = nullptr;
    window = nullptr;
    dataId = -1;
}

void CameraCapture::Update() {
    if (!camera || dataId == -1)
        return;

    bool doFrame = true;
    if (fpsDelta > 0 && Time::get_time() - startTime < fpsDelta * frames)
        doFrame = false;

    // camera->Render horribly breaks the texture, probably because of threading
    camera->enabled = doFrame;
    if (doFrame) {
        frames++;
        IssuePluginEvent(&PluginUpdate, dataId);
    }

    while (syncTimes) {
        currentGameTime = Time::get_time();
        long dspClock = AudioSettings::get_dspTime() * sampleRate;
        long dspDelta = dspClock - startDspClock;
        float gameDelta = currentGameTime - startGameTime;
        long gameDeltaInSamples = gameDelta * sampleRate;
        if (gameDeltaInSamples < dspDelta + (sampleRate * 0.1))
            break;
        // game time is too far ahead, pause for audio
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void CameraCapture::OnPostRender() {
    if (camera)
        camera->enabled = false;
}

void CameraCapture::OnDestroy() {
    Stop();
    if (camera)
        camera->targetTexture = nullptr;
    if (texture)
        Object::Destroy(texture);
}
