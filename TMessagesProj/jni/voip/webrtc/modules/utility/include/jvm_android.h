/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_UTILITY_INCLUDE_JVM_ANDROID_H_
#define MODULES_UTILITY_INCLUDE_JVM_ANDROID_H_

#include <jni.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <thread>

#include "api/sequence_checker.h"
#include "modules/utility/include/helpers_android.h"

namespace webrtc {

class JvmThreadConnector {
 public:
  JvmThreadConnector();
  ~JvmThreadConnector();

 private:
  SequenceChecker thread_checker_;
};

class GlobalRef {
 public:
  GlobalRef(JNIEnv* jni, jobject object);
  ~GlobalRef();

  jboolean CallBooleanMethod(jmethodID methodID, ...);
  jint CallIntMethod(jmethodID methodID, ...);
  void CallVoidMethod(jmethodID methodID, ...);

 private:
  JNIEnv* const jni_;
  const jobject j_object_;
};

class JavaClass {
 public:
  JavaClass(JNIEnv* jni, jclass clazz) : jni_(jni), j_class_(clazz) {}
  ~JavaClass() {}

  jmethodID GetMethodId(const char* name, const char* signature);
  jmethodID GetStaticMethodId(const char* name, const char* signature);
  jobject CallStaticObjectMethod(jmethodID methodID, ...);
  jint CallStaticIntMethod(jmethodID methodID, ...);

 protected:
  JNIEnv* const jni_;
  jclass const j_class_;
};

class NativeRegistration : public JavaClass {
 public:
  NativeRegistration(JNIEnv* jni, jclass clazz);
  ~NativeRegistration();

  std::unique_ptr<GlobalRef> NewObject(const char* name,
                                       const char* signature,
                                       ...);

 private:
  JNIEnv* const jni_;
};

class JNIEnvironment {
 public:
  explicit JNIEnvironment(JNIEnv* jni);
  ~JNIEnvironment();

  std::unique_ptr<NativeRegistration> RegisterNatives(
      const char* name,
      const JNINativeMethod* methods,
      int num_methods);

  std::string JavaToStdString(const jstring& j_string);

 private:
  SequenceChecker thread_checker_;
  JNIEnv* const jni_;
};

class JVM {
 public:

  static void Initialize(JavaVM* jvm);

  static void Initialize(JavaVM* jvm, jobject context);

  static void Uninitialize();

  static JVM* GetInstance();

  std::unique_ptr<JNIEnvironment> environment();

  JavaClass GetClass(const char* name);

  bool attachThread(std::thread::id id);
  bool detachThread(std::thread::id id);

  JavaVM* jvm() const { return jvm_; }

 protected:
  JVM(JavaVM* jvm);
  ~JVM();

 private:
  JNIEnv* jni() const { return GetEnv(jvm_); }

  SequenceChecker thread_checker_;
  JavaVM* const jvm_;
  std::unordered_map<std::thread::id, int> threadAttachCounts;
};

}

#endif
