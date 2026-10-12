// It is licensed under GNU GPL v. 2 or later.

#include <jni.h>

#include <android/log.h>

#include <mutex>
#include <string>
#include <unordered_map>

#include "art_symbols.h"
#include "lsplant.hpp"
#include "shadowhook.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "FGHook", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "FGHook", __VA_ARGS__)

namespace {

std::mutex g_stub_lock;

std::unordered_map<void *, void *> g_stubs;

void *g_art = nullptr;
jclass g_method_class = nullptr;
jmethodID g_method_invoke = nullptr;

bool g_ready = false;

std::string g_error;

void *InlineHook(void *target, void *replacement) {
    void *original = nullptr;

    void *stub = shadowhook_hook_sym_addr(target, replacement, &original);
    if (stub == nullptr) {
        stub = shadowhook_hook_func_addr(target, replacement, &original);
    }
    if (stub == nullptr) {
        const int error = shadowhook_get_errno();
        LOGE("inline hook %p: %d %s", target, error, shadowhook_to_errmsg(error));
        return nullptr;
    }
    std::lock_guard<std::mutex> guard(g_stub_lock);
    g_stubs[target] = stub;
    return original;
}

bool InlineUnhook(void *target) {
    void *stub = nullptr;
    {
        std::lock_guard<std::mutex> guard(g_stub_lock);
        auto found = g_stubs.find(target);
        if (found == g_stubs.end()) return false;
        stub = found->second;
        g_stubs.erase(found);
    }
    return shadowhook_unhook(stub) == 0;
}

void *ResolveSymbol(std::string_view name) {
    if (g_art == nullptr) return nullptr;
    const std::string symbol(name);
    return shadowhook_dlsym(g_art, symbol.c_str());
}

void *ResolvePrefix(std::string_view prefix) {
    return fghook::FindArtSymbolByPrefix(prefix);
}

bool Initialize(JNIEnv *env) {
    const int error = shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false);
    if (error != SHADOWHOOK_ERRNO_OK) {
        g_error = std::string("shadowhook: ") + shadowhook_to_errmsg(error);
        return false;
    }
    g_art = shadowhook_dlopen("libart.so");
    if (g_art == nullptr) {
        g_error = "libart.so не открылась";
        return false;
    }

    jclass method = env->FindClass("java/lang/reflect/Method");
    if (method == nullptr) {
        env->ExceptionClear();
        g_error = "нет класса Method";
        return false;
    }
    g_method_class = static_cast<jclass>(env->NewGlobalRef(method));
    g_method_invoke = env->GetMethodID(method, "invoke",
                                       "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");
    env->DeleteLocalRef(method);
    if (g_method_invoke == nullptr) {
        env->ExceptionClear();
        g_error = "нет Method.invoke";
        return false;
    }

    lsplant::InitInfo info{
        .inline_hooker = InlineHook,
        .inline_unhooker = InlineUnhook,
        .art_symbol_resolver = ResolveSymbol,
        .art_symbol_prefix_resolver = ResolvePrefix,
        .generated_class_name = "FGHooker_",
        .generated_source_name = "FGHook",
    };
    const bool ready = lsplant::Init(env, info);

    fghook::ReleaseArtSymbols();
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    if (!ready) {
        g_error = "LSPlant не нашёл нужные функции ART";
    }
    return ready;
}

}

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    JNIEnv *env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        g_error = "нет JNIEnv";
        return JNI_VERSION_1_6;
    }
    g_ready = Initialize(env);
    LOGI("движок перехватов %s", g_ready ? "готов" : g_error.c_str());
    return JNI_VERSION_1_6;
}

JNIEXPORT jstring JNICALL
Java_com_th3nekit_finegram_hook_ArtHook_nativeInitError(JNIEnv *env, jclass) {
    if (g_ready) return nullptr;
    return env->NewStringUTF(g_error.empty() ? "движок не запускался" : g_error.c_str());
}

JNIEXPORT jobject JNICALL
Java_com_th3nekit_finegram_hook_ArtHook_nativeHook(JNIEnv *env, jclass, jobject target,
                                                    jobject hooker, jobject callback) {
    return lsplant::Hook(env, target, hooker, callback);
}

JNIEXPORT jboolean JNICALL
Java_com_th3nekit_finegram_hook_ArtHook_nativeUnhook(JNIEnv *env, jclass, jobject target) {
    return lsplant::UnHook(env, target) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_th3nekit_finegram_hook_ArtHook_nativeDeoptimize(JNIEnv *env, jclass, jobject method) {
    return lsplant::Deoptimize(env, method) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobject JNICALL
Java_com_th3nekit_finegram_hook_ArtHook_nativeInvoke(JNIEnv *env, jclass, jobject backup,
                                                      jobject receiver, jobjectArray args) {
    return env->CallNonvirtualObjectMethod(backup, g_method_class, g_method_invoke, receiver, args);
}

}
