/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_REPORT_BLOCK_DATA_H_
#define MODULES_RTP_RTCP_INCLUDE_REPORT_BLOCK_DATA_H_

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"

namespace webrtc {

class ReportBlockData {
 public:
  ReportBlockData() = default;

  ReportBlockData(const ReportBlockData&) = default;
  ReportBlockData& operator=(const ReportBlockData&) = default;

  uint32_t sender_ssrc() const { return sender_ssrc_; }

  uint32_t source_ssrc() const { return source_ssrc_; }

  float fraction_lost() const { return fraction_lost_raw() / 256.0; }

  uint8_t fraction_lost_raw() const { return fraction_lost_raw_; }

  int cumulative_lost() const { return cumulative_lost_; }

  uint32_t extended_highest_sequence_number() const {
    return extended_highest_sequence_number_;
  }

  uint32_t jitter() const { return jitter_; }

  TimeDelta jitter(int rtp_clock_rate_hz) const;

  Timestamp report_block_timestamp_utc() const {
    return report_block_timestamp_utc_;
  }

  TimeDelta last_rtt() const { return last_rtt_; }
  TimeDelta sum_rtts() const { return sum_rtt_; }
  size_t num_rtts() const { return num_rtts_; }
  bool has_rtt() const { return num_rtts_ != 0; }

  void set_sender_ssrc(uint32_t ssrc) { sender_ssrc_ = ssrc; }
  void set_source_ssrc(uint32_t ssrc) { source_ssrc_ = ssrc; }
  void set_fraction_lost_raw(uint8_t lost) { fraction_lost_raw_ = lost; }
  void set_cumulative_lost(int lost) { cumulative_lost_ = lost; }
  void set_extended_highest_sequence_number(uint32_t sn) {
    extended_highest_sequence_number_ = sn;
  }
  void set_jitter(uint32_t jitter) { jitter_ = jitter; }
  void set_report_block_timestamp_utc(Timestamp arrival_time) {
    report_block_timestamp_utc_ = arrival_time;
  }

  void SetReportBlock(uint32_t sender_ssrc,
                      const rtcp::ReportBlock& report_block,
                      Timestamp report_block_timestamp_utc);
  void AddRoundTripTimeSample(TimeDelta rtt);

 private:
  uint32_t sender_ssrc_ = 0;
  uint32_t source_ssrc_ = 0;
  uint8_t fraction_lost_raw_ = 0;
  int32_t cumulative_lost_ = 0;
  uint32_t extended_highest_sequence_number_ = 0;
  uint32_t jitter_ = 0;
  Timestamp report_block_timestamp_utc_ = Timestamp::Zero();
  TimeDelta last_rtt_ = TimeDelta::Zero();
  TimeDelta sum_rtt_ = TimeDelta::Zero();
  size_t num_rtts_ = 0;
};

class ReportBlockDataObserver {
 public:
  virtual ~ReportBlockDataObserver() = default;

  virtual void OnReportBlockDataUpdated(ReportBlockData report_block_data) = 0;
};

}

#endif
