/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Originally this class is from Chromium.
// https://cs.chromium.org/chromium/src/base/android/jni_int_wrapper.h.

#ifndef SDK_ANDROID_NATIVE_API_JNI_JNI_INT_WRAPPER_H_
#define SDK_ANDROID_NATIVE_API_JNI_JNI_INT_WRAPPER_H_

#include <jni.h>

#include <cstdint>

#ifdef NDEBUG

typedef jint JniIntWrapper;

inline jint as_jint(JniIntWrapper wrapper) {
  return wrapper;
}

#else

class JniIntWrapper {
 public:
  JniIntWrapper() : i_(0) {}
  JniIntWrapper(int32_t i) : i_(i) {}
  explicit JniIntWrapper(const JniIntWrapper& ji) : i_(ji.i_) {}

  jint as_jint() const { return i_; }

  JniIntWrapper(uint32_t) = delete;
  JniIntWrapper(uint64_t) = delete;
  JniIntWrapper(int64_t) = delete;

 private:
  const jint i_;
};

inline jint as_jint(const JniIntWrapper& wrapper) {
  return wrapper.as_jint();
}

#endif

#endif
