/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_RTP_PACKET_PACER_H_
#define MODULES_PACING_RTP_PACKET_PACER_H_

#include <stdint.h>

#include <vector>

#include "absl/types/optional.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_packet_sender.h"

namespace webrtc {

class RtpPacketPacer {
 public:
  virtual ~RtpPacketPacer() = default;

  virtual void CreateProbeClusters(
      std::vector<ProbeClusterConfig> probe_cluster_configs) = 0;

  virtual void Pause() = 0;

  virtual void Resume() = 0;

  virtual void SetCongested(bool congested) = 0;

  virtual void SetPacingRates(DataRate pacing_rate, DataRate padding_rate) = 0;

  virtual TimeDelta OldestPacketWaitTime() const = 0;

  virtual DataSize QueueSizeData() const = 0;

  virtual absl::optional<Timestamp> FirstSentPacketTime() const = 0;

  virtual TimeDelta ExpectedQueueTime() const = 0;

  virtual void SetQueueTimeLimit(TimeDelta limit) = 0;

  virtual void SetAccountForAudioPackets(bool account_for_audio) = 0;
  virtual void SetIncludeOverhead() = 0;
  virtual void SetTransportOverhead(DataSize overhead_per_packet) = 0;
};

}
#endif
