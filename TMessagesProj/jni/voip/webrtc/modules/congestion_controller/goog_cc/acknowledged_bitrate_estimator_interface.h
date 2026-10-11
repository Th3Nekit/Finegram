/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_ACKNOWLEDGED_BITRATE_ESTIMATOR_INTERFACE_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_ACKNOWLEDGED_BITRATE_ESTIMATOR_INTERFACE_H_

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/field_trials_view.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/experiments/struct_parameters_parser.h"

namespace webrtc {

struct RobustThroughputEstimatorSettings {
  static constexpr char kKey[] = "WebRTC-Bwe-RobustThroughputEstimatorSettings";

  RobustThroughputEstimatorSettings() = delete;
  explicit RobustThroughputEstimatorSettings(
      const FieldTrialsView* key_value_config);

  bool enabled = true;

  unsigned window_packets = 20;
  unsigned max_window_packets = 500;
  TimeDelta min_window_duration = TimeDelta::Seconds(1);
  TimeDelta max_window_duration = TimeDelta::Seconds(5);

  unsigned required_packets = 10;

  double unacked_weight = 1.0;

  std::unique_ptr<StructParametersParser> Parser();
};

class AcknowledgedBitrateEstimatorInterface {
 public:
  static std::unique_ptr<AcknowledgedBitrateEstimatorInterface> Create(
      const FieldTrialsView* key_value_config);
  virtual ~AcknowledgedBitrateEstimatorInterface();

  virtual void IncomingPacketFeedbackVector(
      const std::vector<PacketResult>& packet_feedback_vector) = 0;
  virtual absl::optional<DataRate> bitrate() const = 0;
  virtual absl::optional<DataRate> PeekRate() const = 0;
  virtual void SetAlr(bool in_alr) = 0;
  virtual void SetAlrEndedTime(Timestamp alr_ended_time) = 0;
};

}

#endif
