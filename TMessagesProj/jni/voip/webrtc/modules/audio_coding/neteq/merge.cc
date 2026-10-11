/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/merge.h"

#include <string.h>

#include <algorithm>
#include <memory>

#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "modules/audio_coding/neteq/audio_multi_vector.h"
#include "modules/audio_coding/neteq/cross_correlation.h"
#include "modules/audio_coding/neteq/dsp_helper.h"
#include "modules/audio_coding/neteq/expand.h"
#include "modules/audio_coding/neteq/sync_buffer.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

Merge::Merge(int fs_hz,
             size_t num_channels,
             Expand* expand,
             SyncBuffer* sync_buffer)
    : fs_hz_(fs_hz),
      num_channels_(num_channels),
      fs_mult_(fs_hz_ / 8000),
      timestamps_per_call_(static_cast<size_t>(fs_hz_ / 100)),
      expand_(expand),
      sync_buffer_(sync_buffer),
      expanded_(num_channels_) {
  RTC_DCHECK_GT(num_channels_, 0);
}

Merge::~Merge() = default;

size_t Merge::Process(int16_t* input,
                      size_t input_length,
                      AudioMultiVector* output) {

  RTC_DCHECK(fs_hz_ == 8000 || fs_hz_ == 16000 || fs_hz_ == 32000 ||
             fs_hz_ == 48000);
  RTC_DCHECK_LE(fs_hz_, kMaxSampleRate);
  if (input_length == 0) {
    return 0;
  }

  size_t old_length;
  size_t expand_period;

  size_t expanded_length = GetExpandedSignal(&old_length, &expand_period);

  AudioMultiVector input_vector(num_channels_);
  input_vector.PushBackInterleaved(
      rtc::ArrayView<const int16_t>(input, input_length));
  size_t input_length_per_channel = input_vector.Size();
  RTC_DCHECK_EQ(input_length_per_channel, input_length / num_channels_);

  size_t best_correlation_index = 0;
  size_t output_length = 0;

  std::unique_ptr<int16_t[]> input_channel(
      new int16_t[input_length_per_channel]);
  std::unique_ptr<int16_t[]> expanded_channel(new int16_t[expanded_length]);
  for (size_t channel = 0; channel < num_channels_; ++channel) {
    input_vector[channel].CopyTo(input_length_per_channel, 0,
                                 input_channel.get());
    expanded_[channel].CopyTo(expanded_length, 0, expanded_channel.get());

    const int16_t new_mute_factor = std::min<int16_t>(
        16384, SignalScaling(input_channel.get(), input_length_per_channel,
                             expanded_channel.get()));

    if (channel == 0) {

      Downsample(input_channel.get(), input_length_per_channel,
                 expanded_channel.get(), expanded_length);

      best_correlation_index = CorrelateAndPeakSearch(
          old_length, input_length_per_channel, expand_period);
    }

    temp_data_.resize(input_length_per_channel + best_correlation_index);
    int16_t* decoded_output = temp_data_.data() + best_correlation_index;

    size_t interpolation_length =
        std::min(kMaxCorrelationLength * fs_mult_,
                 expanded_length - best_correlation_index);
    interpolation_length =
        std::min(interpolation_length, input_length_per_channel);

    RTC_DCHECK_LE(new_mute_factor, 16384);
    int16_t mute_factor =
        std::max(expand_->MuteFactor(channel), new_mute_factor);
    RTC_DCHECK_GE(mute_factor, 0);

    if (mute_factor < 16384) {

      const int back_to_fullscale_inc = static_cast<int>(
          ((16384 - mute_factor) << 6) / input_length_per_channel);
      const int increment = std::max(4194 / fs_mult_, back_to_fullscale_inc);
      mute_factor = static_cast<int16_t>(DspHelper::RampSignal(
          input_channel.get(), interpolation_length, mute_factor, increment));
      DspHelper::UnmuteSignal(&input_channel[interpolation_length],
                              input_length_per_channel - interpolation_length,
                              &mute_factor, increment,
                              &decoded_output[interpolation_length]);
    } else {

      memmove(
          &decoded_output[interpolation_length],
          &input_channel[interpolation_length],
          sizeof(int16_t) * (input_length_per_channel - interpolation_length));
    }

    int16_t increment =
        static_cast<int16_t>(16384 / (interpolation_length + 1));
    int16_t local_mute_factor = 16384 - increment;
    memmove(temp_data_.data(), expanded_channel.get(),
            sizeof(int16_t) * best_correlation_index);
    DspHelper::CrossFade(&expanded_channel[best_correlation_index],
                         input_channel.get(), interpolation_length,
                         &local_mute_factor, increment, decoded_output);

    output_length = best_correlation_index + input_length_per_channel;
    if (channel == 0) {
      RTC_DCHECK(output->Empty());
      output->AssertSize(output_length);
    } else {
      RTC_DCHECK_EQ(output->Size(), output_length);
    }
    (*output)[channel].OverwriteAt(temp_data_.data(), output_length, 0);
  }

  sync_buffer_->ReplaceAtIndex(*output, old_length, sync_buffer_->next_index());
  output->PopFront(old_length);

  RTC_DCHECK_GE(output_length, old_length);
  return output_length - old_length;
}

size_t Merge::GetExpandedSignal(size_t* old_length, size_t* expand_period) {

  *old_length = sync_buffer_->FutureLength();

  RTC_DCHECK_GE(*old_length, expand_->overlap_length());

  expand_->SetParametersForMergeAfterExpand();

  if (*old_length >= 210 * kMaxSampleRate / 8000) {

    size_t length_diff = *old_length - 210 * kMaxSampleRate / 8000;
    sync_buffer_->InsertZerosAtIndex(length_diff, sync_buffer_->next_index());
    *old_length = 210 * kMaxSampleRate / 8000;

  }

  RTC_DCHECK_GE(210 * kMaxSampleRate / 8000, *old_length);

  AudioMultiVector expanded_temp(num_channels_);
  expand_->Process(&expanded_temp);
  *expand_period = expanded_temp.Size();

  expanded_.Clear();

  expanded_.PushBackFromIndex(*sync_buffer_, sync_buffer_->next_index());
  RTC_DCHECK_EQ(expanded_.Size(), *old_length);
  RTC_DCHECK_GT(expanded_temp.Size(), 0);

  const size_t required_length = static_cast<size_t>((120 + 80 + 2) * fs_mult_);
  if (expanded_.Size() < required_length) {
    while (expanded_.Size() < required_length) {

      expanded_.PushBack(expanded_temp);
    }

    expanded_.PopBack(expanded_.Size() - required_length);
  }
  RTC_DCHECK_GE(expanded_.Size(), required_length);
  return required_length;
}

int16_t Merge::SignalScaling(const int16_t* input,
                             size_t input_length,
                             const int16_t* expanded_signal) const {

  const auto mod_input_length = rtc::SafeMin<size_t>(
      64 * rtc::dchecked_cast<size_t>(fs_mult_), input_length);
  const int16_t expanded_max =
      WebRtcSpl_MaxAbsValueW16(expanded_signal, mod_input_length);
  int32_t factor =
      (expanded_max * expanded_max) / (std::numeric_limits<int32_t>::max() /
                                       static_cast<int32_t>(mod_input_length));
  const int expanded_shift = factor == 0 ? 0 : 31 - WebRtcSpl_NormW32(factor);
  int32_t energy_expanded = WebRtcSpl_DotProductWithScale(
      expanded_signal, expanded_signal, mod_input_length, expanded_shift);

  const int16_t input_max = WebRtcSpl_MaxAbsValueW16(input, mod_input_length);
  factor = (input_max * input_max) / (std::numeric_limits<int32_t>::max() /
                                      static_cast<int32_t>(mod_input_length));
  const int input_shift = factor == 0 ? 0 : 31 - WebRtcSpl_NormW32(factor);
  int32_t energy_input = WebRtcSpl_DotProductWithScale(
      input, input, mod_input_length, input_shift);

  if (input_shift > expanded_shift) {
    energy_expanded = energy_expanded >> (input_shift - expanded_shift);
  } else {
    energy_input = energy_input >> (expanded_shift - input_shift);
  }

  int16_t mute_factor;
  if (energy_input > energy_expanded) {

    int16_t temp_shift = WebRtcSpl_NormW32(energy_input) - 17;
    energy_input = WEBRTC_SPL_SHIFT_W32(energy_input, temp_shift);

    energy_expanded = WEBRTC_SPL_SHIFT_W32(energy_expanded, temp_shift + 14);

    mute_factor = static_cast<int16_t>(
        WebRtcSpl_SqrtFloor((energy_expanded / energy_input) << 14));
  } else {

    mute_factor = 16384;
  }

  return mute_factor;
}

void Merge::Downsample(const int16_t* input,
                       size_t input_length,
                       const int16_t* expanded_signal,
                       size_t expanded_length) {
  const int16_t* filter_coefficients;
  size_t num_coefficients;
  int decimation_factor = fs_hz_ / 4000;
  static const size_t kCompensateDelay = 0;
  size_t length_limit = static_cast<size_t>(fs_hz_ / 100);
  if (fs_hz_ == 8000) {
    filter_coefficients = DspHelper::kDownsample8kHzTbl;
    num_coefficients = 3;
  } else if (fs_hz_ == 16000) {
    filter_coefficients = DspHelper::kDownsample16kHzTbl;
    num_coefficients = 5;
  } else if (fs_hz_ == 32000) {
    filter_coefficients = DspHelper::kDownsample32kHzTbl;
    num_coefficients = 7;
  } else {
    filter_coefficients = DspHelper::kDownsample48kHzTbl;
    num_coefficients = 7;
  }
  size_t signal_offset = num_coefficients - 1;
  WebRtcSpl_DownsampleFast(
      &expanded_signal[signal_offset], expanded_length - signal_offset,
      expanded_downsampled_, kExpandDownsampLength, filter_coefficients,
      num_coefficients, decimation_factor, kCompensateDelay);
  if (input_length <= length_limit) {

    const size_t temp_len =
        input_length > signal_offset ? input_length - signal_offset : 0;

    size_t downsamp_temp_len = temp_len / decimation_factor;
    if (downsamp_temp_len > 0) {
      WebRtcSpl_DownsampleFast(&input[signal_offset], temp_len,
                               input_downsampled_, downsamp_temp_len,
                               filter_coefficients, num_coefficients,
                               decimation_factor, kCompensateDelay);
    }
    memset(&input_downsampled_[downsamp_temp_len], 0,
           sizeof(int16_t) * (kInputDownsampLength - downsamp_temp_len));
  } else {
    WebRtcSpl_DownsampleFast(
        &input[signal_offset], input_length - signal_offset, input_downsampled_,
        kInputDownsampLength, filter_coefficients, num_coefficients,
        decimation_factor, kCompensateDelay);
  }
}

size_t Merge::CorrelateAndPeakSearch(size_t start_position,
                                     size_t input_length,
                                     size_t expand_period) const {

  const size_t max_corr_length = kMaxCorrelationLength;
  size_t stop_position_downsamp =
      std::min(max_corr_length, expand_->max_lag() / (fs_mult_ * 2) + 1);

  int32_t correlation[kMaxCorrelationLength];
  CrossCorrelationWithAutoShift(input_downsampled_, expanded_downsampled_,
                                kInputDownsampLength, stop_position_downsamp, 1,
                                correlation);

  const size_t pad_length = expand_->overlap_length() - 1;
  const size_t correlation_buffer_size = 2 * pad_length + kMaxCorrelationLength;
  std::unique_ptr<int16_t[]> correlation16(
      new int16_t[correlation_buffer_size]);
  memset(correlation16.get(), 0, correlation_buffer_size * sizeof(int16_t));
  int16_t* correlation_ptr = &correlation16[pad_length];
  int32_t max_correlation =
      WebRtcSpl_MaxAbsValueW32(correlation, stop_position_downsamp);
  int norm_shift = std::max(0, 17 - WebRtcSpl_NormW32(max_correlation));
  WebRtcSpl_VectorBitShiftW32ToW16(correlation_ptr, stop_position_downsamp,
                                   correlation, norm_shift);

  size_t start_index = timestamps_per_call_ + expand_->overlap_length();
  start_index = std::max(start_position, start_index);
  start_index = (input_length > start_index) ? 0 : (start_index - input_length);

  size_t start_index_downsamp = start_index / (fs_mult_ * 2);

  size_t modified_stop_pos =
      std::min(stop_position_downsamp,
               kMaxCorrelationLength + pad_length - start_index_downsamp);
  size_t best_correlation_index;
  int16_t best_correlation;
  static const size_t kNumCorrelationCandidates = 1;
  DspHelper::PeakDetection(&correlation_ptr[start_index_downsamp],
                           modified_stop_pos, kNumCorrelationCandidates,
                           fs_mult_, &best_correlation_index,
                           &best_correlation);

  best_correlation_index += start_index;

  while (((best_correlation_index + input_length) <
          (timestamps_per_call_ + expand_->overlap_length())) ||
         ((best_correlation_index + input_length) < start_position)) {
    RTC_DCHECK_NOTREACHED();
    best_correlation_index += expand_period;
  }
  return best_correlation_index;
}

size_t Merge::RequiredFutureSamples() {
  return fs_hz_ / 100 * num_channels_;
}

}
