/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/audio_level.h"

#include "api/audio/audio_frame.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"

namespace webrtc {
namespace voe {

AudioLevel::AudioLevel()
    : abs_max_(0), count_(0), current_level_full_range_(0) {}

AudioLevel::~AudioLevel() {}

void AudioLevel::Reset() {
  MutexLock lock(&mutex_);
  abs_max_ = 0;
  count_ = 0;
  current_level_full_range_ = 0;
  total_energy_ = 0.0;
  total_duration_ = 0.0;
}

int16_t AudioLevel::LevelFullRange() const {
  MutexLock lock(&mutex_);
  return current_level_full_range_;
}

void AudioLevel::ResetLevelFullRange() {
  MutexLock lock(&mutex_);
  abs_max_ = 0;
  count_ = 0;
  current_level_full_range_ = 0;
}

double AudioLevel::TotalEnergy() const {
  MutexLock lock(&mutex_);
  return total_energy_;
}

double AudioLevel::TotalDuration() const {
  MutexLock lock(&mutex_);
  return total_duration_;
}

void AudioLevel::ComputeLevel(const AudioFrame& audioFrame, double duration) {

  int16_t abs_value =
      audioFrame.muted()
          ? 0
          : WebRtcSpl_MaxAbsValueW16(
                audioFrame.data(),
                audioFrame.samples_per_channel_ * audioFrame.num_channels_);

  MutexLock lock(&mutex_);

  if (abs_value > abs_max_)
    abs_max_ = abs_value;

  if (count_++ == kUpdateFrequency) {
    current_level_full_range_ = abs_max_;

    count_ = 0;

    abs_max_ >>= 2;
  }

  double additional_energy =
      static_cast<double>(current_level_full_range_) / INT16_MAX;
  additional_energy *= additional_energy;
  total_energy_ += additional_energy * duration;
  total_duration_ += duration;
}

}
}
