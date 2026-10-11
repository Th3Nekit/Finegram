/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_STATIONARITY_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_STATIONARITY_ESTIMATOR_H_

#include <stddef.h>

#include <array>
#include <atomic>
#include <memory>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/reverb_model.h"
#include "rtc_base/checks.h"

namespace webrtc {

class ApmDataDumper;
struct SpectrumBuffer;

class StationarityEstimator {
 public:
  StationarityEstimator();
  ~StationarityEstimator();

  void Reset();

  void UpdateNoiseEstimator(
      rtc::ArrayView<const std::array<float, kFftLengthBy2Plus1>> spectrum);

  void UpdateStationarityFlags(
      const SpectrumBuffer& spectrum_buffer,
      rtc::ArrayView<const float> render_reverb_contribution_spectrum,
      int idx_current,
      int num_lookahead);

  bool IsBandStationary(size_t band) const {
    return stationarity_flags_[band] && (hangovers_[band] == 0);
  }

  bool IsBlockStationary() const;

 private:
  static constexpr int kWindowLength = 13;

  float GetStationarityPowerBand(size_t k) const { return noise_.Power(k); }

  bool EstimateBandStationarity(const SpectrumBuffer& spectrum_buffer,
                                rtc::ArrayView<const float> average_reverb,
                                const std::array<int, kWindowLength>& indexes,
                                size_t band) const;

  bool AreAllBandsStationary();

  void UpdateHangover();

  void SmoothStationaryPerFreq();

  class NoiseSpectrum {
   public:
    NoiseSpectrum();
    ~NoiseSpectrum();

    void Reset();

    void Update(
        rtc::ArrayView<const std::array<float, kFftLengthBy2Plus1>> spectrum);

    rtc::ArrayView<const float> Spectrum() const { return noise_spectrum_; }

    float Power(size_t band) const {
      RTC_DCHECK_LT(band, noise_spectrum_.size());
      return noise_spectrum_[band];
    }

   private:

    float GetAlpha() const;

    float UpdateBandBySmoothing(float power_band,
                                float power_band_noise,
                                float alpha) const;
    std::array<float, kFftLengthBy2Plus1> noise_spectrum_;
    size_t block_counter_;
  };

  static std::atomic<int> instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  NoiseSpectrum noise_;
  std::array<int, kFftLengthBy2Plus1> hangovers_;
  std::array<bool, kFftLengthBy2Plus1> stationarity_flags_;
};

}

#endif
