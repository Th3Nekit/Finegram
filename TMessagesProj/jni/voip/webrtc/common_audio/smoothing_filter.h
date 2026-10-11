/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_SMOOTHING_FILTER_H_
#define COMMON_AUDIO_SMOOTHING_FILTER_H_

#include <stdint.h>

#include "absl/types/optional.h"

namespace webrtc {

class SmoothingFilter {
 public:
  virtual ~SmoothingFilter() = default;
  virtual void AddSample(float sample) = 0;
  virtual absl::optional<float> GetAverage() = 0;
  virtual bool SetTimeConstantMs(int time_constant_ms) = 0;
};

class SmoothingFilterImpl final : public SmoothingFilter {
 public:

  explicit SmoothingFilterImpl(int init_time_ms);

  SmoothingFilterImpl() = delete;
  SmoothingFilterImpl(const SmoothingFilterImpl&) = delete;
  SmoothingFilterImpl& operator=(const SmoothingFilterImpl&) = delete;

  ~SmoothingFilterImpl() override;

  void AddSample(float sample) override;
  absl::optional<float> GetAverage() override;
  bool SetTimeConstantMs(int time_constant_ms) override;

  float alpha() const { return alpha_; }

 private:
  void UpdateAlpha(int time_constant_ms);
  void ExtrapolateLastSample(int64_t time_ms);

  const int init_time_ms_;
  const float init_factor_;
  const float init_const_;

  absl::optional<int64_t> init_end_time_ms_;
  float last_sample_;
  float alpha_;
  float state_;
  int64_t last_state_time_ms_;
};

}

#endif
