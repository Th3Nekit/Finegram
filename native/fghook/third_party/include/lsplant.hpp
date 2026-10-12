#pragma once

#include <jni.h>

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

namespace lsplant {

inline namespace v2 {

struct InitInfo {

    using InlineHookFunType = std::function<void *(void *target, void *hooker)>;

    using InlineUnhookFunType = std::function<bool(void *func)>;

    using ArtSymbolResolver = std::function<void *(std::string_view symbol_name)>;

    using ArtSymbolPrefixResolver = std::function<void *(std::string_view symbol_prefix)>;

    using MemoryAllocator = std::function<void *(std::span<const uint8_t> data)>;

    using MemoryRecycler = std::function<void(void *memory)>;

    InlineHookFunType inline_hooker;

    InlineUnhookFunType inline_unhooker;

    ArtSymbolResolver art_symbol_resolver;

    ArtSymbolPrefixResolver art_symbol_prefix_resolver;

    std::string_view generated_class_name = "LSPHooker_";

    std::string_view generated_source_name = "LSP";

    std::string_view generated_field_name = "hooker";

    std::string_view generated_method_name = "{target}";

    MemoryAllocator executable_memory_allocator;

    MemoryRecycler executable_memory_recycler;
};

[[nodiscard, maybe_unused, gnu::visibility("default")]] bool Init(JNIEnv *env,
                                                                  const InitInfo &info);

[[nodiscard, maybe_unused, gnu::visibility("default")]] jobject Hook(JNIEnv *env,
                                                                     jobject target_method,
                                                                     jobject hooker_object,
                                                                     jobject callback_method);

[[nodiscard, maybe_unused, gnu::visibility("default")]] bool UnHook(JNIEnv *env,
                                                                    jobject target_method);

[[nodiscard, maybe_unused, gnu::visibility("default")]] bool IsHooked(JNIEnv *env, jobject method);

[[nodiscard, maybe_unused, gnu::visibility("default")]] bool Deoptimize(JNIEnv *env,
                                                                        jobject method);

[[nodiscard, maybe_unused, gnu::visibility("default")]] void *GetNativeFunction(JNIEnv *env,
                                                                                jobject method);

[[nodiscard, maybe_unused, gnu::visibility("default")]] bool MakeClassInheritable(JNIEnv *env,
                                                                                  jclass target);

[[nodiscard, maybe_unused, gnu::visibility("default")]] bool MakeDexFileTrusted(JNIEnv *env,
                                                                                jobject cookie);
}
}
