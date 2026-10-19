// Finegram: движок перехвата методов для плагинов.
// It is licensed under GNU GPL v. 2 or later.
//
// Сам перехват делает LSPlant (LGPL-3.0): он подменяет вход метода заглушкой с той же
// сигнатурой, так что ссылки на объекты в перехваченном вызове видит сборщик мусора.
// Встраивание в функции ART, которое LSPlant нужно для работы, делает shadowhook.
// Здесь — только связка: как найти функции ART и как вызвать Java-сторону.
//
// Движок поднимается в JNI_OnLoad и только там: при запуске LSPlant читает служебные поля
// ART, закрытые для приложений. Проверка смотрит, чей Java-код привёл к вызову, а во время
// загрузки библиотеки это код самой системы, и запрет не срабатывает. Из обычного вызова
// тот же запуск молча не находит поле и заканчивается ничем.

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
// функция ART -> заглушка shadowhook, по ней перехват и снимают
std::unordered_map<void *, void *> g_stubs;

void *g_art = nullptr;
jclass g_method_class = nullptr;
jmethodID g_method_invoke = nullptr;

bool g_ready = false;
// пусто, пока всё хорошо; иначе шаг, на котором движок не поднялся
std::string g_error;

void *InlineHook(void *target, void *replacement) {
    void *original = nullptr;
    // по адресу символа shadowhook сам проверит границы функции; если символа он не
    // знает (функция из упакованной таблицы), ставим просто по адресу
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
    // таблица символов с диска нужна только здесь; держать мегабайты незачем
    fghook::ReleaseArtSymbols();
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    if (!ready) {
        g_error = "LSPlant не нашёл нужные функции ART";
    }
    return ready;
}

}  // namespace

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

/** Причина, по которой движок не поднялся; null — поднялся. */
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

// Вызов сохранённого оригинала.
//
// Оригинал конструктора — тоже Constructor, и newInstance создал бы новый объект вместо
// того, чтобы выполнить тело на уже созданном. Method.invoke в ART принимает любой
// Executable и вызывает метод на переданном объекте, поэтому зовём его невиртуально —
// так его можно применить и к конструктору. Исключение оригинала приходит обычным
// InvocationTargetException.
JNIEXPORT jobject JNICALL
Java_com_th3nekit_finegram_hook_ArtHook_nativeInvoke(JNIEnv *env, jclass, jobject backup,
                                                      jobject receiver, jobjectArray args) {
    return env->CallNonvirtualObjectMethod(backup, g_method_class, g_method_invoke, receiver, args);
}

}  // extern "C"
