/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Originally these classes are from Chromium.
// https://cs.chromium.org/chromium/src/base/android/scoped_java_ref.h.

#ifndef SDK_ANDROID_NATIVE_API_JNI_SCOPED_JAVA_REF_H_
#define SDK_ANDROID_NATIVE_API_JNI_SCOPED_JAVA_REF_H_

#include <jni.h>

#include <utility>

#include "sdk/android/native_api/jni/jvm.h"

namespace webrtc {

template <typename T>
class JavaRef;

template <>
class JavaRef<jobject> {
 public:
  JavaRef(const JavaRef&) = delete;
  JavaRef& operator=(const JavaRef&) = delete;

  jobject obj() const { return obj_; }
  bool is_null() const {

    return obj_ == nullptr;
  }

 protected:
  constexpr JavaRef() : obj_(nullptr) {}
  explicit JavaRef(jobject obj) : obj_(obj) {}
  jobject obj_;
};

template <typename T>
class JavaRef : public JavaRef<jobject> {
 public:
  JavaRef(const JavaRef&) = delete;
  JavaRef& operator=(const JavaRef&) = delete;

  T obj() const { return static_cast<T>(obj_); }

 protected:
  JavaRef() : JavaRef<jobject>(nullptr) {}
  explicit JavaRef(T obj) : JavaRef<jobject>(obj) {}
};

template <typename T>
class JavaParamRef : public JavaRef<T> {
 public:

  explicit JavaParamRef(T obj) : JavaRef<T>(obj) {}
  JavaParamRef(JNIEnv*, T obj) : JavaRef<T>(obj) {}

  JavaParamRef(const JavaParamRef&) = delete;
  JavaParamRef& operator=(const JavaParamRef&) = delete;
};

template <typename T>
class ScopedJavaLocalRef : public JavaRef<T> {
 public:
  ScopedJavaLocalRef() = default;
  ScopedJavaLocalRef(std::nullptr_t) {}

  ScopedJavaLocalRef(JNIEnv* env, const JavaRef<T>& other) : env_(env) {
    Reset(other.obj(), OwnershipPolicy::RETAIN);
  }

  template <typename G>
  ScopedJavaLocalRef(ScopedJavaLocalRef<G>&& other) : env_(other.env()) {
    Reset(other.Release(), OwnershipPolicy::ADOPT);
  }
  ScopedJavaLocalRef(const ScopedJavaLocalRef& other) : env_(other.env_) {
    Reset(other.obj(), OwnershipPolicy::RETAIN);
  }

  ScopedJavaLocalRef(JNIEnv* env, T obj) : JavaRef<T>(obj), env_(env) {}

  ~ScopedJavaLocalRef() {
    if (obj_ != nullptr)
      env_->DeleteLocalRef(obj_);
  }

  void operator=(const ScopedJavaLocalRef& other) {
    Reset(other.obj(), OwnershipPolicy::RETAIN);
  }
  void operator=(ScopedJavaLocalRef&& other) {
    Reset(other.Release(), OwnershipPolicy::ADOPT);
  }

  T Release() {
    T obj = static_cast<T>(obj_);
    obj_ = nullptr;
    return obj;
  }

  JNIEnv* env() const { return env_; }

 private:
  using JavaRef<T>::obj_;

  enum OwnershipPolicy {

    ADOPT,

    RETAIN
  };

  void Reset(T obj, OwnershipPolicy policy) {
    if (obj_ != nullptr)
      env_->DeleteLocalRef(obj_);
    obj_ = (obj != nullptr && policy == OwnershipPolicy::RETAIN)
               ? env_->NewLocalRef(obj)
               : obj;
  }

  JNIEnv* const env_ = AttachCurrentThreadIfNeeded();
};

template <typename T>
class ScopedJavaGlobalRef : public JavaRef<T> {
 public:
  using JavaRef<T>::obj_;

  ScopedJavaGlobalRef() = default;
  explicit constexpr ScopedJavaGlobalRef(std::nullptr_t) {}
  ScopedJavaGlobalRef(JNIEnv* env, const JavaRef<T>& other)
      : JavaRef<T>(static_cast<T>(env->NewGlobalRef(other.obj()))) {}
  explicit ScopedJavaGlobalRef(const ScopedJavaLocalRef<T>& other)
      : ScopedJavaGlobalRef(other.env(), other) {}
  ScopedJavaGlobalRef(ScopedJavaGlobalRef&& other)
      : JavaRef<T>(other.Release()) {}

  ~ScopedJavaGlobalRef() {
    if (obj_ != nullptr)
      AttachCurrentThreadIfNeeded()->DeleteGlobalRef(obj_);
  }

  ScopedJavaGlobalRef(const ScopedJavaGlobalRef&) = delete;
  ScopedJavaGlobalRef& operator=(const ScopedJavaGlobalRef&) = delete;

  void operator=(const JavaRef<T>& other) {
    JNIEnv* env = AttachCurrentThreadIfNeeded();
    if (obj_ != nullptr) {
      env->DeleteGlobalRef(obj_);
    }
    obj_ = other.is_null() ? nullptr : env->NewGlobalRef(other.obj());
  }

  void operator=(std::nullptr_t) {
    if (obj_ != nullptr) {
      AttachCurrentThreadIfNeeded()->DeleteGlobalRef(obj_);
    }
    obj_ = nullptr;
  }

  T Release() {
    T obj = static_cast<T>(obj_);
    obj_ = nullptr;
    return obj;
  }
};

template <typename T>
inline ScopedJavaLocalRef<T> static_java_ref_cast(JNIEnv* env,
                                                  JavaRef<jobject> const& ref) {
  ScopedJavaLocalRef<jobject> owned_ref(env, ref);
  return ScopedJavaLocalRef<T>(env, static_cast<T>(owned_ref.Release()));
}

}

#endif
