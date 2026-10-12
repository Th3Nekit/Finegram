/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/background_noise.h"

#include <string.h>

#include <algorithm>

#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "modules/audio_coding/neteq/audio_multi_vector.h"
#include "modules/audio_coding/neteq/cross_correlation.h"

namespace webrtc {
namespace {

constexpr size_t kMaxSampleRate = 48000;

}

constexpr size_t BackgroundNoise::kMaxLpcOrder;

BackgroundNoise::BackgroundNoise(size_t num_channels)
    : num_channels_(num_channels),
      channel_parameters_(new ChannelParameters[num_channels_]) {
  Reset();
}

BackgroundNoise::~BackgroundNoise() {}

void BackgroundNoise::Reset() {
  initialized_ = false;
  for (size_t channel = 0; channel < num_channels_; ++channel) {
    channel_parameters_[channel].Reset();
  }
}

bool BackgroundNoise::Update(const AudioMultiVector& sync_buffer) {
  bool filter_params_saved = false;

  int32_t auto_correlation[kMaxLpcOrder + 1];
  int16_t filter_output[kMaxLpcOrder + kResidualLength];
  int16_t reflection_coefficients[kMaxLpcOrder];
  int16_t lpc_coefficients[kMaxLpcOrder + 1];

  for (size_t channel_ix = 0; channel_ix < num_channels_; ++channel_ix) {
    ChannelParameters& parameters = channel_parameters_[channel_ix];
    int16_t temp_signal_array[kVecLen + kMaxLpcOrder] = {0};
    int16_t* temp_signal = &temp_signal_array[kMaxLpcOrder];
    RTC_DCHECK_GE(sync_buffer.Size(), kVecLen);
    sync_buffer[channel_ix].CopyTo(kVecLen, sync_buffer.Size() - kVecLen,
                                   temp_signal);
    int32_t sample_energy =
        CalculateAutoCorrelation(temp_signal, kVecLen, auto_correlation);

    if (sample_energy < parameters.energy_update_threshold) {

      if (auto_correlation[0] <= 0) {

        return filter_params_saved;
      }

      if (sample_energy < parameters.energy_update_threshold) {

        parameters.energy_update_threshold = std::max(sample_energy, 1);
        parameters.low_energy_update_threshold = 0;
      }

      if (WebRtcSpl_LevinsonDurbin(auto_correlation, lpc_coefficients,
                                   reflection_coefficients,
                                   kMaxLpcOrder) != 1) {
        return filter_params_saved;
      }

      WebRtcSpl_FilterMAFastQ12(temp_signal + kVecLen - kResidualLength,
                                filter_output, lpc_coefficients,
                                kMaxLpcOrder + 1, kResidualLength);
      int32_t residual_energy = WebRtcSpl_DotProductWithScale(
          filter_output, filter_output, kResidualLength, 0);

      if ((sample_energy > 0) &&
          (int64_t{5} * residual_energy >= int64_t{16} * sample_energy)) {

        SaveParameters(channel_ix, lpc_coefficients,
                       temp_signal + kVecLen - kMaxLpcOrder, sample_energy,
                       residual_energy);
        filter_params_saved = true;
      }
    } else {

      IncrementEnergyThreshold(channel_ix, sample_energy);
    }
  }
  return filter_params_saved;
}

void BackgroundNoise::GenerateBackgroundNoise(
    rtc::ArrayView<const int16_t> random_vector,
    size_t channel,
    int mute_slope,
    bool too_many_expands,
    size_t num_noise_samples,
    int16_t* buffer) {
  constexpr size_t kNoiseLpcOrder = kMaxLpcOrder;
  int16_t scaled_random_vector[kMaxSampleRate / 8000 * 125];
  RTC_DCHECK_LE(num_noise_samples, (kMaxSampleRate / 8000 * 125));
  RTC_DCHECK_GE(random_vector.size(), num_noise_samples);
  int16_t* noise_samples = &buffer[kNoiseLpcOrder];
  if (initialized()) {

    memcpy(noise_samples - kNoiseLpcOrder, FilterState(channel),
           sizeof(int16_t) * kNoiseLpcOrder);

    int dc_offset = 0;
    if (ScaleShift(channel) > 1) {
      dc_offset = 1 << (ScaleShift(channel) - 1);
    }

    WebRtcSpl_AffineTransformVector(scaled_random_vector, random_vector.data(),
                                    Scale(channel), dc_offset,
                                    ScaleShift(channel), num_noise_samples);

    WebRtcSpl_FilterARFastQ12(scaled_random_vector, noise_samples,
                              Filter(channel), kNoiseLpcOrder + 1,
                              num_noise_samples);

    SetFilterState(
        channel,
        {&(noise_samples[num_noise_samples - kNoiseLpcOrder]), kNoiseLpcOrder});

    int16_t bgn_mute_factor = MuteFactor(channel);
    if (bgn_mute_factor < 16384) {
      WebRtcSpl_AffineTransformVector(noise_samples, noise_samples,
                                      bgn_mute_factor, 8192, 14,
                                      num_noise_samples);
    }

    SetMuteFactor(channel, bgn_mute_factor);
  } else {

    memset(noise_samples, 0, sizeof(int16_t) * num_noise_samples);
  }
}

int32_t BackgroundNoise::Energy(size_t channel) const {
  RTC_DCHECK_LT(channel, num_channels_);
  return channel_parameters_[channel].energy;
}

void BackgroundNoise::SetMuteFactor(size_t channel, int16_t value) {
  RTC_DCHECK_LT(channel, num_channels_);
  channel_parameters_[channel].mute_factor = value;
}

int16_t BackgroundNoise::MuteFactor(size_t channel) const {
  RTC_DCHECK_LT(channel, num_channels_);
  return channel_parameters_[channel].mute_factor;
}

const int16_t* BackgroundNoise::Filter(size_t channel) const {
  RTC_DCHECK_LT(channel, num_channels_);
  return channel_parameters_[channel].filter;
}

const int16_t* BackgroundNoise::FilterState(size_t channel) const {
  RTC_DCHECK_LT(channel, num_channels_);
  return channel_parameters_[channel].filter_state;
}

void BackgroundNoise::SetFilterState(size_t channel,
                                     rtc::ArrayView<const int16_t> input) {
  RTC_DCHECK_LT(channel, num_channels_);
  size_t length = std::min(input.size(), kMaxLpcOrder);
  memcpy(channel_parameters_[channel].filter_state, input.data(),
         length * sizeof(int16_t));
}

int16_t BackgroundNoise::Scale(size_t channel) const {
  RTC_DCHECK_LT(channel, num_channels_);
  return channel_parameters_[channel].scale;
}
int16_t BackgroundNoise::ScaleShift(size_t channel) const {
  RTC_DCHECK_LT(channel, num_channels_);
  return channel_parameters_[channel].scale_shift;
}

int32_t BackgroundNoise::CalculateAutoCorrelation(
    const int16_t* signal,
    size_t length,
    int32_t* auto_correlation) const {
  static const int kCorrelationStep = -1;
  const int correlation_scale =
      CrossCorrelationWithAutoShift(signal, signal, length, kMaxLpcOrder + 1,
                                    kCorrelationStep, auto_correlation);

  int energy_sample_shift = kLogVecLen - correlation_scale;
  return auto_correlation[0] >> energy_sample_shift;
}

void BackgroundNoise::IncrementEnergyThreshold(size_t channel,
                                               int32_t sample_energy) {

  RTC_DCHECK_LT(channel, num_channels_);
  ChannelParameters& parameters = channel_parameters_[channel];
  int32_t temp_energy =
      (kThresholdIncrement * parameters.low_energy_update_threshold) >> 16;
  temp_energy +=
      kThresholdIncrement * (parameters.energy_update_threshold & 0xFF);
  temp_energy +=
      (kThresholdIncrement * ((parameters.energy_update_threshold >> 8) & 0xFF))
      << 8;
  parameters.low_energy_update_threshold += temp_energy;

  parameters.energy_update_threshold +=
      kThresholdIncrement * (parameters.energy_update_threshold >> 16);
  parameters.energy_update_threshold +=
      parameters.low_energy_update_threshold >> 16;
  parameters.low_energy_update_threshold =
      parameters.low_energy_update_threshold & 0x0FFFF;

  parameters.max_energy = parameters.max_energy - (parameters.max_energy >> 10);
  if (sample_energy > parameters.max_energy) {
    parameters.max_energy = sample_energy;
  }

  int32_t energy_update_threshold = (parameters.max_energy + 524288) >> 20;
  if (energy_update_threshold > parameters.energy_update_threshold) {
    parameters.energy_update_threshold = energy_update_threshold;
  }
}

void BackgroundNoise::SaveParameters(size_t channel,
                                     const int16_t* lpc_coefficients,
                                     const int16_t* filter_state,
                                     int32_t sample_energy,
                                     int32_t residual_energy) {
  RTC_DCHECK_LT(channel, num_channels_);
  ChannelParameters& parameters = channel_parameters_[channel];
  memcpy(parameters.filter, lpc_coefficients,
         (kMaxLpcOrder + 1) * sizeof(int16_t));
  memcpy(parameters.filter_state, filter_state, kMaxLpcOrder * sizeof(int16_t));

  parameters.energy = std::max(sample_energy, 1);
  parameters.energy_update_threshold = parameters.energy;
  parameters.low_energy_update_threshold = 0;

  int16_t norm_shift = WebRtcSpl_NormW32(residual_energy) - 1;
  if (norm_shift & 0x1) {
    norm_shift -= 1;
  }
  residual_energy = WEBRTC_SPL_SHIFT_W32(residual_energy, norm_shift);

  parameters.scale = static_cast<int16_t>(WebRtcSpl_SqrtFloor(residual_energy));

  parameters.scale_shift =
      static_cast<int16_t>(13 + ((kLogResidualLength + norm_shift) / 2));

  initialized_ = true;
}

}
