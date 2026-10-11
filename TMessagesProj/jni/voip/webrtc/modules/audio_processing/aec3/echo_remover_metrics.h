/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_ECHO_REMOVER_METRICS_H_
#define MODULES_AUDIO_PROCESSING_AEC3_ECHO_REMOVER_METRICS_H_

#include <array>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/aec_state.h"

namespace webrtc {

class EchoRemoverMetrics {
 public:
  struct DbMetric {
    DbMetric();
    DbMetric(float sum_value, float floor_value, float ceil_value);
    void Update(float value);
    void UpdateInstant(float value);
    float sum_value;
    float floor_value;
    float ceil_value;
  };

  EchoRemoverMetrics();

  EchoRemoverMetrics(const EchoRemoverMetrics&) = delete;
  EchoRemoverMetrics& operator=(const EchoRemoverMetrics&) = delete;

  void Update(
      const AecState& aec_state,
      const std::array<float, kFftLengthBy2Plus1>& comfort_noise_spectrum,
      const std::array<float, kFftLengthBy2Plus1>& suppressor_gain);

  bool MetricsReported() { return metrics_reported_; }

 private:

  void ResetMetrics();

  int block_counter_ = 0;
  DbMetric erl_time_domain_;
  DbMetric erle_time_domain_;
  bool saturated_capture_ = false;
  bool metrics_reported_ = false;
};

namespace aec3 {

void UpdateDbMetric(const std::array<float, kFftLengthBy2Plus1>& value,
                    std::array<EchoRemoverMetrics::DbMetric, 2>* statistic);

int TransformDbMetricForReporting(bool negate,
                                  float min_value,
                                  float max_value,
                                  float offset,
                                  float scaling,
                                  float value);

}

}

#endif
