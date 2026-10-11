/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_JVM_H_
#define SDK_ANDROID_SRC_JNI_JVM_H_

#include <jni.h>

namespace webrtc {
namespace jni {

jint InitGlobalJniVariables(JavaVM* jvm);

JNIEnv* GetEnv();

JavaVM* GetJVM();

JNIEnv* AttachCurrentThreadIfNeeded();

}
}

#endif
