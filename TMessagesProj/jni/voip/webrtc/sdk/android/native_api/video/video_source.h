/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_NATIVE_API_VIDEO_VIDEO_SOURCE_H_
#define SDK_ANDROID_NATIVE_API_VIDEO_VIDEO_SOURCE_H_

#include <jni.h>

#include "api/media_stream_interface.h"
#include "rtc_base/thread.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"

namespace webrtc {

class JavaVideoTrackSourceInterface : public VideoTrackSourceInterface {
 public:

  virtual ScopedJavaLocalRef<jobject> GetJavaVideoCapturerObserver(
      JNIEnv* env) = 0;
};

rtc::scoped_refptr<JavaVideoTrackSourceInterface> CreateJavaVideoSource(
    JNIEnv* env,
    rtc::Thread* signaling_thread,
    bool is_screencast,
    bool align_timestamps);

}

#endif
