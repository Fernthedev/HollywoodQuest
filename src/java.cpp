#include "java.hpp"

#include "assets.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "main.hpp"
#include "scotland2/shared/modloader.h"

static jclass mainClass = nullptr;
static jobject instance = nullptr;

static JNIEnv* getEnv() {
    static JNIEnv* env;
    if (!env)
        modloader_jvm->GetEnv((void**) &env, JNI_VERSION_1_6);

    if (!env) {
        logger.error("Failed to get JNI env");
        return nullptr;
    }
    env->ExceptionClear();
    return env;
}

struct JNIFrame {
    JNIFrame(JNIEnv* env, int size) : env(env) { env->PushLocalFrame(size); }
    ~JNIFrame() { env->PopLocalFrame(nullptr); }

   private:
    JNIEnv* env;
};

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

    JNIEnv* env = getEnv();
    if (!env)
        return false;

    JNIFrame frame(env, 16);

    auto dexBuffer = env->NewDirectByteBuffer((void*) IncludedAssets::classes_dex.data, IncludedAssets::classes_dex.len - 1);

    auto getClassLoader = env->GetMethodID(env->FindClass("java/lang/Class"), "getClassLoader", "()Ljava/lang/ClassLoader;");
    if (!getClassLoader) {
        logger.error("Can't find getClassLoader method");
        return false;
    }
    // not sure if necessary to run this on the UnityPlayer class
    auto baseClassLoader = env->CallObjectMethod(env->FindClass("com/unity3d/player/UnityPlayer"), getClassLoader);

    jobject classLoader;
    {
        jclass classLoaderClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
        jmethodID classLoaderInit = env->GetMethodID(classLoaderClass, "<init>", "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
        classLoader = env->NewObject(classLoaderClass, classLoaderInit, dexBuffer, baseClassLoader);
    }

    {
        jmethodID loadClassMethod = env->GetMethodID(env->GetObjectClass(classLoader), "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        auto mainClassObject = env->CallObjectMethod(classLoader, loadClassMethod, env->NewStringUTF("com.metalit.hollywood.MainClass"));
        mainClass = (jclass) env->NewGlobalRef(mainClassObject);
    }
    if (!mainClass) {
        logger.error("Failed to load class");
        return false;
    }

    if (env->RegisterNatives(mainClass, methods, sizeof(methods) / sizeof(methods[0])) != 0)
        logger.error("Failed to register natives");

    {
        jmethodID recorderInit = env->GetMethodID(mainClass, "<init>", "()V");
        auto mainInstance = env->NewObject(mainClass, recorderInit, dexBuffer, baseClassLoader);
        instance = env->NewGlobalRef(mainInstance);
    }

    logger.info("Finished loading MainClass");
    return true;
}

void Hollywood::setScreenOn(bool value) {
    if (!instance)
        return;

    JNIEnv* env = getEnv();
    if (!env)
        return;

    JNIFrame frame(env, 2);

    jmethodID renderMethod = env->GetMethodID(mainClass, "setScreenOn", "(Z)V");
    if (renderMethod)
        env->CallVoidMethod(instance, renderMethod, value);
    else
        logger.error("Failed to find setScreenOn");
}

void Hollywood::setMute(bool value) {
    if (!instance)
        return;

    JNIEnv* env = getEnv();
    if (!env)
        return;

    JNIFrame frame(env, 2);

    jmethodID renderMethod = env->GetMethodID(mainClass, "setMute", "(Z)V");
    if (renderMethod)
        env->CallVoidMethod(instance, renderMethod, value);
    else
        logger.error("Failed to find setMute");
}

void Hollywood::sendSurfaceCapture(ANativeWindow* surface, float fovAdjustment, int selectedEye, int frameRate, bool showInhibitedLayers) {
    if (!instance)
        return;

    JNIEnv* env = getEnv();
    if (!env)
        return;

    JNIFrame frame(env, 5);

    jobject javaSurface = surface ? ANativeWindow_toSurface(env, surface) : nullptr;
    jmethodID sendMethod = env->GetMethodID(mainClass, "sendSurfaceCapture", "(Landroid/view/Surface;FIIZ)V");
    if (sendMethod)
        env->CallVoidMethod(instance, sendMethod, javaSurface, fovAdjustment, selectedEye, frameRate, showInhibitedLayers);
    else
        logger.error("Failed to find sendSurfaceCapture");
}
