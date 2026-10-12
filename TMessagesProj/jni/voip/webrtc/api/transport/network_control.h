/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TRANSPORT_NETWORK_CONTROL_H_
#define API_TRANSPORT_NETWORK_CONTROL_H_
#include <stdint.h>

#include <memory>

#include "absl/base/attributes.h"
#include "api/field_trials_view.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/transport/network_types.h"

namespace webrtc {

class TargetTransferRateObserver {
 public:
  virtual ~TargetTransferRateObserver() = default;

  virtual void OnTargetTransferRate(TargetTransferRate) = 0;

  virtual void OnStartRateUpdate(DataRate) {}
};

struct NetworkControllerConfig {

  TargetRateConstraints constraints;

  StreamsConfig stream_based_config;

  const FieldTrialsView* key_value_config = nullptr;

  RtcEventLog* event_log = nullptr;
};

class NetworkControllerInterface {
 public:
  virtual ~NetworkControllerInterface() = default;

  ABSL_MUST_USE_RESULT virtual NetworkControlUpdate OnNetworkAvailability(
      NetworkAvailability) = 0;

  ABSL_MUST_USE_RESULT virtual NetworkControlUpdate OnNetworkRouteChange(
      NetworkRouteChange) = 0;

  ABSL_MUST_USE_RESULT virtual NetworkControlUpdate OnProcessInterval(
      ProcessInterval) = 0;

  ABSL_MUST_USE_RESULT virtual NetworkControlUpdate OnRemoteBitrateReport(
      RemoteBitrateReport) = 0;

  ABSL_MUST_USE_RESULT virtual NetworkControlUpdate OnRoundTripTimeUpdate(
      RoundTripTimeUpdate) = 0;

  ABSL_MUST_USE_RESULT virtual NetworkControlUpdate OnSentPacket(
      SentPacket) = 0;

  ABSL_MUST_USE_RESULT virtual NetworkControlUpdate OnReceivedPacket(
      ReceivedPacket) = 0;

  ABSL_MUST_USE_RESULT virtual NetworkControlUpdate OnStreamsConfig(
      StreamsConfig) = 0;

  ABSL_MUST_USE_RESULT virtual NetworkControlUpdate OnTargetRateConstraints(
      TargetRateConstraints) = 0;

  ABSL_MUST_USE_RESULT virtual NetworkControlUpdate OnTransportLossReport(
      TransportLossReport) = 0;

  ABSL_MUST_USE_RESULT virtual NetworkControlUpdate OnTransportPacketsFeedback(
      TransportPacketsFeedback) = 0;

  ABSL_MUST_USE_RESULT virtual NetworkControlUpdate OnNetworkStateEstimate(
      NetworkStateEstimate) = 0;
};

class NetworkControllerFactoryInterface {
 public:
  virtual ~NetworkControllerFactoryInterface() = default;

  virtual std::unique_ptr<NetworkControllerInterface> Create(
      NetworkControllerConfig config) = 0;

  virtual TimeDelta GetProcessInterval() const = 0;
};

class NetworkStateEstimator {
 public:

  virtual absl::optional<NetworkStateEstimate> GetCurrentEstimate() = 0;

  virtual void OnTransportPacketsFeedback(const TransportPacketsFeedback&) = 0;

  virtual void OnReceivedPacket(const PacketResult&) {}

  virtual void OnRouteChange(const NetworkRouteChange&) = 0;
  virtual ~NetworkStateEstimator() = default;
};
class NetworkStateEstimatorFactory {
 public:
  virtual std::unique_ptr<NetworkStateEstimator> Create(
      const FieldTrialsView* key_value_config) = 0;
  virtual ~NetworkStateEstimatorFactory() = default;
};
}

#endif
