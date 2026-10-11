/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_PCC_PCC_NETWORK_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_PCC_PCC_NETWORK_CONTROLLER_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <vector>

#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/pcc/bitrate_controller.h"
#include "modules/congestion_controller/pcc/monitor_interval.h"
#include "modules/congestion_controller/pcc/rtt_tracker.h"
#include "rtc_base/random.h"

namespace webrtc {
namespace pcc {

class PccNetworkController : public NetworkControllerInterface {
 public:
  enum class Mode {
    kStartup,

    kSlowStart,

    kOnlineLearning,

    kDoubleCheck
  };

  enum class MonitorIntervalLengthStrategy {

    kAdaptive,

    kFixed
  };

  explicit PccNetworkController(NetworkControllerConfig config);
  ~PccNetworkController() override;

  NetworkControlUpdate OnNetworkAvailability(NetworkAvailability msg) override;
  NetworkControlUpdate OnNetworkRouteChange(NetworkRouteChange msg) override;
  NetworkControlUpdate OnProcessInterval(ProcessInterval msg) override;
  NetworkControlUpdate OnSentPacket(SentPacket msg) override;
  NetworkControlUpdate OnTargetRateConstraints(
      TargetRateConstraints msg) override;
  NetworkControlUpdate OnTransportPacketsFeedback(
      TransportPacketsFeedback msg) override;

  NetworkControlUpdate OnStreamsConfig(StreamsConfig msg) override;
  NetworkControlUpdate OnRemoteBitrateReport(RemoteBitrateReport msg) override;
  NetworkControlUpdate OnRoundTripTimeUpdate(RoundTripTimeUpdate msg) override;
  NetworkControlUpdate OnTransportLossReport(TransportLossReport msg) override;
  NetworkControlUpdate OnReceivedPacket(ReceivedPacket msg) override;
  NetworkControlUpdate OnNetworkStateEstimate(
      NetworkStateEstimate msg) override;

 private:
  void UpdateSendingRateAndMode();
  NetworkControlUpdate CreateRateUpdate(Timestamp at_time) const;
  TimeDelta ComputeMonitorIntervalsDuration() const;
  bool NeedDoubleCheckMeasurments() const;
  bool IsTimeoutExpired(Timestamp current_time) const;
  bool IsFeedbackCollectionDone() const;

  Timestamp start_time_;
  Timestamp last_sent_packet_time_;
  TimeDelta smoothed_packets_sending_interval_;
  Mode mode_;

  DataRate default_bandwidth_;

  DataRate bandwidth_estimate_;

  RttTracker rtt_tracker_;
  TimeDelta monitor_interval_timeout_;
  const MonitorIntervalLengthStrategy monitor_interval_length_strategy_;
  const double monitor_interval_duration_ratio_;
  const double sampling_step_;
  const double monitor_interval_timeout_ratio_;
  const int64_t min_packets_number_per_interval_;

  PccBitrateController bitrate_controller_;

  std::vector<PccMonitorInterval> monitor_intervals_;
  std::vector<DataRate> monitor_intervals_bitrates_;
  TimeDelta monitor_intervals_duration_;
  size_t complete_feedback_monitor_interval_number_;

  webrtc::Random random_generator_;
  std::deque<PacketResult> last_received_packets_;
};

}
}

#endif
