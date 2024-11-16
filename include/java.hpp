#pragma once

#include <android/native_window_jni.h>

namespace Hollywood {
    bool LoadClassAsset();
    void setScreenOn(bool value);
    void setMute(bool value);
    void sendSurfaceCapture(ANativeWindow* surface, float fovAdjustment, int selectedEye, int frameRate, bool showInhibitedLayers);
}

extern jobject Java_getDeclaredMethod(JNIEnv* env, jclass interface, jobject clazz, jstring method_name, jobjectArray params);
