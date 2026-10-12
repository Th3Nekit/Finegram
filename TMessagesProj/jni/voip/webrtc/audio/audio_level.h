/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_AUDIO_LEVEL_H_
#define AUDIO_AUDIO_LEVEL_H_

#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class AudioFrame;
namespace voe {

class AudioLevel {
 public:
  AudioLevel();
  ~AudioLevel();
  void Reset();

  int16_t LevelFullRange() const;
  void ResetLevelFullRange();

  double TotalEnergy() const;
  double TotalDuration() const;

  void ComputeLevel(const AudioFrame& audioFrame, double duration);

 private:
  enum { kUpdateFrequency = 10 };

  mutable Mutex mutex_;

  int16_t abs_max_ RTC_GUARDED_BY(mutex_);
  int16_t count_ RTC_GUARDED_BY(mutex_);
  int16_t current_level_full_range_ RTC_GUARDED_BY(mutex_);

  double total_energy_ RTC_GUARDED_BY(mutex_) = 0.0;
  double total_duration_ RTC_GUARDED_BY(mutex_) = 0.0;
};

}
}

#endif
