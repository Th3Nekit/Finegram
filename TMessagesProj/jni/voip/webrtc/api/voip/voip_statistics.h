/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VOIP_VOIP_STATISTICS_H_
#define API_VOIP_VOIP_STATISTICS_H_

#include "api/neteq/neteq.h"
#include "api/voip/voip_base.h"

namespace webrtc {

struct IngressStatistics {

  NetEqLifetimeStatistics neteq_stats;

  double total_duration = 0.0;
};

struct RemoteRtcpStatistics {

  double jitter = 0.0;

  int64_t packets_lost = 0;

  double fraction_lost = 0.0;

  absl::optional<double> round_trip_time;

  int64_t last_report_received_timestamp_ms;
};

struct ChannelStatistics {

  uint64_t packets_sent = 0;

  uint64_t bytes_sent = 0;

  uint64_t packets_received = 0;

  uint64_t bytes_received = 0;

  double jitter = 0.0;

  int64_t packets_lost = 0;

  absl::optional<uint32_t> remote_ssrc;

  absl::optional<RemoteRtcpStatistics> remote_rtcp;
};

class VoipStatistics {
 public:

  virtual VoipResult GetIngressStatistics(ChannelId channel_id,
                                          IngressStatistics& ingress_stats) = 0;

  virtual VoipResult GetChannelStatistics(ChannelId channel_id,
                                          ChannelStatistics& channel_stats) = 0;

 protected:
  virtual ~VoipStatistics() = default;
};

}

#endif
