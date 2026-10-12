/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_CONFIG_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_CONFIG_H_

#include <string>

#include "api/array_view.h"
#include "api/rtp_headers.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_bitrate_allocation.h"
#include "modules/rtp_rtcp/include/report_block_data.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/ntp_time.h"

namespace webrtc {
class ReceiveStatisticsProvider;

class MediaReceiverRtcpObserver {
 public:
  virtual ~MediaReceiverRtcpObserver() = default;

  virtual void OnSenderReport(uint32_t sender_ssrc,
                              NtpTime ntp_time,
                              uint32_t rtp_time) {}
  virtual void OnBye(uint32_t sender_ssrc) {}
  virtual void OnBitrateAllocation(uint32_t sender_ssrc,
                                   const VideoBitrateAllocation& allocation) {}
};

class RtpStreamRtcpHandler {
 public:
  virtual ~RtpStreamRtcpHandler() = default;

  class RtpStats {
   public:
    RtpStats() = default;
    RtpStats(const RtpStats&) = default;
    RtpStats& operator=(const RtpStats&) = default;
    ~RtpStats() = default;

    size_t num_sent_packets() const { return num_sent_packets_; }
    size_t num_sent_bytes() const { return num_sent_bytes_; }
    Timestamp last_capture_time() const { return last_capture_time_; }
    uint32_t last_rtp_timestamp() const { return last_rtp_timestamp_; }
    int last_clock_rate() const { return last_clock_rate_; }

    void set_num_sent_packets(size_t v) { num_sent_packets_ = v; }
    void set_num_sent_bytes(size_t v) { num_sent_bytes_ = v; }
    void set_last_capture_time(Timestamp v) { last_capture_time_ = v; }
    void set_last_rtp_timestamp(uint32_t v) { last_rtp_timestamp_ = v; }
    void set_last_clock_rate(int v) { last_clock_rate_ = v; }

   private:
    size_t num_sent_packets_ = 0;
    size_t num_sent_bytes_ = 0;
    Timestamp last_capture_time_ = Timestamp::Zero();
    uint32_t last_rtp_timestamp_ = 0;
    int last_clock_rate_ = 90'000;
  };
  virtual RtpStats SentStats() = 0;

  virtual void OnNack(uint32_t sender_ssrc,
                      rtc::ArrayView<const uint16_t> sequence_numbers) {}
  virtual void OnFir(uint32_t sender_ssrc) {}
  virtual void OnPli(uint32_t sender_ssrc) {}

  virtual void OnReport(const ReportBlockData& report_block) {}
};

struct RtcpTransceiverConfig {
  RtcpTransceiverConfig();
  RtcpTransceiverConfig(const RtcpTransceiverConfig&);
  RtcpTransceiverConfig& operator=(const RtcpTransceiverConfig&);
  ~RtcpTransceiverConfig();

  bool Validate() const;

  std::string debug_id;

  uint32_t feedback_ssrc = 1;

  std::string cname;

  size_t max_packet_size = 1200;

  Clock* clock = nullptr;

  std::function<void(rtc::ArrayView<const uint8_t>)> rtcp_transport;

  TaskQueueBase* task_queue = nullptr;

  ReceiveStatisticsProvider* receive_statistics = nullptr;

  NetworkLinkRtcpObserver* network_link_observer = nullptr;

  RtcpMode rtcp_mode = RtcpMode::kCompound;

  bool initial_ready_to_send = true;

  TimeDelta initial_report_delay = TimeDelta::Millis(500);

  TimeDelta report_period = TimeDelta::Seconds(1);

  bool schedule_periodic_compound_packets = true;

  bool non_sender_rtt_measurement = false;

  bool reply_to_non_sender_rtt_measurement = true;

  bool reply_to_non_sender_rtt_mesaurments_on_all_ssrcs = true;

  bool send_remb_on_change = false;
};

}

#endif
