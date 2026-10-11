/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/remote_estimator_proxy.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

#include "absl/types/optional.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "modules/rtp_rtcp/source/rtcp_packet/remote_estimate.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {

constexpr TimeDelta kBackWindow = TimeDelta::Millis(500);
constexpr TimeDelta kMinInterval = TimeDelta::Millis(50);
constexpr TimeDelta kMaxInterval = TimeDelta::Millis(250);
constexpr TimeDelta kDefaultInterval = TimeDelta::Millis(100);

TimeDelta GetAbsoluteSendTimeDelta(uint32_t new_sendtime,
                                   uint32_t previous_sendtime) {
  static constexpr uint32_t kWrapAroundPeriod = 0x0100'0000;
  RTC_DCHECK_LT(new_sendtime, kWrapAroundPeriod);
  RTC_DCHECK_LT(previous_sendtime, kWrapAroundPeriod);
  uint32_t delta = (new_sendtime - previous_sendtime) % kWrapAroundPeriod;
  if (delta >= kWrapAroundPeriod / 2) {

    delta = (previous_sendtime - new_sendtime) % kWrapAroundPeriod;
    return TimeDelta::Micros(int64_t{delta} * -1'000'000 / (1 << 18));
  }
  return TimeDelta::Micros(int64_t{delta} * 1'000'000 / (1 << 18));
}
}

RemoteEstimatorProxy::RemoteEstimatorProxy(
    TransportFeedbackSender feedback_sender,
    NetworkStateEstimator* network_state_estimator)
    : feedback_sender_(std::move(feedback_sender)),
      last_process_time_(Timestamp::MinusInfinity()),
      network_state_estimator_(network_state_estimator),
      media_ssrc_(0),
      feedback_packet_count_(0),
      packet_overhead_(DataSize::Zero()),
      send_interval_(kDefaultInterval),
      send_periodic_feedback_(true),
      previous_abs_send_time_(0),
      abs_send_timestamp_(Timestamp::Zero()),
      last_arrival_time_with_abs_send_time_(Timestamp::MinusInfinity()) {
  RTC_LOG(LS_INFO)
      << "Maximum interval between transport feedback RTCP messages: "
      << kMaxInterval;
}

RemoteEstimatorProxy::~RemoteEstimatorProxy() {}

void RemoteEstimatorProxy::MaybeCullOldPackets(int64_t sequence_number,
                                               Timestamp arrival_time) {
  if (periodic_window_start_seq_ >=
          packet_arrival_times_.end_sequence_number() &&
      arrival_time - Timestamp::Zero() >= kBackWindow) {

    packet_arrival_times_.RemoveOldPackets(sequence_number,
                                           arrival_time - kBackWindow);
  }
}

void RemoteEstimatorProxy::IncomingPacket(const RtpPacketReceived& packet) {
  if (packet.arrival_time().IsInfinite()) {
    RTC_LOG(LS_WARNING) << "Arrival time not set.";
    return;
  }

  uint16_t seqnum = 0;
  absl::optional<FeedbackRequest> feedback_request;
  if (!packet.GetExtension<TransportSequenceNumber>(&seqnum) &&
      !packet.GetExtension<TransportSequenceNumberV2>(&seqnum,
                                                      &feedback_request)) {

    RTC_DCHECK(packet.HasExtension<TransportSequenceNumber>() ||
               packet.HasExtension<TransportSequenceNumberV2>())
        << " Expected transport sequence number.";
    return;
  }

  MutexLock lock(&lock_);
  send_periodic_feedback_ = packet.HasExtension<TransportSequenceNumber>();

  media_ssrc_ = packet.Ssrc();
  int64_t seq = unwrapper_.Unwrap(seqnum);

  if (send_periodic_feedback_) {
    MaybeCullOldPackets(seq, packet.arrival_time());

    if (!periodic_window_start_seq_ || seq < *periodic_window_start_seq_) {
      periodic_window_start_seq_ = seq;
    }
  }

  if (packet_arrival_times_.has_received(seq)) {
    return;
  }

  packet_arrival_times_.AddPacket(seq, packet.arrival_time());

  if (periodic_window_start_seq_ <
      packet_arrival_times_.begin_sequence_number()) {
    periodic_window_start_seq_ = packet_arrival_times_.begin_sequence_number();
  }

  if (feedback_request.has_value()) {

    SendFeedbackOnRequest(seq, *feedback_request);
  }

  absl::optional<uint32_t> absolute_send_time_24bits =
      packet.GetExtension<AbsoluteSendTime>();
  if (network_state_estimator_ && absolute_send_time_24bits.has_value()) {
    PacketResult packet_result;
    packet_result.receive_time = packet.arrival_time();
    if (packet.arrival_time() - last_arrival_time_with_abs_send_time_ <
        TimeDelta::Seconds(10)) {
      abs_send_timestamp_ += GetAbsoluteSendTimeDelta(
          *absolute_send_time_24bits, previous_abs_send_time_);
    } else {
      abs_send_timestamp_ = packet.arrival_time();
    }
    last_arrival_time_with_abs_send_time_ = packet.arrival_time();
    previous_abs_send_time_ = *absolute_send_time_24bits;
    packet_result.sent_packet.send_time = abs_send_timestamp_;
    packet_result.sent_packet.size =
        DataSize::Bytes(packet.size()) + packet_overhead_;
    packet_result.sent_packet.sequence_number = seq;
    network_state_estimator_->OnReceivedPacket(packet_result);
  }
}

TimeDelta RemoteEstimatorProxy::Process(Timestamp now) {
  MutexLock lock(&lock_);
  if (!send_periodic_feedback_) {

    return TimeDelta::PlusInfinity();
  }
  Timestamp next_process_time = last_process_time_ + send_interval_;
  if (now >= next_process_time) {
    last_process_time_ = now;
    SendPeriodicFeedbacks();
    return send_interval_;
  }

  return next_process_time - now;
}

void RemoteEstimatorProxy::OnBitrateChanged(int bitrate_bps) {

  constexpr DataSize kTwccReportSize = DataSize::Bytes(20 + 8 + 10 + 30);
  constexpr DataRate kMinTwccRate = kTwccReportSize / kMaxInterval;

  DataRate twcc_bitrate = DataRate::BitsPerSec(0.05 * bitrate_bps);

  TimeDelta send_interval =
      twcc_bitrate <= kMinTwccRate
          ? kMaxInterval
          : std::max(kTwccReportSize / twcc_bitrate, kMinInterval);

  MutexLock lock(&lock_);
  send_interval_ = send_interval;
}

void RemoteEstimatorProxy::SetTransportOverhead(DataSize overhead_per_packet) {
  MutexLock lock(&lock_);
  packet_overhead_ = overhead_per_packet;
}

void RemoteEstimatorProxy::SendPeriodicFeedbacks() {

  if (!periodic_window_start_seq_)
    return;

  std::unique_ptr<rtcp::RemoteEstimate> remote_estimate;
  if (network_state_estimator_) {
    absl::optional<NetworkStateEstimate> state_estimate =
        network_state_estimator_->GetCurrentEstimate();
    if (state_estimate) {
      remote_estimate = std::make_unique<rtcp::RemoteEstimate>();
      remote_estimate->SetEstimate(state_estimate.value());
    }
  }

  int64_t packet_arrival_times_end_seq =
      packet_arrival_times_.end_sequence_number();
  while (periodic_window_start_seq_ < packet_arrival_times_end_seq) {
    auto feedback_packet = MaybeBuildFeedbackPacket(
                               true, *periodic_window_start_seq_,
        packet_arrival_times_end_seq,
                               true);

    if (feedback_packet == nullptr) {
      break;
    }

    RTC_DCHECK(feedback_sender_ != nullptr);

    std::vector<std::unique_ptr<rtcp::RtcpPacket>> packets;
    if (remote_estimate) {
      packets.push_back(std::move(remote_estimate));
    }
    packets.push_back(std::move(feedback_packet));

    feedback_sender_(std::move(packets));

  }
}

void RemoteEstimatorProxy::SendFeedbackOnRequest(
    int64_t sequence_number,
    const FeedbackRequest& feedback_request) {
  if (feedback_request.sequence_count == 0) {
    return;
  }

  int64_t first_sequence_number =
      sequence_number - feedback_request.sequence_count + 1;

  auto feedback_packet = MaybeBuildFeedbackPacket(
      feedback_request.include_timestamps, first_sequence_number,
      sequence_number + 1,                        false);

  if (feedback_packet == nullptr) {
    return;
  }

  packet_arrival_times_.EraseTo(first_sequence_number);

  RTC_DCHECK(feedback_sender_ != nullptr);
  std::vector<std::unique_ptr<rtcp::RtcpPacket>> packets;
  packets.push_back(std::move(feedback_packet));
  feedback_sender_(std::move(packets));
}

std::unique_ptr<rtcp::TransportFeedback>
RemoteEstimatorProxy::MaybeBuildFeedbackPacket(
    bool include_timestamps,
    int64_t begin_sequence_number_inclusive,
    int64_t end_sequence_number_exclusive,
    bool is_periodic_update) {
  RTC_DCHECK_LT(begin_sequence_number_inclusive, end_sequence_number_exclusive);

  int64_t start_seq =
      packet_arrival_times_.clamp(begin_sequence_number_inclusive);

  int64_t end_seq = packet_arrival_times_.clamp(end_sequence_number_exclusive);

  std::unique_ptr<rtcp::TransportFeedback> feedback_packet;

  int64_t next_sequence_number = begin_sequence_number_inclusive;

  for (int64_t seq = start_seq; seq < end_seq; ++seq) {
    PacketArrivalTimeMap::PacketArrivalTime packet =
        packet_arrival_times_.FindNextAtOrAfter(seq);
    seq = packet.sequence_number;
    if (seq >= end_seq) {
      break;
    }

    if (feedback_packet == nullptr) {
      feedback_packet =
          std::make_unique<rtcp::TransportFeedback>(include_timestamps);
      feedback_packet->SetMediaSsrc(media_ssrc_);

      static constexpr int kMaxMissingSequenceNumbers = 0x7FFE;
      int64_t base_sequence_number = std::max(begin_sequence_number_inclusive,
                                              seq - kMaxMissingSequenceNumbers);

      feedback_packet->SetBase(static_cast<uint16_t>(base_sequence_number),
                               packet.arrival_time);
      feedback_packet->SetFeedbackSequenceNumber(feedback_packet_count_++);

      if (!feedback_packet->AddReceivedPacket(static_cast<uint16_t>(seq),
                                              packet.arrival_time)) {

        RTC_DCHECK_NOTREACHED()
            << "Failed to create an RTCP transport feedback with base sequence "
               "number "
            << base_sequence_number << " and 1st received " << seq;
        periodic_window_start_seq_ = seq;
        return nullptr;
      }
    } else {
      if (!feedback_packet->AddReceivedPacket(static_cast<uint16_t>(seq),
                                              packet.arrival_time)) {

        break;
      }
    }

    next_sequence_number = seq + 1;
  }
  if (is_periodic_update) {
    periodic_window_start_seq_ = next_sequence_number;
  }
  return feedback_packet;
}

}
