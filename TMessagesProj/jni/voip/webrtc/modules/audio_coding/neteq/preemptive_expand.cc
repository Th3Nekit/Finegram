/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/preemptive_expand.h"

#include <algorithm>

#include "api/array_view.h"
#include "modules/audio_coding/neteq/audio_multi_vector.h"
#include "modules/audio_coding/neteq/time_stretch.h"

namespace webrtc {

PreemptiveExpand::ReturnCodes PreemptiveExpand::Process(
    const int16_t* input,
    size_t input_length,
    size_t old_data_length,
    AudioMultiVector* output,
    size_t* length_change_samples) {
  old_data_length_per_channel_ = old_data_length;

  static const size_t k15ms = 120;
  if (num_channels_ == 0 ||
      input_length / num_channels_ < (2 * k15ms - 1) * fs_mult_ ||
      old_data_length >= input_length / num_channels_ - overlap_samples_) {

    output->PushBackInterleaved(
        rtc::ArrayView<const int16_t>(input, input_length));
    return kError;
  }
  const bool kFastMode = false;
  return TimeStretch::Process(input, input_length, kFastMode, output,
                              length_change_samples);
}

void PreemptiveExpand::SetParametersForPassiveSpeech(size_t len,
                                                     int16_t* best_correlation,
                                                     size_t* peak_index) const {

  *best_correlation = 0;

  *peak_index = std::min(*peak_index, len - old_data_length_per_channel_);
}

PreemptiveExpand::ReturnCodes PreemptiveExpand::CheckCriteriaAndStretch(
    const int16_t* input,
    size_t input_length,
    size_t peak_index,
    int16_t best_correlation,
    bool active_speech,
    bool              ,
    AudioMultiVector* output) const {

  size_t fs_mult_120 = static_cast<size_t>(fs_mult_ * 120);

  if (((best_correlation > kCorrelationThreshold) &&
       (old_data_length_per_channel_ <= fs_mult_120)) ||
      !active_speech) {

    size_t unmodified_length =
        std::max(old_data_length_per_channel_, fs_mult_120);

    output->PushBackInterleaved(rtc::ArrayView<const int16_t>(
        input, (unmodified_length + peak_index) * num_channels_));

    AudioMultiVector temp_vector(num_channels_);
    temp_vector.PushBackInterleaved(rtc::ArrayView<const int16_t>(
        &input[(unmodified_length - peak_index) * num_channels_],
        peak_index * num_channels_));

    output->CrossFade(temp_vector, peak_index);

    output->PushBackInterleaved(rtc::ArrayView<const int16_t>(
        &input[unmodified_length * num_channels_],
        input_length - unmodified_length * num_channels_));

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

PreemptiveExpand* PreemptiveExpandFactory::Create(
    int sample_rate_hz,
    size_t num_channels,
    const BackgroundNoise& background_noise,
    size_t overlap_samples) const {
  return new PreemptiveExpand(sample_rate_hz, num_channels, background_noise,
                              overlap_samples);
}

}
