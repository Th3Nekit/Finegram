/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_RESIDUAL_ECHO_DETECTOR_H_
#define MODULES_AUDIO_PROCESSING_RESIDUAL_ECHO_DETECTOR_H_

#include <atomic>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/echo_detector/circular_buffer.h"
#include "modules/audio_processing/echo_detector/mean_variance_estimator.h"
#include "modules/audio_processing/echo_detector/moving_max.h"
#include "modules/audio_processing/echo_detector/normalized_covariance_estimator.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {

class ApmDataDumper;
class AudioBuffer;

class ResidualEchoDetector : public EchoDetector {
 public:
  ResidualEchoDetector();
  ~ResidualEchoDetector() override;

  void AnalyzeRenderAudio(rtc::ArrayView<const float> render_audio) override;

  void AnalyzeCaptureAudio(rtc::ArrayView<const float> capture_audio) override;

  void Initialize(int capture_sample_rate_hz,
                  int num_capture_channels,
                  int render_sample_rate_hz,
                  int num_render_channels) override;

  void SetReliabilityForTest(float value) { reliability_ = value; }

  EchoDetector::Metrics GetMetrics() const override;

 private:
  static std::atomic<int> instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;

  bool first_process_call_ = true;

  CircularBuffer render_buffer_;

  size_t frames_since_zero_buffer_size_ = 0;

  std::vector<float> render_power_;
  std::vector<float> render_power_mean_;
  std::vector<float> render_power_std_dev_;

  std::vector<NormalizedCovarianceEstimator> covariances_;

  size_t next_insertion_index_ = 0;

  MeanVarianceEstimator render_statistics_;
  MeanVarianceEstimator capture_statistics_;

  float echo_likelihood_ = 0.f;

  float reliability_ = 0.f;
  MovingMax recent_likelihood_max_;

  int log_counter_ = 0;
};

}

#endif
