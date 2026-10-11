/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
// Do not include this file directly. It's intended to be used only by the JNI
// generation script. We are exporting types in strange namespaces in order to
// be compatible with the generated code targeted for Chromium.

#ifndef SDK_ANDROID_SRC_JNI_JNI_GENERATOR_HELPER_H_
#define SDK_ANDROID_SRC_JNI_JNI_GENERATOR_HELPER_H_

#include <jni.h>

#include <atomic>

#include "rtc_base/checks.h"
#include "sdk/android/native_api/jni/jni_int_wrapper.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"

#define CHECK_CLAZZ(env, jcaller, clazz, ...) RTC_DCHECK(clazz);
#define CHECK_NATIVE_PTR(env, jcaller, native_ptr, method_name, ...) \
  RTC_DCHECK(native_ptr) << method_name;

#define BASE_EXPORT
#define JNI_REGISTRATION_EXPORT __attribute__((visibility("default")))

#if defined(WEBRTC_ARCH_X86)

#define JNI_GENERATOR_EXPORT \
  __attribute__((force_align_arg_pointer)) extern "C" JNIEXPORT JNICALL
#else
#define JNI_GENERATOR_EXPORT extern "C" JNIEXPORT JNICALL
#endif

#define CHECK_EXCEPTION(jni)        \
  RTC_CHECK(!jni->ExceptionCheck()) \
      << (jni->ExceptionDescribe(), jni->ExceptionClear(), "")

namespace webrtc {

jclass LazyGetClass(JNIEnv* env,
                    const char* class_name,
                    std::atomic<jclass>* atomic_class_id);

class MethodID {
 public:
  enum Type {
    TYPE_STATIC,
    TYPE_INSTANCE,
  };

  template <Type type>
  static jmethodID LazyGet(JNIEnv* env,
                           jclass clazz,
                           const char* method_name,
                           const char* jni_signature,
                           std::atomic<jmethodID>* atomic_method_id);
};

}

namespace jni_zero {

using webrtc::JavaParamRef;
using webrtc::JavaRef;
using webrtc::LazyGetClass;
using webrtc::MethodID;
using webrtc::ScopedJavaLocalRef;

inline void CheckException(JNIEnv* env) {
  CHECK_EXCEPTION(env);
}

constexpr uint64_t kJniStackMarkerValue = 0xbdbdef1bebcade1b;

struct BASE_EXPORT JniJavaCallContextUnchecked {
  inline JniJavaCallContextUnchecked() {

#if defined(__arm__) || defined(__aarch64__)

    asm volatile("mov %0, sp" : "=r"(sp));
#else
    sp = 0;
#endif
  }

  template <jni_zero::MethodID::Type type>
  void Init(JNIEnv* env,
            jclass clazz,
            const char* method_name,
            const char* jni_signature,
            std::atomic<jmethodID>* atomic_method_id) {
    env1 = env;

    memcpy(&marker, &kJniStackMarkerValue, sizeof(kJniStackMarkerValue));

    pc = reinterpret_cast<uintptr_t>(__builtin_return_address(0));

    method_id = jni_zero::MethodID::LazyGet<type>(
        env, clazz, method_name, jni_signature, atomic_method_id);
  }

  ~JniJavaCallContextUnchecked() {

    memset(&marker, 0, sizeof(marker));
  }

  uint64_t marker;
  uintptr_t sp;
  uintptr_t pc;

  JNIEnv* env1;
  jmethodID method_id;
};

struct BASE_EXPORT JniJavaCallContextChecked {

  template <jni_zero::MethodID::Type type>
  void Init(JNIEnv* env,
            jclass clazz,
            const char* method_name,
            const char* jni_signature,
            std::atomic<jmethodID>* atomic_method_id) {
    base.Init<type>(env, clazz, method_name, jni_signature, atomic_method_id);

    base.pc = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
  }

  ~JniJavaCallContextChecked() { jni_zero::CheckException(base.env1); }

  JniJavaCallContextUnchecked base;
};

static_assert(sizeof(JniJavaCallContextChecked) ==
                  sizeof(JniJavaCallContextUnchecked),
              "Stack unwinder cannot work with structs of different sizes.");
}

namespace jni_generator {
using jni_zero::JniJavaCallContextChecked;
using jni_zero::JniJavaCallContextUnchecked;
}

namespace base {
namespace android {
using webrtc::JavaParamRef;
using webrtc::JavaRef;
using webrtc::LazyGetClass;
using webrtc::MethodID;
using webrtc::ScopedJavaLocalRef;
}
}
#endif
