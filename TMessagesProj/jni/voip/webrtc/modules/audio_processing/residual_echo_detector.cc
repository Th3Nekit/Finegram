/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/residual_echo_detector.h"

#include <algorithm>
#include <numeric>

#include "absl/types/optional.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/metrics.h"

namespace {

float Power(rtc::ArrayView<const float> input) {
  if (input.empty()) {
    return 0.f;
  }
  return std::inner_product(input.begin(), input.end(), input.begin(), 0.f) /
         input.size();
}

constexpr size_t kLookbackFrames = 650;

constexpr size_t kRenderBufferSize = 30;
constexpr float kAlpha = 0.001f;

constexpr size_t kAggregationBufferSize = 10 * 100;

}

namespace webrtc {

std::atomic<int> ResidualEchoDetector::instance_count_(0);

ResidualEchoDetector::ResidualEchoDetector()
    : data_dumper_(new ApmDataDumper(instance_count_.fetch_add(1) + 1)),
      render_buffer_(kRenderBufferSize),
      render_power_(kLookbackFrames),
      render_power_mean_(kLookbackFrames),
      render_power_std_dev_(kLookbackFrames),
      covariances_(kLookbackFrames),
      recent_likelihood_max_(kAggregationBufferSize) {}

ResidualEchoDetector::~ResidualEchoDetector() = default;

void ResidualEchoDetector::AnalyzeRenderAudio(
    rtc::ArrayView<const float> render_audio) {

  data_dumper_->DumpWav("ed_render", render_audio.size(), render_audio.data(),
                        48000, 1);

  if (render_buffer_.Size() == 0) {
    frames_since_zero_buffer_size_ = 0;
  } else if (frames_since_zero_buffer_size_ >= kRenderBufferSize) {

    render_buffer_.Pop();
    frames_since_zero_buffer_size_ = 0;
  }
  ++frames_since_zero_buffer_size_;
  float power = Power(render_audio);
  render_buffer_.Push(power);
}

void ResidualEchoDetector::AnalyzeCaptureAudio(
    rtc::ArrayView<const float> capture_audio) {

  data_dumper_->DumpWav("ed_capture", capture_audio.size(),
                        capture_audio.data(), 48000, 1);

  if (first_process_call_) {

    render_buffer_.Clear();
    first_process_call_ = false;
  }

  const absl::optional<float> buffered_render_power = render_buffer_.Pop();
  if (!buffered_render_power) {

    return;
  }

  render_statistics_.Update(*buffered_render_power);
  RTC_DCHECK_LT(next_insertion_index_, kLookbackFrames);
  render_power_[next_insertion_index_] = *buffered_render_power;
  render_power_mean_[next_insertion_index_] = render_statistics_.mean();
  render_power_std_dev_[next_insertion_index_] =
      render_statistics_.std_deviation();

  const float capture_power = Power(capture_audio);
  capture_statistics_.Update(capture_power);
  const float capture_mean = capture_statistics_.mean();
  const float capture_std_deviation = capture_statistics_.std_deviation();

  echo_likelihood_ = 0.f;
  size_t read_index = next_insertion_index_;

  int best_delay = -1;
  for (size_t delay = 0; delay < covariances_.size(); ++delay) {
    RTC_DCHECK_LT(read_index, render_power_.size());
    covariances_[delay].Update(capture_power, capture_mean,
                               capture_std_deviation, render_power_[read_index],
                               render_power_mean_[read_index],
                               render_power_std_dev_[read_index]);
    read_index = read_index > 0 ? read_index - 1 : kLookbackFrames - 1;

    if (covariances_[delay].normalized_cross_correlation() > echo_likelihood_) {
      echo_likelihood_ = covariances_[delay].normalized_cross_correlation();
      best_delay = static_cast<int>(delay);
    }
  }

  if (echo_likelihood_ > 1.1f) {

    if (log_counter_ < 5 && best_delay != -1) {
      size_t read_index = kLookbackFrames + next_insertion_index_ - best_delay;
      if (read_index >= kLookbackFrames) {
        read_index -= kLookbackFrames;
      }
      RTC_DCHECK_LT(read_index, render_power_.size());
      RTC_LOG_F(LS_ERROR) << "Echo detector internal state: {"
                             "Echo likelihood: "
                          << echo_likelihood_ << ", Best Delay: " << best_delay
                          << ", Covariance: "
                          << covariances_[best_delay].covariance()
                          << ", Last capture power: " << capture_power
                          << ", Capture mean: " << capture_mean
                          << ", Capture_standard deviation: "
                          << capture_std_deviation << ", Last render power: "
                          << render_power_[read_index]
                          << ", Render mean: " << render_power_mean_[read_index]
                          << ", Render standard deviation: "
                          << render_power_std_dev_[read_index]
                          << ", Reliability: " << reliability_ << "}";
      log_counter_++;
    }
  }
  RTC_DCHECK_LT(echo_likelihood_, 1.1f);

  reliability_ = (1.0f - kAlpha) * reliability_ + kAlpha * 1.0f;
  echo_likelihood_ *= reliability_;

  echo_likelihood_ = std::min(echo_likelihood_, 1.0f);
  int echo_percentage = static_cast<int>(echo_likelihood_ * 100);
  RTC_HISTOGRAM_COUNTS("WebRTC.Audio.ResidualEchoDetector.EchoLikelihood",
                       echo_percentage, 0, 100, 100                     );

  recent_likelihood_max_.Update(echo_likelihood_);

  next_insertion_index_ = next_insertion_index_ < (kLookbackFrames - 1)
                              ? next_insertion_index_ + 1
                              : 0;
}

void ResidualEchoDetector::Initialize(int                           ,
                                      int                         ,
                                      int                          ,
                                      int                        ) {
  render_buffer_.Clear();
  std::fill(render_power_.begin(), render_power_.end(), 0.f);
  std::fill(render_power_mean_.begin(), render_power_mean_.end(), 0.f);
  std::fill(render_power_std_dev_.begin(), render_power_std_dev_.end(), 0.f);
  render_statistics_.Clear();
  capture_statistics_.Clear();
  recent_likelihood_max_.Clear();
  for (auto& cov : covariances_) {
    cov.Clear();
  }
  echo_likelihood_ = 0.f;
  next_insertion_index_ = 0;
  reliability_ = 0.f;
}

EchoDetector::Metrics ResidualEchoDetector::GetMetrics() const {
  EchoDetector::Metrics metrics;
  metrics.echo_likelihood = echo_likelihood_;
  metrics.echo_likelihood_recent_max = recent_likelihood_max_.max();
  return metrics;
}
}
