#include "java.hpp"

#include "assets.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "main.hpp"
#include "metacore/shared/java.hpp"
#include "scotland2/shared/modloader.h"

static jclass mainClass = nullptr;
static jobject instance = nullptr;

static void Java_log(JNIEnv* env, jobject, int level, jstring string) {
    auto chars = env->GetStringUTFChars(string, 0);
    Paper::Logger::vfmtLog("{}", (Paper::LogLevel) level, Paper::sl::current(), MOD_ID, fmt::make_format_args(chars));
    env->ReleaseStringUTFChars(string, chars);
}

static JNINativeMethod const methods[] = {
    {"log", "(ILjava/lang/String;)V", (void*) &Java_log},
    {"nativeGetDeclaredMethod", "(Ljava/lang/Object;Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;", (void*) Java_getDeclaredMethod},
};

bool Hollywood::LoadClassAsset() {
    if (instance)
        return true;

    JNIEnv* env = MetaCore::Java::GetEnv();
    if (!env)
        return false;
    MetaCore::Java::JNIFrame frame(env, 8);

    mainClass = MetaCore::Java::LoadClass(env, "com.metalit.hollywood.MainClass", IncludedAssets::classes_dex);

    if (env->RegisterNatives(mainClass, methods, sizeof(methods) / sizeof(methods[0])) != 0)
        logger.error("Failed to register natives");

    instance = env->NewGlobalRef(MetaCore::Java::NewObject(env, mainClass, "()V"));

    logger.info("Finished loading MainClass");
    return true;
}

void Hollywood::setScreenOn(bool value) {
    if (!instance)
        return;

    JNIEnv* env = MetaCore::Java::GetEnv();
    if (!env)
        return;
    MetaCore::Java::JNIFrame frame(env, 4);

    MetaCore::Java::RunMethod(env, instance, {"setScreenOn", "(Z)V"}, value);
}

void Hollywood::setMute(bool value) {
    if (!instance)
        return;

    JNIEnv* env = MetaCore::Java::GetEnv();
    if (!env)
        return;
    MetaCore::Java::JNIFrame frame(env, 4);

    MetaCore::Java::RunMethod(env, instance, {"setMute", "(Z)V"}, value);
}

void Hollywood::sendSurfaceCapture(ANativeWindow* surface, float fovAdjustment, int selectedEye, int frameRate, bool showInhibitedLayers) {
    if (!instance)
        return;

    JNIEnv* env = MetaCore::Java::GetEnv();
    if (!env)
        return;
    MetaCore::Java::JNIFrame frame(env, 4);

    jobject javaSurface = surface ? ANativeWindow_toSurface(env, surface) : nullptr;
    MetaCore::Java::RunMethod(
        env, instance, {"sendSurfaceCapture", "(Landroid/view/Surface;FIIZ)V"}, javaSurface, fovAdjustment, selectedEye, frameRate, showInhibitedLayers
    );
}
