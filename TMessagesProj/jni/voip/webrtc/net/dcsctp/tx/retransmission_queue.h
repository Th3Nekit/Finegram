/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_TX_RETRANSMISSION_QUEUE_H_
#define NET_DCSCTP_TX_RETRANSMISSION_QUEUE_H_

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "net/dcsctp/packet/chunk/forward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/iforward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/sack_chunk.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/public/dcsctp_handover_state.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/timer/timer.h"
#include "net/dcsctp/tx/outstanding_data.h"
#include "net/dcsctp/tx/retransmission_timeout.h"
#include "net/dcsctp/tx/send_queue.h"

namespace dcsctp {

class RetransmissionQueue {
 public:
  static constexpr size_t kMinimumFragmentedPayload = 10;
  using State = OutstandingData::State;

  RetransmissionQueue(absl::string_view log_prefix,
                      DcSctpSocketCallbacks* callbacks,
                      TSN my_initial_tsn,
                      size_t a_rwnd,
                      SendQueue& send_queue,
                      std::function<void(webrtc::TimeDelta rtt)> on_new_rtt,
                      std::function<void()> on_clear_retransmission_counter,
                      Timer& t3_rtx,
                      const DcSctpOptions& options,
                      bool supports_partial_reliability = true,
                      bool use_message_interleaving = false);

  bool HandleSack(webrtc::Timestamp now, const SackChunk& sack);

  void HandleT3RtxTimerExpiry();

  bool has_data_to_be_fast_retransmitted() const {
    return outstanding_data_.has_data_to_be_fast_retransmitted();
  }

  std::vector<std::pair<TSN, Data>> GetChunksForFastRetransmit(
      size_t bytes_in_packet);

  std::vector<std::pair<TSN, Data>> GetChunksToSend(
      webrtc::Timestamp now,
      size_t bytes_remaining_in_packet);

  std::vector<std::pair<TSN, OutstandingData::State>> GetChunkStatesForTesting()
      const {
    return outstanding_data_.GetChunkStatesForTesting();
  }

  TSN next_tsn() const { return outstanding_data_.next_tsn().Wrap(); }

  TSN last_assigned_tsn() const {
    return UnwrappedTSN::AddTo(outstanding_data_.next_tsn(), -1).Wrap();
  }

  size_t cwnd() const { return cwnd_; }

  void set_cwnd(size_t cwnd) { cwnd_ = cwnd; }

  size_t rwnd() const { return rwnd_; }

  size_t rtx_packets_count() const { return rtx_packets_count_; }
  uint64_t rtx_bytes_count() const { return rtx_bytes_count_; }

  size_t unacked_bytes() const { return outstanding_data_.unacked_bytes(); }

  size_t unacked_items() const { return outstanding_data_.unacked_items(); }

  bool can_send_data() const;

  bool ShouldSendForwardTsn(webrtc::Timestamp now);

  ForwardTsnChunk CreateForwardTsn() const {
    return outstanding_data_.CreateForwardTsn();
  }

  IForwardTsnChunk CreateIForwardTsn() const {
    return outstanding_data_.CreateIForwardTsn();
  }

  void PrepareResetStream(StreamID stream_id);
  bool HasStreamsReadyToBeReset() const;
  std::vector<StreamID> BeginResetStreams();
  void CommitResetStreams();
  void RollbackResetStreams();

  HandoverReadinessStatus GetHandoverReadiness() const;

  void AddHandoverState(DcSctpSocketHandoverState& state);
  void RestoreFromState(const DcSctpSocketHandoverState& state);

 private:
  enum class CongestionAlgorithmPhase {
    kSlowStart,
    kCongestionAvoidance,
  };

  bool IsConsistent() const;

  size_t GetSerializedChunkSize(const Data& data) const;

  bool is_in_fast_recovery() const {
    return fast_recovery_exit_tsn_.has_value();
  }

  bool IsSackValid(const SackChunk& sack) const;

  void UpdateRTT(webrtc::Timestamp now, UnwrappedTSN cumulative_tsn_ack);

  void MaybeExitFastRecovery(UnwrappedTSN cumulative_tsn_ack);

  void StopT3RtxTimerOnIncreasedCumulativeTsnAck(
      UnwrappedTSN cumulative_tsn_ack);

  void HandleIncreasedCumulativeTsnAck(size_t unacked_bytes,
                                       size_t total_bytes_acked);

  void HandlePacketLoss(UnwrappedTSN highest_tsn_acked);

  void UpdateReceiverWindow(uint32_t a_rwnd);

  void StartT3RtxTimerIfOutstandingData();

  CongestionAlgorithmPhase phase() const {
    return (cwnd_ <= ssthresh_)
               ? CongestionAlgorithmPhase::kSlowStart
               : CongestionAlgorithmPhase::kCongestionAvoidance;
  }

  size_t max_bytes_to_send() const;

  DcSctpSocketCallbacks& callbacks_;
  const DcSctpOptions options_;

  const size_t min_bytes_required_to_send_;

  const bool partial_reliability_;
  const absl::string_view log_prefix_;

  const size_t data_chunk_header_size_;

  const std::function<void(webrtc::TimeDelta rtt)> on_new_rtt_;

  const std::function<void()> on_clear_retransmission_counter_;

  Timer& t3_rtx_;

  UnwrappedTSN::Unwrapper tsn_unwrapper_;

  size_t cwnd_;

  size_t rwnd_;

  size_t ssthresh_;

  size_t partial_bytes_acked_;

  size_t rtx_packets_count_ = 0;
  uint64_t rtx_bytes_count_ = 0;

  absl::optional<UnwrappedTSN> fast_recovery_exit_tsn_ = absl::nullopt;

  SendQueue& send_queue_;

  OutstandingData outstanding_data_;
};
}

#endif
