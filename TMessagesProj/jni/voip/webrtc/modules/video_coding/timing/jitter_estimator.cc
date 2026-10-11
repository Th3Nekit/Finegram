/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/jitter_estimator.h"

#include <math.h>
#include <string.h>

#include <algorithm>
#include <cstdint>

#include "absl/types/optional.h"
#include "api/field_trials_view.h"
#include "api/units/data_size.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/video_coding/timing/rtt_filter.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {

constexpr size_t kFrameProcessingStartupCount = 30;

constexpr size_t kFramesUntilSizeFiltering = 5;

constexpr double kInitialAvgAndMaxFrameSizeBytes = 500.0;

constexpr double kPhi = 0.97;

constexpr double kPsi = 0.9999;

constexpr double kDefaultMaxFrameSizePercentile = 0.95;
constexpr int kDefaultFrameSizeWindow = 30 * 10;

constexpr double kNumStdDevDelayClamp = 3.5;
constexpr double kNumStdDevDelayOutlier = 15.0;
constexpr double kNumStdDevSizeOutlier = 3.0;
constexpr double kCongestionRejectionFactor = -0.25;

constexpr size_t kAlphaCountMax = 400;

constexpr double kNoiseStdDevs = 2.33;

constexpr double kNoiseStdDevOffset = 30.0;

constexpr TimeDelta kMinJitterEstimate = TimeDelta::Millis(1);
constexpr TimeDelta kMaxJitterEstimate = TimeDelta::Seconds(10);

constexpr TimeDelta OPERATING_SYSTEM_JITTER = TimeDelta::Millis(10);

constexpr TimeDelta kNackCountTimeout = TimeDelta::Seconds(60);

constexpr size_t kNackLimit = 3;

constexpr Frequency kMaxFramerateEstimate = Frequency::Hertz(200);

}

constexpr char JitterEstimator::Config::kFieldTrialsKey[];

JitterEstimator::Config JitterEstimator::Config::ParseAndValidate(
    absl::string_view field_trial) {
  Config config;
  config.Parser()->Parse(field_trial);

  if (config.max_frame_size_percentile) {
    double original = *config.max_frame_size_percentile;
    config.max_frame_size_percentile = std::min(std::max(0.0, original), 1.0);
    if (config.max_frame_size_percentile != original) {
      RTC_LOG(LS_ERROR) << "Skipping invalid max_frame_size_percentile="
                        << original;
    }
  }
  if (config.frame_size_window && config.frame_size_window < 1) {
    RTC_LOG(LS_ERROR) << "Skipping invalid frame_size_window="
                      << *config.frame_size_window;
    config.frame_size_window = 1;
  }

  if (config.num_stddev_delay_clamp && config.num_stddev_delay_clamp < 0.0) {
    RTC_LOG(LS_ERROR) << "Skipping invalid num_stddev_delay_clamp="
                      << *config.num_stddev_delay_clamp;
    config.num_stddev_delay_clamp = 0.0;
  }
  if (config.num_stddev_delay_outlier &&
      config.num_stddev_delay_outlier < 0.0) {
    RTC_LOG(LS_ERROR) << "Skipping invalid num_stddev_delay_outlier="
                      << *config.num_stddev_delay_outlier;
    config.num_stddev_delay_outlier = 0.0;
  }
  if (config.num_stddev_size_outlier && config.num_stddev_size_outlier < 0.0) {
    RTC_LOG(LS_ERROR) << "Skipping invalid num_stddev_size_outlier="
                      << *config.num_stddev_size_outlier;
    config.num_stddev_size_outlier = 0.0;
  }

  return config;
}

JitterEstimator::JitterEstimator(Clock* clock,
                                 const FieldTrialsView& field_trials)
    : config_(Config::ParseAndValidate(
          field_trials.Lookup(Config::kFieldTrialsKey))),
      avg_frame_size_median_bytes_(static_cast<size_t>(
          config_.frame_size_window.value_or(kDefaultFrameSizeWindow))),
      max_frame_size_bytes_percentile_(
          config_.max_frame_size_percentile.value_or(
              kDefaultMaxFrameSizePercentile),
          static_cast<size_t>(
              config_.frame_size_window.value_or(kDefaultFrameSizeWindow))),
      fps_counter_(30),

      clock_(clock) {
  Reset();
}

JitterEstimator::~JitterEstimator() = default;

void JitterEstimator::Reset() {
  avg_frame_size_bytes_ = kInitialAvgAndMaxFrameSizeBytes;
  max_frame_size_bytes_ = kInitialAvgAndMaxFrameSizeBytes;
  var_frame_size_bytes2_ = 100;
  avg_frame_size_median_bytes_.Reset();
  max_frame_size_bytes_percentile_.Reset();
  last_update_time_ = absl::nullopt;
  prev_estimate_ = absl::nullopt;
  prev_frame_size_ = absl::nullopt;
  avg_noise_ms_ = 0.0;
  var_noise_ms2_ = 4.0;
  alpha_count_ = 1;
  filter_jitter_estimate_ = TimeDelta::Zero();
  latest_nack_ = Timestamp::Zero();
  nack_count_ = 0;
  startup_frame_size_sum_bytes_ = 0;
  startup_frame_size_count_ = 0;
  startup_count_ = 0;
  rtt_filter_.Reset();
  fps_counter_.Reset();

  kalman_filter_ = FrameDelayVariationKalmanFilter();
}

void JitterEstimator::UpdateEstimate(TimeDelta frame_delay,
                                     DataSize frame_size) {
  if (frame_size.IsZero()) {
    return;
  }

  double delta_frame_bytes =
      frame_size.bytes() - prev_frame_size_.value_or(DataSize::Zero()).bytes();
  if (startup_frame_size_count_ < kFramesUntilSizeFiltering) {
    startup_frame_size_sum_bytes_ += frame_size.bytes();
    startup_frame_size_count_++;
  } else if (startup_frame_size_count_ == kFramesUntilSizeFiltering) {

    avg_frame_size_bytes_ = startup_frame_size_sum_bytes_ /
                            static_cast<double>(startup_frame_size_count_);
    startup_frame_size_count_++;
  }

  double avg_frame_size_bytes =
      kPhi * avg_frame_size_bytes_ + (1 - kPhi) * frame_size.bytes();
  double deviation_size_bytes = 2 * sqrt(var_frame_size_bytes2_);
  if (frame_size.bytes() < avg_frame_size_bytes_ + deviation_size_bytes) {

    avg_frame_size_bytes_ = avg_frame_size_bytes;
  }

  double delta_bytes = frame_size.bytes() - avg_frame_size_bytes;
  var_frame_size_bytes2_ = std::max(
      kPhi * var_frame_size_bytes2_ + (1 - kPhi) * (delta_bytes * delta_bytes),
      1.0);

  max_frame_size_bytes_ =
      std::max<double>(kPsi * max_frame_size_bytes_, frame_size.bytes());

  if (config_.avg_frame_size_median) {
    avg_frame_size_median_bytes_.Insert(frame_size.bytes());
  }
  if (config_.MaxFrameSizePercentileEnabled()) {
    max_frame_size_bytes_percentile_.Insert(frame_size.bytes());
  }

  if (!prev_frame_size_) {
    prev_frame_size_ = frame_size;
    return;
  }
  prev_frame_size_ = frame_size;

  double num_stddev_delay_clamp =
      config_.num_stddev_delay_clamp.value_or(kNumStdDevDelayClamp);
  TimeDelta max_time_deviation =
      TimeDelta::Millis(num_stddev_delay_clamp * sqrt(var_noise_ms2_) + 0.5);
  frame_delay.Clamp(-max_time_deviation, max_time_deviation);

  double delay_deviation_ms =
      frame_delay.ms() -
      kalman_filter_.GetFrameDelayVariationEstimateTotal(delta_frame_bytes);

  double num_stddev_delay_outlier =
      config_.num_stddev_delay_outlier.value_or(kNumStdDevDelayOutlier);

  bool abs_delay_is_not_outlier =
      fabs(delay_deviation_ms) <
      num_stddev_delay_outlier * sqrt(var_noise_ms2_);

  double num_stddev_size_outlier =
      config_.num_stddev_size_outlier.value_or(kNumStdDevSizeOutlier);
  bool size_is_positive_outlier =
      frame_size.bytes() >
      avg_frame_size_bytes_ +
          num_stddev_size_outlier * sqrt(var_frame_size_bytes2_);

  if (abs_delay_is_not_outlier || size_is_positive_outlier) {

    double congestion_rejection_factor =
        config_.congestion_rejection_factor.value_or(
            kCongestionRejectionFactor);
    double filtered_max_frame_size_bytes =
        config_.MaxFrameSizePercentileEnabled()
            ? max_frame_size_bytes_percentile_.GetFilteredValue()
            : max_frame_size_bytes_;
    bool is_not_congested =
        delta_frame_bytes >
        congestion_rejection_factor * filtered_max_frame_size_bytes;

    if (is_not_congested || config_.estimate_noise_when_congested) {

      EstimateRandomJitter(delay_deviation_ms);
    }
    if (is_not_congested) {

      kalman_filter_.PredictAndUpdate(frame_delay.ms(), delta_frame_bytes,
                                      filtered_max_frame_size_bytes,
                                      var_noise_ms2_);
    }
  } else {

    double num_stddev = (delay_deviation_ms >= 0) ? num_stddev_delay_outlier
                                                  : -num_stddev_delay_outlier;
    EstimateRandomJitter(num_stddev * sqrt(var_noise_ms2_));
  }

  if (startup_count_ >= kFrameProcessingStartupCount) {
    PostProcessEstimate();
  } else {
    startup_count_++;
  }
}

void JitterEstimator::FrameNacked() {
  if (nack_count_ < kNackLimit) {
    nack_count_++;
  }
  latest_nack_ = clock_->CurrentTime();
}

void JitterEstimator::UpdateRtt(TimeDelta rtt) {
  rtt_filter_.Update(rtt);
}

JitterEstimator::Config JitterEstimator::GetConfigForTest() const {
  return config_;
}

void JitterEstimator::EstimateRandomJitter(double d_dT) {
  Timestamp now = clock_->CurrentTime();
  if (last_update_time_.has_value()) {
    fps_counter_.AddSample((now - *last_update_time_).us());
  }
  last_update_time_ = now;

  if (alpha_count_ == 0) {
    RTC_DCHECK_NOTREACHED();
    return;
  }
  double alpha =
      static_cast<double>(alpha_count_ - 1) / static_cast<double>(alpha_count_);
  alpha_count_++;
  if (alpha_count_ > kAlphaCountMax)
    alpha_count_ = kAlphaCountMax;

  Frequency fps = GetFrameRate();
  if (fps > Frequency::Zero()) {
    constexpr Frequency k30Fps = Frequency::Hertz(30);
    double rate_scale = k30Fps / fps;

    if (alpha_count_ < kFrameProcessingStartupCount) {
      rate_scale = (alpha_count_ * rate_scale +
                    (kFrameProcessingStartupCount - alpha_count_)) /
                   kFrameProcessingStartupCount;
    }
    alpha = pow(alpha, rate_scale);
  }

  double avg_noise_ms = alpha * avg_noise_ms_ + (1 - alpha) * d_dT;
  double var_noise_ms2 = alpha * var_noise_ms2_ + (1 - alpha) *
                                                      (d_dT - avg_noise_ms_) *
                                                      (d_dT - avg_noise_ms_);
  avg_noise_ms_ = avg_noise_ms;
  var_noise_ms2_ = var_noise_ms2;
  if (var_noise_ms2_ < 1.0) {

    var_noise_ms2_ = 1.0;
  }
}

double JitterEstimator::NoiseThreshold() const {
  double noise_threshold_ms =
      kNoiseStdDevs * sqrt(var_noise_ms2_) - kNoiseStdDevOffset;
  if (noise_threshold_ms < 1.0) {
    noise_threshold_ms = 1.0;
  }
  return noise_threshold_ms;
}

TimeDelta JitterEstimator::CalculateEstimate() {

  double filtered_avg_frame_size_bytes =
      config_.avg_frame_size_median
          ? avg_frame_size_median_bytes_.GetFilteredValue()
          : avg_frame_size_bytes_;
  double filtered_max_frame_size_bytes =
      config_.MaxFrameSizePercentileEnabled()
          ? max_frame_size_bytes_percentile_.GetFilteredValue()
          : max_frame_size_bytes_;
  double worst_case_frame_size_deviation_bytes =
      filtered_max_frame_size_bytes - filtered_avg_frame_size_bytes;
  double ret_ms = kalman_filter_.GetFrameDelayVariationEstimateSizeBased(
                      worst_case_frame_size_deviation_bytes) +
                  NoiseThreshold();
  TimeDelta ret = TimeDelta::Millis(ret_ms);

  if (ret < kMinJitterEstimate) {
    ret = prev_estimate_.value_or(kMinJitterEstimate);

    RTC_DCHECK_GE(ret, kMinJitterEstimate);
  } else if (ret > kMaxJitterEstimate) {
    ret = kMaxJitterEstimate;
  }
  prev_estimate_ = ret;
  return ret;
}

void JitterEstimator::PostProcessEstimate() {
  filter_jitter_estimate_ = CalculateEstimate();
}

TimeDelta JitterEstimator::GetJitterEstimate(
    double rtt_multiplier,
    absl::optional<TimeDelta> rtt_mult_add_cap) {
  TimeDelta jitter = CalculateEstimate() + OPERATING_SYSTEM_JITTER;
  Timestamp now = clock_->CurrentTime();

  if (now - latest_nack_ > kNackCountTimeout)
    nack_count_ = 0;

  if (filter_jitter_estimate_ > jitter)
    jitter = filter_jitter_estimate_;
  if (nack_count_ >= kNackLimit) {
    if (rtt_mult_add_cap.has_value()) {
      jitter += std::min(rtt_filter_.Rtt() * rtt_multiplier,
                         rtt_mult_add_cap.value());
    } else {
      jitter += rtt_filter_.Rtt() * rtt_multiplier;
    }
  }

  static const Frequency kJitterScaleLowThreshold = Frequency::Hertz(5);
  static const Frequency kJitterScaleHighThreshold = Frequency::Hertz(10);
  Frequency fps = GetFrameRate();

  if (fps < kJitterScaleLowThreshold) {
    if (fps.IsZero()) {
      return std::max(TimeDelta::Zero(), jitter);
    }
    return TimeDelta::Zero();
  }

  if (fps < kJitterScaleHighThreshold) {
    jitter = (1.0 / (kJitterScaleHighThreshold - kJitterScaleLowThreshold)) *
             (fps - kJitterScaleLowThreshold) * jitter;
  }

  return std::max(TimeDelta::Zero(), jitter);
}

Frequency JitterEstimator::GetFrameRate() const {
  TimeDelta mean_frame_period = TimeDelta::Micros(fps_counter_.ComputeMean());
  if (mean_frame_period <= TimeDelta::Zero())
    return Frequency::Zero();

  Frequency fps = 1 / mean_frame_period;

  RTC_DCHECK_GE(fps, Frequency::Zero());
  return std::min(fps, kMaxFramerateEstimate);
}
}
