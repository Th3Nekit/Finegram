/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_PCC_MONITOR_INTERVAL_H_
#define MODULES_CONGESTION_CONTROLLER_PCC_MONITOR_INTERVAL_H_

#include <vector>

#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {
namespace pcc {

class PccMonitorInterval {
 public:
  PccMonitorInterval(DataRate target_sending_rate,
                     Timestamp start_time,
                     TimeDelta duration);
  ~PccMonitorInterval();
  PccMonitorInterval(const PccMonitorInterval& other);
  void OnPacketsFeedback(const std::vector<PacketResult>& packets_results);

  bool IsFeedbackCollectionDone() const;
  Timestamp GetEndTime() const;

  double GetLossRate() const;

  double ComputeDelayGradient(double delay_gradient_threshold) const;
  DataRate GetTargetSendingRate() const;

  DataRate GetTransmittedPacketsRate() const;

 private:
  struct ReceivedPacket {
    TimeDelta delay;
    Timestamp sent_time;
  };

  DataRate target_sending_rate_;

  Timestamp start_time_;
  TimeDelta interval_duration_;

  std::vector<ReceivedPacket> received_packets_;
  std::vector<Timestamp> lost_packets_sent_time_;
  DataSize received_packets_size_;
  bool feedback_collection_done_;
};

}
}

#endif
