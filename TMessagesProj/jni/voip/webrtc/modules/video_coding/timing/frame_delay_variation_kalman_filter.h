/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_TIMING_FRAME_DELAY_VARIATION_KALMAN_FILTER_H_
#define MODULES_VIDEO_CODING_TIMING_FRAME_DELAY_VARIATION_KALMAN_FILTER_H_

#include "api/units/data_size.h"
#include "api/units/time_delta.h"

namespace webrtc {

class FrameDelayVariationKalmanFilter {
 public:
  FrameDelayVariationKalmanFilter();
  ~FrameDelayVariationKalmanFilter() = default;

  void PredictAndUpdate(double frame_delay_variation_ms,
                        double frame_size_variation_bytes,
                        double max_frame_size_bytes,
                        double var_noise);

  double GetFrameDelayVariationEstimateSizeBased(
      double frame_size_variation_bytes) const;

  double GetFrameDelayVariationEstimateTotal(
      double frame_size_variation_bytes) const;

 private:

  double estimate_[2];
  double estimate_cov_[2][2];

  double process_noise_cov_diag_[2];
};

}

#endif
