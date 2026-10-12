/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_UTILITY_CASCADED_BIQUAD_FILTER_H_
#define MODULES_AUDIO_PROCESSING_UTILITY_CASCADED_BIQUAD_FILTER_H_

#include <stddef.h>

#include <complex>
#include <vector>

#include "api/array_view.h"

namespace webrtc {

class CascadedBiQuadFilter {
 public:
  struct BiQuadParam {
    BiQuadParam(std::complex<float> zero,
                std::complex<float> pole,
                float gain,
                bool mirror_zero_along_i_axis = false);
    explicit BiQuadParam(const BiQuadParam&);
    std::complex<float> zero;
    std::complex<float> pole;
    float gain;
    bool mirror_zero_along_i_axis;
  };

  struct BiQuadCoefficients {
    float b[3];
    float a[2];
  };

  struct BiQuad {
    explicit BiQuad(const BiQuadCoefficients& coefficients)
        : coefficients(coefficients), x(), y() {}
    explicit BiQuad(const CascadedBiQuadFilter::BiQuadParam& param);
    void Reset();
    BiQuadCoefficients coefficients;
    float x[2];
    float y[2];
  };

  CascadedBiQuadFilter(
      const CascadedBiQuadFilter::BiQuadCoefficients& coefficients,
      size_t num_biquads);
  explicit CascadedBiQuadFilter(
      const std::vector<CascadedBiQuadFilter::BiQuadParam>& biquad_params);
  ~CascadedBiQuadFilter();
  CascadedBiQuadFilter(const CascadedBiQuadFilter&) = delete;
  CascadedBiQuadFilter& operator=(const CascadedBiQuadFilter&) = delete;

  void Process(rtc::ArrayView<const float> x, rtc::ArrayView<float> y);

  void Process(rtc::ArrayView<float> y);

  void Reset();

 private:
  void ApplyBiQuad(rtc::ArrayView<const float> x,
                   rtc::ArrayView<float> y,
                   CascadedBiQuadFilter::BiQuad* biquad);

  std::vector<BiQuad> biquads_;
};

}

#endif
