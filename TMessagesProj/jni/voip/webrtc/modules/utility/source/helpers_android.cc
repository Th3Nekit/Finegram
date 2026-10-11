/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/utility/include/helpers_android.h"

#include <android/log.h>
#include <pthread.h>
#include <stddef.h>
#include <unistd.h>

#include "rtc_base/checks.h"
#include "rtc_base/platform_thread.h"

#define TAG "HelpersAndroid"
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

namespace webrtc {

JNIEnv* GetEnv(JavaVM* jvm) {
  void* env = NULL;
  jint status = jvm->GetEnv(&env, JNI_VERSION_1_6);
  RTC_CHECK(((env != NULL) && (status == JNI_OK)) ||
            ((env == NULL) && (status == JNI_EDETACHED)))
      << "Unexpected GetEnv return: " << status << ":" << env;
  return reinterpret_cast<JNIEnv*>(env);
}

jlong PointerTojlong(void* ptr) {
  static_assert(sizeof(intptr_t) <= sizeof(jlong),
                "Time to rethink the use of jlongs");

  jlong ret = reinterpret_cast<intptr_t>(ptr);
  RTC_DCHECK(reinterpret_cast<void*>(ret) == ptr);
  return ret;
}

jmethodID GetMethodID(JNIEnv* jni,
                      jclass c,
                      const char* name,
                      const char* signature) {
  jmethodID m = jni->GetMethodID(c, name, signature);
  CHECK_EXCEPTION(jni) << "Error during GetMethodID: " << name << ", "
                       << signature;
  RTC_CHECK(m) << name << ", " << signature;
  return m;
}

jmethodID GetStaticMethodID(JNIEnv* jni,
                            jclass c,
                            const char* name,
                            const char* signature) {
  jmethodID m = jni->GetStaticMethodID(c, name, signature);
  CHECK_EXCEPTION(jni) << "Error during GetStaticMethodID: " << name << ", "
                       << signature;
  RTC_CHECK(m) << name << ", " << signature;
  return m;
}

jclass FindClass(JNIEnv* jni, const char* name) {
  jclass c = jni->FindClass(name);
  CHECK_EXCEPTION(jni) << "Error during FindClass: " << name;
  RTC_CHECK(c) << name;
  return c;
}

jobject NewGlobalRef(JNIEnv* jni, jobject o) {
  jobject ret = jni->NewGlobalRef(o);
  CHECK_EXCEPTION(jni) << "Error during NewGlobalRef";
  RTC_CHECK(ret);
  return ret;
}

void DeleteGlobalRef(JNIEnv* jni, jobject o) {
  jni->DeleteGlobalRef(o);
  CHECK_EXCEPTION(jni) << "Error during DeleteGlobalRef";
}

AttachThreadScoped::AttachThreadScoped(JavaVM* jvm)
    : attached_(false), jvm_(jvm), env_(NULL) {
  env_ = GetEnv(jvm);
  if (!env_) {

    ALOGD("Attaching thread to JVM[tid=%d]", rtc::CurrentThreadId());
    jint res = jvm->AttachCurrentThread(&env_, NULL);
    attached_ = (res == JNI_OK);
    RTC_CHECK(attached_) << "AttachCurrentThread failed: " << res;
  }
}

AttachThreadScoped::~AttachThreadScoped() {
  if (attached_) {
    ALOGD("Detaching thread from JVM[tid=%d]", rtc::CurrentThreadId());
    jint res = jvm_->DetachCurrentThread();
    RTC_CHECK(res == JNI_OK) << "DetachCurrentThread failed: " << res;
    RTC_CHECK(!GetEnv(jvm_));
  }
}

JNIEnv* AttachThreadScoped::env() {
  return env_;
}

}
