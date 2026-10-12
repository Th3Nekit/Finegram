/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Android's FindClass() is tricky because the app-specific ClassLoader is not
// consulted when there is no app-specific frame on the stack (i.e. when called
// from a thread created from native C++ code). These helper functions provide a
// workaround for this.
// http://developer.android.com/training/articles/perf-jni.html#faq_FindClass

#ifndef SDK_ANDROID_NATIVE_API_JNI_CLASS_LOADER_H_
#define SDK_ANDROID_NATIVE_API_JNI_CLASS_LOADER_H_

#include <jni.h>

#include "sdk/android/native_api/jni/scoped_java_ref.h"

namespace webrtc {

void InitClassLoader(JNIEnv* env);

ScopedJavaLocalRef<jclass> GetClass(JNIEnv* env, const char* name);

}

#endif
