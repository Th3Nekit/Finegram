/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/ns/signal_model_estimator.h"

#include "modules/audio_processing/ns/fast_math.h"

namespace webrtc {

namespace {

constexpr float kOneByFftSizeBy2Plus1 = 1.f / kFftSizeBy2Plus1;

float ComputeSpectralDiff(
    rtc::ArrayView<const float, kFftSizeBy2Plus1> conservative_noise_spectrum,
    rtc::ArrayView<const float, kFftSizeBy2Plus1> signal_spectrum,
    float signal_spectral_sum,
    float diff_normalization) {

  float noise_average = 0.f;
  for (size_t i = 0; i < kFftSizeBy2Plus1; ++i) {

    noise_average += conservative_noise_spectrum[i];
  }
  noise_average = noise_average * kOneByFftSizeBy2Plus1;
  float signal_average = signal_spectral_sum * kOneByFftSizeBy2Plus1;

  float covariance = 0.f;
  float noise_variance = 0.f;
  float signal_variance = 0.f;
  for (size_t i = 0; i < kFftSizeBy2Plus1; ++i) {
    float signal_diff = signal_spectrum[i] - signal_average;
    float noise_diff = conservative_noise_spectrum[i] - noise_average;
    covariance += signal_diff * noise_diff;
    noise_variance += noise_diff * noise_diff;
    signal_variance += signal_diff * signal_diff;
  }
  covariance *= kOneByFftSizeBy2Plus1;
  noise_variance *= kOneByFftSizeBy2Plus1;
  signal_variance *= kOneByFftSizeBy2Plus1;

  float spectral_diff =
      signal_variance - (covariance * covariance) / (noise_variance + 0.0001f);

  return spectral_diff / (diff_normalization + 0.0001f);
}

void UpdateSpectralFlatness(
    rtc::ArrayView<const float, kFftSizeBy2Plus1> signal_spectrum,
    float signal_spectral_sum,
    float* spectral_flatness) {
  RTC_DCHECK(spectral_flatness);

  constexpr float kAveraging = 0.3f;
  float avg_spect_flatness_num = 0.f;
  for (size_t i = 1; i < kFftSizeBy2Plus1; ++i) {
    if (signal_spectrum[i] == 0.f) {
      *spectral_flatness -= kAveraging * (*spectral_flatness);
      return;
    }
  }

  for (size_t i = 1; i < kFftSizeBy2Plus1; ++i) {
    avg_spect_flatness_num += LogApproximation(signal_spectrum[i]);
  }

  float avg_spect_flatness_denom = signal_spectral_sum - signal_spectrum[0];

  avg_spect_flatness_denom = avg_spect_flatness_denom * kOneByFftSizeBy2Plus1;
  avg_spect_flatness_num = avg_spect_flatness_num * kOneByFftSizeBy2Plus1;

  float spectral_tmp =
      ExpApproximation(avg_spect_flatness_num) / avg_spect_flatness_denom;

  *spectral_flatness += kAveraging * (spectral_tmp - *spectral_flatness);
}

void UpdateSpectralLrt(rtc::ArrayView<const float, kFftSizeBy2Plus1> prior_snr,
                       rtc::ArrayView<const float, kFftSizeBy2Plus1> post_snr,
                       rtc::ArrayView<float, kFftSizeBy2Plus1> avg_log_lrt,
                       float* lrt) {
  RTC_DCHECK(lrt);

  for (size_t i = 0; i < kFftSizeBy2Plus1; ++i) {
    float tmp1 = 1.f + 2.f * prior_snr[i];
    float tmp2 = 2.f * prior_snr[i] / (tmp1 + 0.0001f);
    float bessel_tmp = (post_snr[i] + 1.f) * tmp2;
    avg_log_lrt[i] +=
        .5f * (bessel_tmp - LogApproximation(tmp1) - avg_log_lrt[i]);
  }

  float log_lrt_time_avg_k_sum = 0.f;
  for (size_t i = 0; i < kFftSizeBy2Plus1; ++i) {
    log_lrt_time_avg_k_sum += avg_log_lrt[i];
  }
  *lrt = log_lrt_time_avg_k_sum * kOneByFftSizeBy2Plus1;
}

}

SignalModelEstimator::SignalModelEstimator()
    : prior_model_estimator_(kLtrFeatureThr) {}

void SignalModelEstimator::AdjustNormalization(int32_t num_analyzed_frames,
                                               float signal_energy) {
  diff_normalization_ *= num_analyzed_frames;
  diff_normalization_ += signal_energy;
  diff_normalization_ /= (num_analyzed_frames + 1);
}

void SignalModelEstimator::Update(
    rtc::ArrayView<const float, kFftSizeBy2Plus1> prior_snr,
    rtc::ArrayView<const float, kFftSizeBy2Plus1> post_snr,
    rtc::ArrayView<const float, kFftSizeBy2Plus1> conservative_noise_spectrum,
    rtc::ArrayView<const float, kFftSizeBy2Plus1> signal_spectrum,
    float signal_spectral_sum,
    float signal_energy) {

  UpdateSpectralFlatness(signal_spectrum, signal_spectral_sum,
                         &features_.spectral_flatness);

  float spectral_diff =
      ComputeSpectralDiff(conservative_noise_spectrum, signal_spectrum,
                          signal_spectral_sum, diff_normalization_);

  features_.spectral_diff += 0.3f * (spectral_diff - features_.spectral_diff);

  signal_energy_sum_ += signal_energy;

  if (--histogram_analysis_counter_ > 0) {
    histograms_.Update(features_);
  } else {

    prior_model_estimator_.Update(histograms_);

    histograms_.Clear();

    histogram_analysis_counter_ = kFeatureUpdateWindowSize;

    signal_energy_sum_ = signal_energy_sum_ / kFeatureUpdateWindowSize;
    diff_normalization_ = 0.5f * (signal_energy_sum_ + diff_normalization_);
    signal_energy_sum_ = 0.f;
  }

  UpdateSpectralLrt(prior_snr, post_snr, features_.avg_log_lrt, &features_.lrt);
}

}
