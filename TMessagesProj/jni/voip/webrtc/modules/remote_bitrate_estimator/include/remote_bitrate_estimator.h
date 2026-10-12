/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This class estimates the incoming available bandwidth.

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_REMOTE_BITRATE_ESTIMATOR_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_REMOTE_BITRATE_ESTIMATOR_H_

#include <cstdint>
#include <vector>

#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {

class Clock;

class RemoteBitrateObserver {
 public:

  virtual void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                                       uint32_t bitrate) = 0;

  virtual ~RemoteBitrateObserver() {}
};

class RemoteBitrateEstimator : public CallStatsObserver {
 public:
  ~RemoteBitrateEstimator() override {}

  virtual void IncomingPacket(const RtpPacketReceived& rtp_packet) = 0;

  virtual void RemoveStream(uint32_t ssrc) = 0;

  virtual DataRate LatestEstimate() const = 0;

  virtual TimeDelta Process() = 0;

 protected:
  static constexpr TimeDelta kProcessInterval = TimeDelta::Millis(500);
  static constexpr TimeDelta kStreamTimeOut = TimeDelta::Seconds(2);
};

}

#endif
