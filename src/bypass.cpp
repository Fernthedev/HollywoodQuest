#include <android/log.h>

#include <future>
#include <java.hpp>
#include <main.hpp>

#include "scotland2/shared/modloader.h"

// some weird threading stuff idk I got this from https://github.com/ChickenHook/RestrictionBypass/tree/master

static JNIEnv* attachCurrentThread() {
    JNIEnv* env;

    int res = modloader_jvm->AttachCurrentThread(&env, nullptr);
    logger.debug("Found attached {}", res);
    return env;
}

static void detachCurrentThread() {
    modloader_jvm->DetachCurrentThread();
}

static jobject getDeclaredMethod(jobject clazz, jstring method_name, jobjectArray params) {
    JNIEnv* env = attachCurrentThread();

    jclass clazz_class = env->GetObjectClass(clazz);
    jmethodID get_declared_method_id =
        env->GetMethodID(clazz_class, "getDeclaredMethod", "(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;");

    jobject res = env->CallObjectMethod(clazz, get_declared_method_id, method_name, params);
    jobject global_res = nullptr;
    if (res)
        global_res = env->NewGlobalRef(res);

    detachCurrentThread();
    return global_res;
}

jobject Java_getDeclaredMethod(JNIEnv* env, jclass interface, jobject clazz, jstring method_name, jobjectArray params) {
    auto global_clazz = env->NewGlobalRef(clazz);
    jstring global_method_name = (jstring) env->NewGlobalRef(method_name);
    int arg_length = env->GetArrayLength(params);

    jobjectArray global_params = nullptr;
    if (params != nullptr) {
        for (int i = 0; i < arg_length; i++) {
            jobject element = (jobject) env->GetObjectArrayElement(params, i);
            jobject global_element = env->NewGlobalRef(element);
            env->SetObjectArrayElement(params, i, global_element);
        }
        global_params = (jobjectArray) env->NewGlobalRef(params);
    }

    auto future = std::async(&getDeclaredMethod, global_clazz, global_method_name, global_params);
    return future.get();
}
