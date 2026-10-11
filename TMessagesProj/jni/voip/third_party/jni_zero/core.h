// Copyright 2023 The Chromium Authors

#ifndef JNI_ZERO_CORE_H_
#define JNI_ZERO_CORE_H_

#include <jni.h>

#include <atomic>
#include <string>

#include "third_party/jni_zero/jni_export.h"
#include "third_party/jni_zero/scoped_java_ref.h"

namespace jni_zero {

JNI_ZERO_COMPONENT_BUILD_EXPORT JNIEnv* AttachCurrentThread();

JNI_ZERO_COMPONENT_BUILD_EXPORT JNIEnv* AttachCurrentThreadWithName(
    const std::string& thread_name);

JNI_ZERO_COMPONENT_BUILD_EXPORT void DetachFromVM();

JNI_ZERO_COMPONENT_BUILD_EXPORT void InitVM(JavaVM* vm);

JNI_ZERO_COMPONENT_BUILD_EXPORT bool IsVMInitialized();

JNI_ZERO_COMPONENT_BUILD_EXPORT JavaVM* GetVM();

JNI_ZERO_COMPONENT_BUILD_EXPORT void DisableJvmForTesting();

JNI_ZERO_COMPONENT_BUILD_EXPORT void SetExceptionHandler(
    void (*callback)(JNIEnv*));

JNI_ZERO_COMPONENT_BUILD_EXPORT bool HasException(JNIEnv* env);

JNI_ZERO_COMPONENT_BUILD_EXPORT bool ClearException(JNIEnv* env);

JNI_ZERO_COMPONENT_BUILD_EXPORT void CheckException(JNIEnv* env);

JNI_ZERO_COMPONENT_BUILD_EXPORT void SetClassResolver(
    jclass (*resolver)(JNIEnv*, const char*, const char*));

JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jclass>
GetClass(JNIEnv* env, const char* class_name, const char* split_name);
JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jclass> GetClass(
    JNIEnv* env,
    const char* class_name);

JNI_ZERO_COMPONENT_BUILD_EXPORT jclass
LazyGetClass(JNIEnv* env,
             const char* class_name,
             const char* split_name,
             std::atomic<jclass>* atomic_class_id);

JNI_ZERO_COMPONENT_BUILD_EXPORT jclass
LazyGetClass(JNIEnv* env,
             const char* class_name,
             std::atomic<jclass>* atomic_class_id);

class JNI_ZERO_COMPONENT_BUILD_EXPORT MethodID {
 public:
  enum Type {
    TYPE_STATIC,
    TYPE_INSTANCE,
  };

  template <Type type>
  static jmethodID Get(JNIEnv* env,
                       jclass clazz,
                       const char* method_name,
                       const char* jni_signature);

  template <Type type>
  static jmethodID LazyGet(JNIEnv* env,
                           jclass clazz,
                           const char* method_name,
                           const char* jni_signature,
                           std::atomic<jmethodID>* atomic_method_id);
};

}

#endif
