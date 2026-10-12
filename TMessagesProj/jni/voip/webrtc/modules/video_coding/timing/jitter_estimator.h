/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_TIMING_JITTER_ESTIMATOR_H_
#define MODULES_VIDEO_CODING_TIMING_JITTER_ESTIMATOR_H_

#include <algorithm>
#include <memory>
#include <queue>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/field_trials_view.h"
#include "api/units/data_size.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/video_coding/timing/frame_delay_variation_kalman_filter.h"
#include "modules/video_coding/timing/rtt_filter.h"
#include "rtc_base/experiments/struct_parameters_parser.h"
#include "rtc_base/numerics/moving_percentile_filter.h"
#include "rtc_base/rolling_accumulator.h"

namespace webrtc {

class Clock;

class JitterEstimator {
 public:

  struct Config {
    static constexpr char kFieldTrialsKey[] = "WebRTC-JitterEstimatorConfig";

    static Config ParseAndValidate(absl::string_view field_trial);

    std::unique_ptr<StructParametersParser> Parser() {

      return StructParametersParser::Create(
          "avg_frame_size_median", &avg_frame_size_median,
          "max_frame_size_percentile", &max_frame_size_percentile,
          "frame_size_window", &frame_size_window,
          "num_stddev_delay_clamp", &num_stddev_delay_clamp,
          "num_stddev_delay_outlier", &num_stddev_delay_outlier,
          "num_stddev_size_outlier", &num_stddev_size_outlier,
          "congestion_rejection_factor", &congestion_rejection_factor,
          "estimate_noise_when_congested", &estimate_noise_when_congested);

    }

    bool MaxFrameSizePercentileEnabled() const {
      return max_frame_size_percentile.has_value();
    }

    bool avg_frame_size_median = false;

    absl::optional<double> max_frame_size_percentile = absl::nullopt;

    absl::optional<int> frame_size_window = absl::nullopt;

    absl::optional<double> num_stddev_delay_clamp = absl::nullopt;

    absl::optional<double> num_stddev_delay_outlier = absl::nullopt;

    absl::optional<double> num_stddev_size_outlier = absl::nullopt;

    absl::optional<double> congestion_rejection_factor = absl::nullopt;

    bool estimate_noise_when_congested = true;
  };

  JitterEstimator(Clock* clock, const FieldTrialsView& field_trials);
  JitterEstimator(const JitterEstimator&) = delete;
  JitterEstimator& operator=(const JitterEstimator&) = delete;
  ~JitterEstimator();

  void Reset();

  void UpdateEstimate(TimeDelta frame_delay, DataSize frame_size);

  TimeDelta GetJitterEstimate(double rtt_multiplier,
                              absl::optional<TimeDelta> rtt_mult_add_cap);

  void FrameNacked();

  void UpdateRtt(TimeDelta rtt);

  Config GetConfigForTest() const;

 private:

  void EstimateRandomJitter(double d_dT);

  double NoiseThreshold() const;

  TimeDelta CalculateEstimate();

  void PostProcessEstimate();

  Frequency GetFrameRate() const;

  const Config config_;

  FrameDelayVariationKalmanFilter kalman_filter_;

  double avg_frame_size_bytes_;
  double var_frame_size_bytes2_;

  double max_frame_size_bytes_;

  MovingMedianFilter<int64_t> avg_frame_size_median_bytes_;
  MovingPercentileFilter<int64_t> max_frame_size_bytes_percentile_;

  double startup_frame_size_sum_bytes_;
  size_t startup_frame_size_count_;

  absl::optional<Timestamp> last_update_time_;

  absl::optional<TimeDelta> prev_estimate_;

  absl::optional<DataSize> prev_frame_size_;

  double avg_noise_ms_;

  double var_noise_ms2_;
  size_t alpha_count_;

  TimeDelta filter_jitter_estimate_ = TimeDelta::Zero();

  size_t startup_count_;

  Timestamp latest_nack_ = Timestamp::Zero();

  size_t nack_count_;
  RttFilter rtt_filter_;

  rtc::RollingAccumulator<uint64_t> fps_counter_;
  Clock* clock_;
};

}

#endif
