/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_RX_DATA_TRACKER_H_
#define NET_DCSCTP_RX_DATA_TRACKER_H_

#include <stddef.h>
#include <stdint.h>

#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "net/dcsctp/packet/chunk/data_common.h"
#include "net/dcsctp/packet/chunk/sack_chunk.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/public/dcsctp_handover_state.h"
#include "net/dcsctp/timer/timer.h"

namespace dcsctp {

class DataTracker {
 public:

  static constexpr size_t kMaxDuplicateTsnReported = 20;

  static constexpr size_t kMaxGapAckBlocksReported = 20;

  static constexpr uint32_t kMaxAcceptedOutstandingFragments = 100000;

  DataTracker(absl::string_view log_prefix,
              Timer* delayed_ack_timer,
              TSN peer_initial_tsn)
      : log_prefix_(log_prefix),
        seen_packet_(false),
        delayed_ack_timer_(*delayed_ack_timer),
        last_cumulative_acked_tsn_(
            tsn_unwrapper_.Unwrap(TSN(*peer_initial_tsn - 1))) {}

  bool IsTSNValid(TSN tsn) const;

  bool Observe(TSN tsn,
               AnyDataChunk::ImmediateAckFlag immediate_ack =
                   AnyDataChunk::ImmediateAckFlag(false));

  void ObservePacketEnd();

  bool HandleForwardTsn(TSN new_cumulative_ack);

  bool ShouldSendAck(bool also_if_delayed = false);

  TSN last_cumulative_acked_tsn() const {
    return TSN(last_cumulative_acked_tsn_.Wrap());
  }

  bool IsLaterThanCumulativeAckedTsn(TSN tsn) const {
    return tsn_unwrapper_.PeekUnwrap(tsn) > last_cumulative_acked_tsn_;
  }

  bool will_increase_cum_ack_tsn(TSN tsn) const;

  void ForceImmediateSack();

  SackChunk CreateSelectiveAck(size_t a_rwnd);

  void HandleDelayedAckTimerExpiry();

  HandoverReadinessStatus GetHandoverReadiness() const;

  void AddHandoverState(DcSctpSocketHandoverState& state);
  void RestoreFromState(const DcSctpSocketHandoverState& state);

 private:
  enum class AckState {

    kIdle,

    kBecomingDelayed,

    kDelayed,

    kImmediate,
  };

  class AdditionalTsnBlocks {
   public:

    struct TsnRange {
      TsnRange(UnwrappedTSN first, UnwrappedTSN last)
          : first(first), last(last) {}
      UnwrappedTSN first;
      UnwrappedTSN last;
    };

    bool Add(UnwrappedTSN tsn);

    void EraseTo(UnwrappedTSN tsn);

    void PopFront();

    const std::vector<TsnRange>& blocks() const { return blocks_; }

    bool empty() const { return blocks_.empty(); }

    const TsnRange& front() const { return blocks_.front(); }

   private:

    std::vector<TsnRange> blocks_;
  };

  std::vector<SackChunk::GapAckBlock> CreateGapAckBlocks() const;
  void UpdateAckState(AckState new_state, absl::string_view reason);
  static absl::string_view ToString(AckState ack_state);

  const absl::string_view log_prefix_;

  bool seen_packet_;
  Timer& delayed_ack_timer_;
  AckState ack_state_ = AckState::kIdle;
  UnwrappedTSN::Unwrapper tsn_unwrapper_;

  UnwrappedTSN last_cumulative_acked_tsn_;

  AdditionalTsnBlocks additional_tsn_blocks_;
  std::set<TSN> duplicate_tsns_;
};
}

#endif
