/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_OPENSLES_COMMON_H_
#define SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_OPENSLES_COMMON_H_

#include <SLES/OpenSLES.h>
#include <stddef.h>

#include "api/ref_counted_base.h"
#include "api/sequence_checker.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace jni {

const char* GetSLErrorString(size_t code);

SLDataFormat_PCM CreatePCMConfiguration(size_t channels,
                                        int sample_rate,
                                        size_t bits_per_sample);

template <typename SLType, typename SLDerefType>
class ScopedSLObject {
 public:
  ScopedSLObject() : obj_(nullptr) {}

  ~ScopedSLObject() { Reset(); }

  SLType* Receive() {
    RTC_DCHECK(!obj_);
    return &obj_;
  }

  SLDerefType operator->() { return *obj_; }

  SLType Get() const { return obj_; }

  void Reset() {
    if (obj_) {
      (*obj_)->Destroy(obj_);
      obj_ = nullptr;
    }
  }

 private:
  SLType obj_;
};

typedef ScopedSLObject<SLObjectItf, const SLObjectItf_*> ScopedSLObjectItf;

class OpenSLEngineManager
    : public rtc::RefCountedNonVirtual<OpenSLEngineManager> {
 public:
  OpenSLEngineManager();
  ~OpenSLEngineManager() = default;
  SLObjectItf GetOpenSLEngine();

 private:
  SequenceChecker thread_checker_;

  ScopedSLObjectItf engine_object_;
};

}

}

#endif
