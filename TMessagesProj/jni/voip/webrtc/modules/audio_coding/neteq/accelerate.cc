/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/accelerate.h"

#include "api/array_view.h"
#include "modules/audio_coding/neteq/audio_multi_vector.h"

namespace webrtc {

Accelerate::ReturnCodes Accelerate::Process(const int16_t* input,
                                            size_t input_length,
                                            bool fast_accelerate,
                                            AudioMultiVector* output,
                                            size_t* length_change_samples) {

  static const size_t k15ms = 120;
  if (num_channels_ == 0 ||
      input_length / num_channels_ < (2 * k15ms - 1) * fs_mult_) {

    output->PushBackInterleaved(
        rtc::ArrayView<const int16_t>(input, input_length));
    return kError;
  }
  return TimeStretch::Process(input, input_length, fast_accelerate, output,
                              length_change_samples);
}

void Accelerate::SetParametersForPassiveSpeech(size_t        ,
                                               int16_t* best_correlation,
                                               size_t*               ) const {

  *best_correlation = 0;
}

Accelerate::ReturnCodes Accelerate::CheckCriteriaAndStretch(
    const int16_t* input,
    size_t input_length,
    size_t peak_index,
    int16_t best_correlation,
    bool active_speech,
    bool fast_mode,
    AudioMultiVector* output) const {

  const int correlation_threshold = fast_mode ? 8192 : kCorrelationThreshold;
  if ((best_correlation > correlation_threshold) || !active_speech) {

    size_t fs_mult_120 = fs_mult_ * 120;

    if (fast_mode) {

      peak_index = (fs_mult_120 / peak_index) * peak_index;
    }

    RTC_DCHECK_GE(fs_mult_120, peak_index);

    output->PushBackInterleaved(
        rtc::ArrayView<const int16_t>(input, fs_mult_120 * num_channels_));

    AudioMultiVector temp_vector(num_channels_);
    temp_vector.PushBackInterleaved(rtc::ArrayView<const int16_t>(
        &input[fs_mult_120 * num_channels_], peak_index * num_channels_));

    output->CrossFade(temp_vector, peak_index);

    output->PushBackInterleaved(rtc::ArrayView<const int16_t>(
        &input[(fs_mult_120 + peak_index) * num_channels_],
        input_length - (fs_mult_120 + peak_index) * num_channels_));

    if (active_speech) {
      return kSuccess;
    } else {
      return kSuccessLowEnergy;
    }
  } else {

    output->PushBackInterleaved(
        rtc::ArrayView<const int16_t>(input, input_length));
    return kNoStretch;
  }
}

Accelerate* AccelerateFactory::Create(
    int sample_rate_hz,
    size_t num_channels,
    const BackgroundNoise& background_noise) const {
  return new Accelerate(sample_rate_hz, num_channels, background_noise);
}

}
