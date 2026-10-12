/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/jvm.h"

#include <asm/unistd.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <string>

#include "rtc_base/checks.h"

namespace webrtc {
namespace jni {

static JavaVM* g_jvm = nullptr;

static pthread_once_t g_jni_ptr_once = PTHREAD_ONCE_INIT;

static pthread_key_t g_jni_ptr;

JavaVM* GetJVM() {
  RTC_CHECK(g_jvm) << "JNI_OnLoad failed to run?";
  return g_jvm;
}

JNIEnv* GetEnv() {
  void* env = nullptr;
  jint status = g_jvm->GetEnv(&env, JNI_VERSION_1_6);
  RTC_CHECK(((env != nullptr) && (status == JNI_OK)) ||
            ((env == nullptr) && (status == JNI_EDETACHED)))
      << "Unexpected GetEnv return: " << status << ":" << env;
  return reinterpret_cast<JNIEnv*>(env);
}

static void ThreadDestructor(void* prev_jni_ptr) {

  if (!GetEnv())
    return;

  RTC_CHECK(GetEnv() == prev_jni_ptr)
      << "Detaching from another thread: " << prev_jni_ptr << ":" << GetEnv();
  jint status = g_jvm->DetachCurrentThread();
  RTC_CHECK(status == JNI_OK) << "Failed to detach thread: " << status;
  RTC_CHECK(!GetEnv()) << "Detaching was a successful no-op???";
}

static void CreateJNIPtrKey() {
  RTC_CHECK(!pthread_key_create(&g_jni_ptr, &ThreadDestructor))
      << "pthread_key_create";
}

jint InitGlobalJniVariables(JavaVM* jvm) {
  RTC_CHECK(!g_jvm) << "InitGlobalJniVariables!";
  g_jvm = jvm;
  RTC_CHECK(g_jvm) << "InitGlobalJniVariables handed NULL?";

  RTC_CHECK(!pthread_once(&g_jni_ptr_once, &CreateJNIPtrKey)) << "pthread_once";

  JNIEnv* jni = nullptr;
  if (jvm->GetEnv(reinterpret_cast<void**>(&jni), JNI_VERSION_1_6) != JNI_OK)
    return -1;

  return JNI_VERSION_1_6;
}

static std::string GetThreadId() {
  char buf[21];
  RTC_CHECK_LT(snprintf(buf, sizeof(buf), "%ld",
                        static_cast<long>(syscall(__NR_gettid))),
               sizeof(buf))
      << "Thread id is bigger than uint64??";
  return std::string(buf);
}

static std::string GetThreadName() {
  char name[17] = {0};
  if (prctl(PR_GET_NAME, name) != 0)
    return std::string("<noname>");
  return std::string(name);
}

JNIEnv* AttachCurrentThreadIfNeeded() {
  JNIEnv* jni = GetEnv();
  if (jni)
    return jni;
  RTC_CHECK(!pthread_getspecific(g_jni_ptr))
      << "TLS has a JNIEnv* but not attached?";

  std::string name(GetThreadName() + " - " + GetThreadId());
  JavaVMAttachArgs args;
  args.version = JNI_VERSION_1_6;
  args.name = &name[0];
  args.group = nullptr;

#ifdef _JAVASOFT_JNI_H_
  void* env = nullptr;
#else
  JNIEnv* env = nullptr;
#endif
  RTC_CHECK(!g_jvm->AttachCurrentThread(&env, &args))
      << "Failed to attach thread";
  RTC_CHECK(env) << "AttachCurrentThread handed back NULL!";
  jni = reinterpret_cast<JNIEnv*>(env);
  RTC_CHECK(!pthread_setspecific(g_jni_ptr, jni)) << "pthread_setspecific";
  return jni;
}

}
}
