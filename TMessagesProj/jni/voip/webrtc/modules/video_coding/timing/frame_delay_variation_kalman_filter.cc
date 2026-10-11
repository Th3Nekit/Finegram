/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/frame_delay_variation_kalman_filter.h"

#include "api/units/data_size.h"
#include "api/units/time_delta.h"

namespace webrtc {

namespace {

constexpr double kMaxBandwidth = 0.000001;
}

FrameDelayVariationKalmanFilter::FrameDelayVariationKalmanFilter() {

  estimate_[0] = 1 / (512e3 / 8);
  estimate_[1] = 0;

  estimate_cov_[0][0] = 1e-4;
  estimate_cov_[1][1] = 1e2;
  estimate_cov_[0][1] = estimate_cov_[1][0] = 0;

  process_noise_cov_diag_[0] = 2.5e-10;
  process_noise_cov_diag_[1] = 1e-10;
}

void FrameDelayVariationKalmanFilter::PredictAndUpdate(
    double frame_delay_variation_ms,
    double frame_size_variation_bytes,
    double max_frame_size_bytes,
    double var_noise) {

  if (max_frame_size_bytes < 1) {
    return;
  }
  if (var_noise <= 0.0) {
    return;
  }

  estimate_cov_[0][0] += process_noise_cov_diag_[0];
  estimate_cov_[1][1] += process_noise_cov_diag_[1];

  double innovation =
      frame_delay_variation_ms -
      GetFrameDelayVariationEstimateTotal(frame_size_variation_bytes);

  double estim_cov_times_obs[2];
  estim_cov_times_obs[0] =
      estimate_cov_[0][0] * frame_size_variation_bytes + estimate_cov_[0][1];
  estim_cov_times_obs[1] =
      estimate_cov_[1][0] * frame_size_variation_bytes + estimate_cov_[1][1];
  double observation_noise_stddev =
      (300.0 * exp(-fabs(frame_size_variation_bytes) /
                   (1e0 * max_frame_size_bytes)) +
       1) *
      sqrt(var_noise);
  if (observation_noise_stddev < 1.0) {
    observation_noise_stddev = 1.0;
  }

  double innovation_var = frame_size_variation_bytes * estim_cov_times_obs[0] +
                          estim_cov_times_obs[1] + observation_noise_stddev;
  if ((innovation_var < 1e-9 && innovation_var >= 0) ||
      (innovation_var > -1e-9 && innovation_var <= 0)) {
    RTC_DCHECK_NOTREACHED();
    return;
  }

  double kalman_gain[2];
  kalman_gain[0] = estim_cov_times_obs[0] / innovation_var;
  kalman_gain[1] = estim_cov_times_obs[1] / innovation_var;

  estimate_[0] += kalman_gain[0] * innovation;
  estimate_[1] += kalman_gain[1] * innovation;

  if (estimate_[0] < kMaxBandwidth) {
    estimate_[0] = kMaxBandwidth;
  }

  double t00 = estimate_cov_[0][0];
  double t01 = estimate_cov_[0][1];
  estimate_cov_[0][0] =
      (1 - kalman_gain[0] * frame_size_variation_bytes) * t00 -
      kalman_gain[0] * estimate_cov_[1][0];
  estimate_cov_[0][1] =
      (1 - kalman_gain[0] * frame_size_variation_bytes) * t01 -
      kalman_gain[0] * estimate_cov_[1][1];
  estimate_cov_[1][0] = estimate_cov_[1][0] * (1 - kalman_gain[1]) -
                        kalman_gain[1] * frame_size_variation_bytes * t00;
  estimate_cov_[1][1] = estimate_cov_[1][1] * (1 - kalman_gain[1]) -
                        kalman_gain[1] * frame_size_variation_bytes * t01;

  RTC_DCHECK(estimate_cov_[0][0] + estimate_cov_[1][1] >= 0 &&
             estimate_cov_[0][0] * estimate_cov_[1][1] -
                     estimate_cov_[0][1] * estimate_cov_[1][0] >=
                 0 &&
             estimate_cov_[0][0] >= 0);
}

double FrameDelayVariationKalmanFilter::GetFrameDelayVariationEstimateSizeBased(
    double frame_size_variation_bytes) const {

  return estimate_[0] * frame_size_variation_bytes;
}

double FrameDelayVariationKalmanFilter::GetFrameDelayVariationEstimateTotal(
    double frame_size_variation_bytes) const {
  double frame_transmission_delay_ms =
      GetFrameDelayVariationEstimateSizeBased(frame_size_variation_bytes);
  double link_queuing_delay_ms = estimate_[1];
  return frame_transmission_delay_ms + link_queuing_delay_ms;
}

}
