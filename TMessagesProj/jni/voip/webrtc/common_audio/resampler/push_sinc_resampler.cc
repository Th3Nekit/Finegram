/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/resampler/push_sinc_resampler.h"

#include <cstring>

#include "common_audio/include/audio_util.h"
#include "rtc_base/checks.h"

namespace webrtc {

PushSincResampler::PushSincResampler(size_t source_frames,
                                     size_t destination_frames)
    : resampler_(new SincResampler(source_frames * 1.0 / destination_frames,
                                   source_frames,
                                   this)),
      source_ptr_(nullptr),
      source_ptr_int_(nullptr),
      destination_frames_(destination_frames),
      first_pass_(true),
      source_available_(0) {}

PushSincResampler::~PushSincResampler() {}

size_t PushSincResampler::Resample(const int16_t* source,
                                   size_t source_length,
                                   int16_t* destination,
                                   size_t destination_capacity) {
  if (!float_buffer_.get())
    float_buffer_.reset(new float[destination_frames_]);

  source_ptr_int_ = source;

  Resample(nullptr, source_length, float_buffer_.get(), destination_frames_);
  FloatS16ToS16(float_buffer_.get(), destination_frames_, destination);
  source_ptr_int_ = nullptr;
  return destination_frames_;
}

size_t PushSincResampler::Resample(const float* source,
                                   size_t source_length,
                                   float* destination,
                                   size_t destination_capacity) {
  RTC_CHECK_EQ(source_length, resampler_->request_frames());
  RTC_CHECK_GE(destination_capacity, destination_frames_);

  source_ptr_ = source;
  source_available_ = source_length;

  if (first_pass_)
    resampler_->Resample(resampler_->ChunkSize(), destination);

  resampler_->Resample(destination_frames_, destination);
  source_ptr_ = nullptr;
  return destination_frames_;
}

void PushSincResampler::Run(size_t frames, float* destination) {

  RTC_CHECK_EQ(source_available_, frames);

  if (first_pass_) {

    std::memset(destination, 0, frames * sizeof(*destination));
    first_pass_ = false;
    return;
  }

  if (source_ptr_) {
    std::memcpy(destination, source_ptr_, frames * sizeof(*destination));
  } else {
    for (size_t i = 0; i < frames; ++i)
      destination[i] = static_cast<float>(source_ptr_int_[i]);
  }
  source_available_ -= frames;
}

}
