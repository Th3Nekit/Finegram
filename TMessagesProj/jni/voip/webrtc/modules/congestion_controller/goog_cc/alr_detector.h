/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_ALR_DETECTOR_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_ALR_DETECTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "absl/types/optional.h"
#include "api/field_trials_view.h"
#include "modules/pacing/interval_budget.h"
#include "rtc_base/experiments/alr_experiment.h"
#include "rtc_base/experiments/struct_parameters_parser.h"

namespace webrtc {

class RtcEventLog;

struct AlrDetectorConfig {

  double bandwidth_usage_ratio = 0.65;
  double start_budget_level_ratio = 0.80;
  double stop_budget_level_ratio = 0.50;
  std::unique_ptr<StructParametersParser> Parser();
};

class AlrDetector {
 public:
  AlrDetector(AlrDetectorConfig config, RtcEventLog* event_log);
  explicit AlrDetector(const FieldTrialsView* key_value_config);
  AlrDetector(const FieldTrialsView* key_value_config, RtcEventLog* event_log);
  ~AlrDetector();

  void OnBytesSent(size_t bytes_sent, int64_t send_time_ms);

  void SetEstimatedBitrate(int bitrate_bps);

  absl::optional<int64_t> GetApplicationLimitedRegionStartTime() const;

 private:
  friend class GoogCcStatePrinter;
  const AlrDetectorConfig conf_;

  absl::optional<int64_t> last_send_time_ms_;

  IntervalBudget alr_budget_;
  absl::optional<int64_t> alr_started_time_ms_;

  RtcEventLog* event_log_;
};
}

#endif
