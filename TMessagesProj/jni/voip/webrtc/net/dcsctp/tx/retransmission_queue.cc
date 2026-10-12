/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/retransmission_queue.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/common/math.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "net/dcsctp/packet/chunk/data_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_common.h"
#include "net/dcsctp/packet/chunk/idata_chunk.h"
#include "net/dcsctp/packet/chunk/iforward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/sack_chunk.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/timer/timer.h"
#include "net/dcsctp/tx/outstanding_data.h"
#include "net/dcsctp/tx/send_queue.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/str_join.h"
#include "rtc_base/strings/string_builder.h"

namespace dcsctp {
namespace {
using ::webrtc::TimeDelta;
using ::webrtc::Timestamp;

constexpr float kMinBytesRequiredToSendFactor = 0.9;
}

RetransmissionQueue::RetransmissionQueue(
    absl::string_view log_prefix,
    DcSctpSocketCallbacks* callbacks,
    TSN my_initial_tsn,
    size_t a_rwnd,
    SendQueue& send_queue,
    std::function<void(TimeDelta rtt)> on_new_rtt,
    std::function<void()> on_clear_retransmission_counter,
    Timer& t3_rtx,
    const DcSctpOptions& options,
    bool supports_partial_reliability,
    bool use_message_interleaving)
    : callbacks_(*callbacks),
      options_(options),
      min_bytes_required_to_send_(options.mtu * kMinBytesRequiredToSendFactor),
      partial_reliability_(supports_partial_reliability),
      log_prefix_(log_prefix),
      data_chunk_header_size_(use_message_interleaving
                                  ? IDataChunk::kHeaderSize
                                  : DataChunk::kHeaderSize),
      on_new_rtt_(std::move(on_new_rtt)),
      on_clear_retransmission_counter_(
          std::move(on_clear_retransmission_counter)),
      t3_rtx_(t3_rtx),
      cwnd_(options_.cwnd_mtus_initial * options_.mtu),
      rwnd_(a_rwnd),

      ssthresh_(rwnd_),
      partial_bytes_acked_(0),
      send_queue_(send_queue),
      outstanding_data_(
          data_chunk_header_size_,
          tsn_unwrapper_.Unwrap(TSN(*my_initial_tsn - 1)),
          [this](StreamID stream_id, OutgoingMessageId message_id) {
            return send_queue_.Discard(stream_id, message_id);
          }) {}

bool RetransmissionQueue::IsConsistent() const {
  return true;
}

size_t RetransmissionQueue::GetSerializedChunkSize(const Data& data) const {
  return RoundUpTo4(data_chunk_header_size_ + data.size());
}

void RetransmissionQueue::MaybeExitFastRecovery(
    UnwrappedTSN cumulative_tsn_ack) {

  if (fast_recovery_exit_tsn_.has_value() &&
      cumulative_tsn_ack >= *fast_recovery_exit_tsn_) {
    RTC_DLOG(LS_VERBOSE) << log_prefix_
                         << "exit_point=" << *fast_recovery_exit_tsn_->Wrap()
                         << " reached - exiting fast recovery";
    fast_recovery_exit_tsn_ = absl::nullopt;
  }
}

void RetransmissionQueue::HandleIncreasedCumulativeTsnAck(
    size_t unacked_bytes,
    size_t total_bytes_acked) {

  bool is_fully_utilized = unacked_bytes + options_.mtu >= cwnd_;
  size_t old_cwnd = cwnd_;
  if (phase() == CongestionAlgorithmPhase::kSlowStart) {
    if (is_fully_utilized && !is_in_fast_recovery()) {

      cwnd_ += std::min(total_bytes_acked, options_.mtu);
      RTC_DLOG(LS_VERBOSE) << log_prefix_ << "SS increase cwnd=" << cwnd_
                           << " (" << old_cwnd << ")";
    }
  } else if (phase() == CongestionAlgorithmPhase::kCongestionAvoidance) {

    size_t old_pba = partial_bytes_acked_;
    partial_bytes_acked_ += total_bytes_acked;

    if (partial_bytes_acked_ >= cwnd_ && is_fully_utilized) {

      partial_bytes_acked_ -= cwnd_;
      cwnd_ += options_.mtu;
      RTC_DLOG(LS_VERBOSE) << log_prefix_ << "CA increase cwnd=" << cwnd_
                           << " (" << old_cwnd << ") ssthresh=" << ssthresh_
                           << ", pba=" << partial_bytes_acked_ << " ("
                           << old_pba << ")";
    } else {
      RTC_DLOG(LS_VERBOSE) << log_prefix_ << "CA unchanged cwnd=" << cwnd_
                           << " (" << old_cwnd << ") ssthresh=" << ssthresh_
                           << ", pba=" << partial_bytes_acked_ << " ("
                           << old_pba << ")";
    }
  }
}

void RetransmissionQueue::HandlePacketLoss(UnwrappedTSN highest_tsn_acked) {
  if (!is_in_fast_recovery()) {

    size_t old_cwnd = cwnd_;
    size_t old_pba = partial_bytes_acked_;
    ssthresh_ = std::max(cwnd_ / 2, options_.cwnd_mtus_min * options_.mtu);
    cwnd_ = ssthresh_;
    partial_bytes_acked_ = 0;

    RTC_DLOG(LS_VERBOSE) << log_prefix_
                         << "packet loss detected (not fast recovery). cwnd="
                         << cwnd_ << " (" << old_cwnd
                         << "), ssthresh=" << ssthresh_
                         << ", pba=" << partial_bytes_acked_ << " (" << old_pba
                         << ")";

    fast_recovery_exit_tsn_ = outstanding_data_.highest_outstanding_tsn();
    RTC_DLOG(LS_VERBOSE) << log_prefix_
                         << "fast recovery initiated with exit_point="
                         << *fast_recovery_exit_tsn_->Wrap();
  } else {

    RTC_DLOG(LS_VERBOSE) << log_prefix_
                         << "packet loss detected (fast recovery). No changes.";
  }
}

void RetransmissionQueue::UpdateReceiverWindow(uint32_t a_rwnd) {
  rwnd_ = outstanding_data_.unacked_bytes() >= a_rwnd
              ? 0
              : a_rwnd - outstanding_data_.unacked_bytes();
}

void RetransmissionQueue::StartT3RtxTimerIfOutstandingData() {

  if (outstanding_data_.empty()) {

  } else {

    if (!t3_rtx_.is_running()) {
      t3_rtx_.Start();
    }
  }
}

bool RetransmissionQueue::IsSackValid(const SackChunk& sack) const {

  UnwrappedTSN cumulative_tsn_ack =
      tsn_unwrapper_.PeekUnwrap(sack.cumulative_tsn_ack());
  if (cumulative_tsn_ack < outstanding_data_.last_cumulative_tsn_ack()) {

    return false;
  } else if (cumulative_tsn_ack > outstanding_data_.highest_outstanding_tsn()) {
    return false;
  }
  return true;
}

bool RetransmissionQueue::HandleSack(Timestamp now, const SackChunk& sack) {
  if (!IsSackValid(sack)) {
    return false;
  }

  UnwrappedTSN old_last_cumulative_tsn_ack =
      outstanding_data_.last_cumulative_tsn_ack();
  size_t old_unacked_bytes = outstanding_data_.unacked_bytes();
  size_t old_rwnd = rwnd_;
  UnwrappedTSN cumulative_tsn_ack =
      tsn_unwrapper_.Unwrap(sack.cumulative_tsn_ack());

  if (sack.gap_ack_blocks().empty()) {
    UpdateRTT(now, cumulative_tsn_ack);
  }

  MaybeExitFastRecovery(cumulative_tsn_ack);

  OutstandingData::AckInfo ack_info = outstanding_data_.HandleSack(
      cumulative_tsn_ack, sack.gap_ack_blocks(), is_in_fast_recovery());

  for (LifecycleId lifecycle_id : ack_info.acked_lifecycle_ids) {
    RTC_DLOG(LS_VERBOSE) << "Triggering OnLifecycleMessageDelivered("
                         << lifecycle_id.value() << ")";
    callbacks_.OnLifecycleMessageDelivered(lifecycle_id);
    callbacks_.OnLifecycleEnd(lifecycle_id);
  }
  for (LifecycleId lifecycle_id : ack_info.abandoned_lifecycle_ids) {
    RTC_DLOG(LS_VERBOSE) << "Triggering OnLifecycleMessageExpired("
                         << lifecycle_id.value() << ", true)";
    callbacks_.OnLifecycleMessageExpired(lifecycle_id,
                                                             true);
    callbacks_.OnLifecycleEnd(lifecycle_id);
  }

  UpdateReceiverWindow(sack.a_rwnd());

  RTC_DLOG(LS_VERBOSE) << log_prefix_ << "Received SACK, cum_tsn_ack="
                       << *cumulative_tsn_ack.Wrap() << " ("
                       << *old_last_cumulative_tsn_ack.Wrap()
                       << "), unacked_bytes="
                       << outstanding_data_.unacked_bytes() << " ("
                       << old_unacked_bytes << "), rwnd=" << rwnd_ << " ("
                       << old_rwnd << ")";

  if (cumulative_tsn_ack > old_last_cumulative_tsn_ack) {

    t3_rtx_.Stop();

    HandleIncreasedCumulativeTsnAck(old_unacked_bytes, ack_info.bytes_acked);
  }

  if (ack_info.has_packet_loss) {
    HandlePacketLoss(ack_info.highest_tsn_acked);
  }

  if (ack_info.bytes_acked > 0) {
    on_clear_retransmission_counter_();
  }

  StartT3RtxTimerIfOutstandingData();
  RTC_DCHECK(IsConsistent());
  return true;
}

void RetransmissionQueue::UpdateRTT(Timestamp now,
                                    UnwrappedTSN cumulative_tsn_ack) {

  TimeDelta rtt = outstanding_data_.MeasureRTT(now, cumulative_tsn_ack);

  if (rtt.IsFinite()) {
    on_new_rtt_(rtt);
  }
}

void RetransmissionQueue::HandleT3RtxTimerExpiry() {
  size_t old_cwnd = cwnd_;
  size_t old_unacked_bytes = unacked_bytes();

  ssthresh_ = std::max(cwnd_ / 2, 4 * options_.mtu);
  cwnd_ = 1 * options_.mtu;

  partial_bytes_acked_ = 0;

  outstanding_data_.NackAll();

  RTC_DLOG(LS_INFO) << log_prefix_ << "t3-rtx expired. new cwnd=" << cwnd_
                    << " (" << old_cwnd << "), ssthresh=" << ssthresh_
                    << ", unacked_bytes " << unacked_bytes() << " ("
                    << old_unacked_bytes << ")";
  RTC_DCHECK(IsConsistent());
}

std::vector<std::pair<TSN, Data>>
RetransmissionQueue::GetChunksForFastRetransmit(size_t bytes_in_packet) {
  RTC_DCHECK(outstanding_data_.has_data_to_be_fast_retransmitted());
  RTC_DCHECK(IsDivisibleBy4(bytes_in_packet));
  std::vector<std::pair<TSN, Data>> to_be_sent;
  size_t old_unacked_bytes = unacked_bytes();

  to_be_sent =
      outstanding_data_.GetChunksToBeFastRetransmitted(bytes_in_packet);
  RTC_DCHECK(!to_be_sent.empty());

  if (to_be_sent[0].first ==
      outstanding_data_.last_cumulative_tsn_ack().next_value().Wrap()) {
    RTC_DLOG(LS_VERBOSE)
        << log_prefix_
        << "First outstanding DATA to be retransmitted - restarting T3-RTX";
    t3_rtx_.Stop();
  }

  if (!t3_rtx_.is_running()) {
    t3_rtx_.Start();
  }

  size_t bytes_retransmitted = absl::c_accumulate(
      to_be_sent, 0, [&](size_t r, const std::pair<TSN, Data>& d) {
        return r + GetSerializedChunkSize(d.second);
      });
  ++rtx_packets_count_;
  rtx_bytes_count_ += bytes_retransmitted;

  RTC_DLOG(LS_VERBOSE) << log_prefix_ << "Fast-retransmitting TSN "
                       << StrJoin(to_be_sent, ",",
                                  [&](rtc::StringBuilder& sb,
                                      const std::pair<TSN, Data>& c) {
                                    sb << *c.first;
                                  })
                       << " - " << bytes_retransmitted
                       << " bytes. unacked_bytes=" << unacked_bytes() << " ("
                       << old_unacked_bytes << ")";

  RTC_DCHECK(IsConsistent());
  return to_be_sent;
}

std::vector<std::pair<TSN, Data>> RetransmissionQueue::GetChunksToSend(
    Timestamp now,
    size_t bytes_remaining_in_packet) {

  RTC_DCHECK(IsDivisibleBy4(bytes_remaining_in_packet));

  std::vector<std::pair<TSN, Data>> to_be_sent;
  size_t old_unacked_bytes = unacked_bytes();
  size_t old_rwnd = rwnd_;

  size_t max_bytes =
      RoundDownTo4(std::min(max_bytes_to_send(), bytes_remaining_in_packet));

  to_be_sent = outstanding_data_.GetChunksToBeRetransmitted(max_bytes);

  size_t bytes_retransmitted = absl::c_accumulate(
      to_be_sent, 0, [&](size_t r, const std::pair<TSN, Data>& d) {
        return r + GetSerializedChunkSize(d.second);
      });
  max_bytes -= bytes_retransmitted;

  if (!to_be_sent.empty()) {
    ++rtx_packets_count_;
    rtx_bytes_count_ += bytes_retransmitted;
  }

  while (max_bytes > data_chunk_header_size_) {
    RTC_DCHECK(IsDivisibleBy4(max_bytes));
    absl::optional<SendQueue::DataToSend> chunk_opt =
        send_queue_.Produce(now, max_bytes - data_chunk_header_size_);
    if (!chunk_opt.has_value()) {
      break;
    }

    size_t chunk_size = GetSerializedChunkSize(chunk_opt->data);
    max_bytes -= chunk_size;
    rwnd_ -= chunk_size;

    absl::optional<UnwrappedTSN> tsn = outstanding_data_.Insert(
        chunk_opt->message_id, chunk_opt->data, now,
        partial_reliability_ ? chunk_opt->max_retransmissions
                             : MaxRetransmits::NoLimit(),
        partial_reliability_ ? chunk_opt->expires_at
                             : Timestamp::PlusInfinity(),
        chunk_opt->lifecycle_id);

    if (tsn.has_value()) {
      if (chunk_opt->lifecycle_id.IsSet()) {
        RTC_DCHECK(chunk_opt->data.is_end);
        callbacks_.OnLifecycleMessageFullySent(chunk_opt->lifecycle_id);
      }
      to_be_sent.emplace_back(tsn->Wrap(), std::move(chunk_opt->data));
    }
  }

  if (!to_be_sent.empty()) {

    if (!t3_rtx_.is_running()) {
      t3_rtx_.Start();
    }
    RTC_DLOG(LS_VERBOSE) << log_prefix_ << "Sending TSN "
                         << StrJoin(to_be_sent, ",",
                                    [&](rtc::StringBuilder& sb,
                                        const std::pair<TSN, Data>& c) {
                                      sb << *c.first;
                                    })
                         << " - "
                         << absl::c_accumulate(
                                to_be_sent, 0,
                                [&](size_t r, const std::pair<TSN, Data>& d) {
                                  return r + GetSerializedChunkSize(d.second);
                                })
                         << " bytes. unacked_bytes=" << unacked_bytes() << " ("
                         << old_unacked_bytes << "), cwnd=" << cwnd_
                         << ", rwnd=" << rwnd_ << " (" << old_rwnd << ")";
  }
  RTC_DCHECK(IsConsistent());
  return to_be_sent;
}

bool RetransmissionQueue::can_send_data() const {
  return cwnd_ < options_.avoid_fragmentation_cwnd_mtus * options_.mtu ||
         max_bytes_to_send() >= min_bytes_required_to_send_;
}

bool RetransmissionQueue::ShouldSendForwardTsn(Timestamp now) {
  if (!partial_reliability_) {
    return false;
  }
  outstanding_data_.ExpireOutstandingChunks(now);
  bool ret = outstanding_data_.ShouldSendForwardTsn();
  RTC_DCHECK(IsConsistent());
  return ret;
}

size_t RetransmissionQueue::max_bytes_to_send() const {
  size_t left = unacked_bytes() >= cwnd_ ? 0 : cwnd_ - unacked_bytes();

  if (unacked_bytes() == 0) {

    return left;
  }

  return std::min(rwnd(), left);
}

void RetransmissionQueue::PrepareResetStream(StreamID stream_id) {

  send_queue_.PrepareResetStream(stream_id);
}
bool RetransmissionQueue::HasStreamsReadyToBeReset() const {
  return send_queue_.HasStreamsReadyToBeReset();
}
std::vector<StreamID> RetransmissionQueue::BeginResetStreams() {
  outstanding_data_.BeginResetStreams();
  return send_queue_.GetStreamsReadyToBeReset();
}
void RetransmissionQueue::CommitResetStreams() {
  send_queue_.CommitResetStreams();
}
void RetransmissionQueue::RollbackResetStreams() {
  send_queue_.RollbackResetStreams();
}

HandoverReadinessStatus RetransmissionQueue::GetHandoverReadiness() const {
  HandoverReadinessStatus status;
  if (!outstanding_data_.empty()) {
    status.Add(HandoverUnreadinessReason::kRetransmissionQueueOutstandingData);
  }
  if (fast_recovery_exit_tsn_.has_value()) {
    status.Add(HandoverUnreadinessReason::kRetransmissionQueueFastRecovery);
  }
  if (outstanding_data_.has_data_to_be_retransmitted()) {
    status.Add(HandoverUnreadinessReason::kRetransmissionQueueNotEmpty);
  }
  return status;
}

void RetransmissionQueue::AddHandoverState(DcSctpSocketHandoverState& state) {
  state.tx.next_tsn = next_tsn().value();
  state.tx.rwnd = rwnd_;
  state.tx.cwnd = cwnd_;
  state.tx.ssthresh = ssthresh_;
  state.tx.partial_bytes_acked = partial_bytes_acked_;
}

void RetransmissionQueue::RestoreFromState(
    const DcSctpSocketHandoverState& state) {

  RTC_DCHECK(outstanding_data_.empty());
  RTC_DCHECK(!t3_rtx_.is_running());
  RTC_DCHECK(partial_bytes_acked_ == 0);

  cwnd_ = state.tx.cwnd;
  rwnd_ = state.tx.rwnd;
  ssthresh_ = state.tx.ssthresh;
  partial_bytes_acked_ = state.tx.partial_bytes_acked;

  outstanding_data_.ResetSequenceNumbers(
      tsn_unwrapper_.Unwrap(TSN(state.tx.next_tsn - 1)));
}
}
