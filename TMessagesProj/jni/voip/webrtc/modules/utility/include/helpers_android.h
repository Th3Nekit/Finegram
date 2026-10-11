/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_UTILITY_INCLUDE_HELPERS_ANDROID_H_
#define MODULES_UTILITY_INCLUDE_HELPERS_ANDROID_H_

#include <jni.h>

#include <string>

#include "rtc_base/system/arch.h"

#define CHECK_EXCEPTION(jni)        \
  RTC_CHECK(!jni->ExceptionCheck()) \
      << (jni->ExceptionDescribe(), jni->ExceptionClear(), "")

#if defined(WEBRTC_ARCH_X86)

#define JNI_FUNCTION_ALIGN __attribute__((force_align_arg_pointer))
#else
#define JNI_FUNCTION_ALIGN
#endif

namespace webrtc {

JNIEnv* GetEnv(JavaVM* jvm);

jlong PointerTojlong(void* ptr);

jmethodID GetMethodID(JNIEnv* jni,
                      jclass c,
                      const char* name,
                      const char* signature);

jmethodID GetStaticMethodID(JNIEnv* jni,
                            jclass c,
                            const char* name,
                            const char* signature);

jclass FindClass(JNIEnv* jni, const char* name);

jobject NewGlobalRef(JNIEnv* jni, jobject o);

void DeleteGlobalRef(JNIEnv* jni, jobject o);

class AttachThreadScoped {
 public:
  explicit AttachThreadScoped(JavaVM* jvm);
  ~AttachThreadScoped();
  JNIEnv* env();

 private:
  bool attached_;
  JavaVM* jvm_;
  JNIEnv* env_;
};

}

#endif
