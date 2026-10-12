/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TIME_STRETCH_H_
#define MODULES_AUDIO_CODING_NETEQ_TIME_STRETCH_H_

#include <string.h>

#include "modules/audio_coding/neteq/audio_multi_vector.h"

namespace webrtc {

class BackgroundNoise;

class TimeStretch {
 public:
  enum ReturnCodes {
    kSuccess = 0,
    kSuccessLowEnergy = 1,
    kNoStretch = 2,
    kError = -1
  };

  TimeStretch(int sample_rate_hz,
              size_t num_channels,
              const BackgroundNoise& background_noise)
      : sample_rate_hz_(sample_rate_hz),
        fs_mult_(sample_rate_hz / 8000),
        num_channels_(num_channels),
        background_noise_(background_noise),
        max_input_value_(0) {
    RTC_DCHECK(sample_rate_hz_ == 8000 || sample_rate_hz_ == 16000 ||
               sample_rate_hz_ == 32000 || sample_rate_hz_ == 48000);
    RTC_DCHECK_GT(num_channels_, 0);
    memset(auto_correlation_, 0, sizeof(auto_correlation_));
  }

  virtual ~TimeStretch() {}

  TimeStretch(const TimeStretch&) = delete;
  TimeStretch& operator=(const TimeStretch&) = delete;

  ReturnCodes Process(const int16_t* input,
                      size_t input_len,
                      bool fast_mode,
                      AudioMultiVector* output,
                      size_t* length_change_samples);

 protected:

  virtual void SetParametersForPassiveSpeech(size_t input_length,
                                             int16_t* best_correlation,
                                             size_t* peak_index) const = 0;

  virtual ReturnCodes CheckCriteriaAndStretch(
      const int16_t* input,
      size_t input_length,
      size_t peak_index,
      int16_t best_correlation,
      bool active_speech,
      bool fast_mode,
      AudioMultiVector* output) const = 0;

  static const size_t kCorrelationLen = 50;
  static const size_t kLogCorrelationLen = 6;
  static const size_t kMinLag = 10;
  static const size_t kMaxLag = 60;
  static const size_t kDownsampledLen = kCorrelationLen + kMaxLag;
  static const int kCorrelationThreshold = 14746;
  static constexpr size_t kRefChannel = 0;

  const int sample_rate_hz_;
  const int fs_mult_;
  const size_t num_channels_;
  const BackgroundNoise& background_noise_;
  int16_t max_input_value_;
  int16_t downsampled_input_[kDownsampledLen];

  int16_t auto_correlation_[kCorrelationLen + 1];

 private:

  void AutoCorrelation();

  bool SpeechDetection(int32_t vec1_energy,
                       int32_t vec2_energy,
                       size_t peak_index,
                       int scaling) const;
};

}
#endif
