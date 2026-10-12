/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_
#define MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "rtc_base/bitrate_tracker.h"
#include "rtc_base/containers/flat_map.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class StreamStatisticianImplInterface : public StreamStatistician {
 public:
  virtual ~StreamStatisticianImplInterface() = default;
  virtual void MaybeAppendReportBlockAndReset(
      std::vector<rtcp::ReportBlock>& report_blocks) = 0;
  virtual void SetMaxReorderingThreshold(int max_reordering_threshold) = 0;
  virtual void EnableRetransmitDetection(bool enable) = 0;
  virtual void UpdateCounters(const RtpPacketReceived& packet) = 0;
};

class StreamStatisticianImpl : public StreamStatisticianImplInterface {
 public:
  StreamStatisticianImpl(uint32_t ssrc,
                         Clock* clock,
                         int max_reordering_threshold);
  ~StreamStatisticianImpl() override;

  RtpReceiveStats GetStats() const override;
  absl::optional<int> GetFractionLostInPercent() const override;
  StreamDataCounters GetReceiveStreamDataCounters() const override;
  uint32_t BitrateReceived() const override;

  void MaybeAppendReportBlockAndReset(
      std::vector<rtcp::ReportBlock>& report_blocks) override;
  void SetMaxReorderingThreshold(int max_reordering_threshold) override;
  void EnableRetransmitDetection(bool enable) override;

  void UpdateCounters(const RtpPacketReceived& packet) override;

 private:
  bool IsRetransmitOfOldPacket(const RtpPacketReceived& packet,
                               Timestamp now) const;
  void UpdateJitter(const RtpPacketReceived& packet, Timestamp receive_time);
  void ReviseFrequencyAndJitter(int payload_type_frequency);

  bool UpdateOutOfOrder(const RtpPacketReceived& packet,
                        int64_t sequence_number,
                        Timestamp now);

  bool ReceivedRtpPacket() const { return last_receive_time_.has_value(); }

  const uint32_t ssrc_;
  Clock* const clock_;

  const TimeDelta delta_internal_unix_epoch_;
  BitrateTracker incoming_bitrate_;

  int max_reordering_threshold_;
  bool enable_retransmit_detection_;
  bool cumulative_loss_is_capped_;

  uint32_t jitter_q4_;

  int32_t cumulative_loss_;

  int32_t cumulative_loss_rtcp_offset_;

  absl::optional<Timestamp> last_receive_time_;
  uint32_t last_received_timestamp_;
  RtpSequenceNumberUnwrapper seq_unwrapper_;
  int64_t received_seq_first_;
  int64_t received_seq_max_;

  absl::optional<uint16_t> received_seq_out_of_order_;

  StreamDataCounters receive_counters_;

  int32_t last_report_cumulative_loss_;
  int64_t last_report_seq_max_;

  int last_payload_type_frequency_;
};

class StreamStatisticianLocked : public StreamStatisticianImplInterface {
 public:
  StreamStatisticianLocked(uint32_t ssrc,
                           Clock* clock,
                           int max_reordering_threshold)
      : impl_(ssrc, clock, max_reordering_threshold) {}
  ~StreamStatisticianLocked() override = default;

  RtpReceiveStats GetStats() const override {
    MutexLock lock(&stream_lock_);
    return impl_.GetStats();
  }
  absl::optional<int> GetFractionLostInPercent() const override {
    MutexLock lock(&stream_lock_);
    return impl_.GetFractionLostInPercent();
  }
  StreamDataCounters GetReceiveStreamDataCounters() const override {
    MutexLock lock(&stream_lock_);
    return impl_.GetReceiveStreamDataCounters();
  }
  uint32_t BitrateReceived() const override {
    MutexLock lock(&stream_lock_);
    return impl_.BitrateReceived();
  }
  void MaybeAppendReportBlockAndReset(
      std::vector<rtcp::ReportBlock>& report_blocks) override {
    MutexLock lock(&stream_lock_);
    impl_.MaybeAppendReportBlockAndReset(report_blocks);
  }
  void SetMaxReorderingThreshold(int max_reordering_threshold) override {
    MutexLock lock(&stream_lock_);
    return impl_.SetMaxReorderingThreshold(max_reordering_threshold);
  }
  void EnableRetransmitDetection(bool enable) override {
    MutexLock lock(&stream_lock_);
    return impl_.EnableRetransmitDetection(enable);
  }
  void UpdateCounters(const RtpPacketReceived& packet) override {
    MutexLock lock(&stream_lock_);
    return impl_.UpdateCounters(packet);
  }

 private:
  mutable Mutex stream_lock_;
  StreamStatisticianImpl impl_ RTC_GUARDED_BY(&stream_lock_);
};

class ReceiveStatisticsImpl : public ReceiveStatistics {
 public:
  ReceiveStatisticsImpl(
      Clock* clock,
      std::function<std::unique_ptr<StreamStatisticianImplInterface>(
          uint32_t ssrc,
          Clock* clock,
          int max_reordering_threshold)> stream_statistician_factory);
  ~ReceiveStatisticsImpl() override = default;

  std::vector<rtcp::ReportBlock> RtcpReportBlocks(size_t max_blocks) override;

  void OnRtpPacket(const RtpPacketReceived& packet) override;

  StreamStatistician* GetStatistician(uint32_t ssrc) const override;
  void SetMaxReorderingThreshold(int max_reordering_threshold) override;
  void SetMaxReorderingThreshold(uint32_t ssrc,
                                 int max_reordering_threshold) override;
  void EnableRetransmitDetection(uint32_t ssrc, bool enable) override;

 private:
  StreamStatisticianImplInterface* GetOrCreateStatistician(uint32_t ssrc);

  Clock* const clock_;
  std::function<std::unique_ptr<StreamStatisticianImplInterface>(
      uint32_t ssrc,
      Clock* clock,
      int max_reordering_threshold)>
      stream_statistician_factory_;

  size_t last_returned_ssrc_idx_;
  std::vector<uint32_t> all_ssrcs_;
  int max_reordering_threshold_;
  flat_map<uint32_t         , std::unique_ptr<StreamStatisticianImplInterface>>
      statisticians_;
};

class ReceiveStatisticsLocked : public ReceiveStatistics {
 public:
  explicit ReceiveStatisticsLocked(
      Clock* clock,
      std::function<std::unique_ptr<StreamStatisticianImplInterface>(
          uint32_t ssrc,
          Clock* clock,
          int max_reordering_threshold)> stream_statitician_factory)
      : impl_(clock, std::move(stream_statitician_factory)) {}
  ~ReceiveStatisticsLocked() override = default;
  std::vector<rtcp::ReportBlock> RtcpReportBlocks(size_t max_blocks) override {
    MutexLock lock(&receive_statistics_lock_);
    return impl_.RtcpReportBlocks(max_blocks);
  }
  void OnRtpPacket(const RtpPacketReceived& packet) override {
    MutexLock lock(&receive_statistics_lock_);
    return impl_.OnRtpPacket(packet);
  }
  StreamStatistician* GetStatistician(uint32_t ssrc) const override {
    MutexLock lock(&receive_statistics_lock_);
    return impl_.GetStatistician(ssrc);
  }
  void SetMaxReorderingThreshold(int max_reordering_threshold) override {
    MutexLock lock(&receive_statistics_lock_);
    return impl_.SetMaxReorderingThreshold(max_reordering_threshold);
  }
  void SetMaxReorderingThreshold(uint32_t ssrc,
                                 int max_reordering_threshold) override {
    MutexLock lock(&receive_statistics_lock_);
    return impl_.SetMaxReorderingThreshold(ssrc, max_reordering_threshold);
  }
  void EnableRetransmitDetection(uint32_t ssrc, bool enable) override {
    MutexLock lock(&receive_statistics_lock_);
    return impl_.EnableRetransmitDetection(ssrc, enable);
  }

 private:
  mutable Mutex receive_statistics_lock_;
  ReceiveStatisticsImpl impl_ RTC_GUARDED_BY(&receive_statistics_lock_);
};

}
#endif
