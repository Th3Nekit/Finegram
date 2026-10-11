/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_RECEIVE_STATISTICS_H_
#define MODULES_RTP_RTCP_INCLUDE_RECEIVE_STATISTICS_H_

#include <map>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "call/rtp_packet_sink_interface.h"
#include "modules/rtp_rtcp/include/rtcp_statistics.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"

namespace webrtc {

class Clock;

class ReceiveStatisticsProvider {
 public:
  virtual ~ReceiveStatisticsProvider() = default;

  virtual std::vector<rtcp::ReportBlock> RtcpReportBlocks(
      size_t max_blocks) = 0;
};

class StreamStatistician {
 public:
  virtual ~StreamStatistician();

  virtual RtpReceiveStats GetStats() const = 0;

  virtual absl::optional<int> GetFractionLostInPercent() const = 0;

  virtual StreamDataCounters GetReceiveStreamDataCounters() const = 0;

  virtual uint32_t BitrateReceived() const = 0;
};

class ReceiveStatistics : public ReceiveStatisticsProvider,
                          public RtpPacketSinkInterface {
 public:
  ~ReceiveStatistics() override = default;

  static std::unique_ptr<ReceiveStatistics> Create(Clock* clock);

  static std::unique_ptr<ReceiveStatistics> CreateThreadCompatible(
      Clock* clock);

  virtual StreamStatistician* GetStatistician(uint32_t ssrc) const = 0;

  virtual void SetMaxReorderingThreshold(int max_reordering_threshold) = 0;

  virtual void SetMaxReorderingThreshold(uint32_t ssrc,
                                         int max_reordering_threshold) = 0;

  virtual void EnableRetransmitDetection(uint32_t ssrc, bool enable) = 0;
};

}
#endif
