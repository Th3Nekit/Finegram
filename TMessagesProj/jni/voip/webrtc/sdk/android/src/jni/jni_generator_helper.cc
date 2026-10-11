/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/jni_generator_helper.h"

#include "sdk/android/native_api/jni/class_loader.h"

namespace webrtc {

jclass LazyGetClass(JNIEnv* env,
                    const char* class_name,
                    std::atomic<jclass>* atomic_class_id) {
  const jclass value = std::atomic_load(atomic_class_id);
  if (value)
    return value;
  webrtc::ScopedJavaGlobalRef<jclass> clazz(webrtc::GetClass(env, class_name));
  RTC_CHECK(!clazz.is_null()) << class_name;
  jclass cas_result = nullptr;
  if (std::atomic_compare_exchange_strong(atomic_class_id, &cas_result,
                                          clazz.obj())) {

    return clazz.Release();
  } else {

    return cas_result;
  }
}

template <MethodID::Type type>
jmethodID MethodID::LazyGet(JNIEnv* env,
                            jclass clazz,
                            const char* method_name,
                            const char* jni_signature,
                            std::atomic<jmethodID>* atomic_method_id) {
  const jmethodID value = std::atomic_load(atomic_method_id);
  if (value)
    return value;
  auto get_method_ptr = type == MethodID::TYPE_STATIC
                            ? &JNIEnv::GetStaticMethodID
                            : &JNIEnv::GetMethodID;
  jmethodID id = (env->*get_method_ptr)(clazz, method_name, jni_signature);
  CHECK_EXCEPTION(env) << "error during GetMethodID: " << method_name << ", "
                       << jni_signature;
  RTC_CHECK(id) << method_name << ", " << jni_signature;
  std::atomic_store(atomic_method_id, id);
  return id;
}

template jmethodID MethodID::LazyGet<MethodID::TYPE_STATIC>(
    JNIEnv* env,
    jclass clazz,
    const char* method_name,
    const char* jni_signature,
    std::atomic<jmethodID>* atomic_method_id);

template jmethodID MethodID::LazyGet<MethodID::TYPE_INSTANCE>(
    JNIEnv* env,
    jclass clazz,
    const char* method_name,
    const char* jni_signature,
    std::atomic<jmethodID>* atomic_method_id);

}
