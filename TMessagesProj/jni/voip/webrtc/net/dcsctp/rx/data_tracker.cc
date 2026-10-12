/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/rx/data_tracker.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "net/dcsctp/packet/chunk/sack_chunk.h"
#include "net/dcsctp/timer/timer.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"

namespace dcsctp {

constexpr size_t DataTracker::kMaxDuplicateTsnReported;
constexpr size_t DataTracker::kMaxGapAckBlocksReported;

bool DataTracker::AdditionalTsnBlocks::Add(UnwrappedTSN tsn) {

  auto it = absl::c_lower_bound(
      blocks_, tsn, [&](const TsnRange& elem, const UnwrappedTSN& t) {
        return elem.last.next_value() < t;
      });

  if (it == blocks_.end()) {

    blocks_.emplace_back(tsn, tsn);
    return true;
  }

  if (tsn >= it->first && tsn <= it->last) {

    return false;
  }

  if (it->last.next_value() == tsn) {

    auto next_it = it + 1;
    if (next_it != blocks_.end() && tsn.next_value() == next_it->first) {

      it->last = next_it->last;
      blocks_.erase(next_it);
      return true;
    }

    it->last = tsn;
    return true;
  }

  if (it->first == tsn.next_value()) {

    RTC_DCHECK(it == blocks_.begin() || (it - 1)->last.next_value() != tsn);

    it->first = tsn;
    return true;
  }

  blocks_.emplace(it, tsn, tsn);
  return true;
}

void DataTracker::AdditionalTsnBlocks::EraseTo(UnwrappedTSN tsn) {

  auto it = absl::c_lower_bound(
      blocks_, tsn, [&](const TsnRange& elem, const UnwrappedTSN& t) {
        return elem.last < t;
      });

  bool tsn_is_within_block = it != blocks_.end() && tsn >= it->first;
  blocks_.erase(blocks_.begin(), it);

  if (tsn_is_within_block) {
    blocks_.front().first = tsn.next_value();
  }
}

void DataTracker::AdditionalTsnBlocks::PopFront() {
  RTC_DCHECK(!blocks_.empty());
  blocks_.erase(blocks_.begin());
}

bool DataTracker::IsTSNValid(TSN tsn) const {
  UnwrappedTSN unwrapped_tsn = tsn_unwrapper_.PeekUnwrap(tsn);

  uint32_t difference =
      UnwrappedTSN::Difference(unwrapped_tsn, last_cumulative_acked_tsn_);
  if (difference > kMaxAcceptedOutstandingFragments) {
    return false;
  }
  return true;
}

bool DataTracker::Observe(TSN tsn,
                          AnyDataChunk::ImmediateAckFlag immediate_ack) {
  bool is_duplicate = false;
  UnwrappedTSN unwrapped_tsn = tsn_unwrapper_.Unwrap(tsn);

  RTC_DCHECK(
      UnwrappedTSN::Difference(unwrapped_tsn, last_cumulative_acked_tsn_) <=
      kMaxAcceptedOutstandingFragments);

  if (unwrapped_tsn <= last_cumulative_acked_tsn_) {
    if (duplicate_tsns_.size() < kMaxDuplicateTsnReported) {
      duplicate_tsns_.insert(unwrapped_tsn.Wrap());
    }

    UpdateAckState(AckState::kImmediate, "duplicate data");
    is_duplicate = true;
  } else {
    if (unwrapped_tsn == last_cumulative_acked_tsn_.next_value()) {
      last_cumulative_acked_tsn_ = unwrapped_tsn;

      if (!additional_tsn_blocks_.empty() &&
          additional_tsn_blocks_.front().first ==
              last_cumulative_acked_tsn_.next_value()) {
        last_cumulative_acked_tsn_ = additional_tsn_blocks_.front().last;
        additional_tsn_blocks_.PopFront();
      }
    } else {
      bool inserted = additional_tsn_blocks_.Add(unwrapped_tsn);
      if (!inserted) {

        if (duplicate_tsns_.size() < kMaxDuplicateTsnReported) {
          duplicate_tsns_.insert(unwrapped_tsn.Wrap());
        }

        is_duplicate = true;
      }
    }
  }

  if (!additional_tsn_blocks_.empty()) {
    UpdateAckState(AckState::kImmediate, "packet loss");
  }

  if (*immediate_ack) {
    UpdateAckState(AckState::kImmediate, "immediate-ack bit set");
  }

  if (!seen_packet_) {

    seen_packet_ = true;
    UpdateAckState(AckState::kImmediate, "first DATA chunk");
  }

  if (ack_state_ == AckState::kIdle) {
    UpdateAckState(AckState::kBecomingDelayed, "received DATA when idle");
  } else if (ack_state_ == AckState::kDelayed) {
    UpdateAckState(AckState::kImmediate, "received DATA when already delayed");
  }
  return !is_duplicate;
}

bool DataTracker::HandleForwardTsn(TSN new_cumulative_ack) {

  UnwrappedTSN unwrapped_tsn = tsn_unwrapper_.Unwrap(new_cumulative_ack);
  UnwrappedTSN prev_last_cum_ack_tsn = last_cumulative_acked_tsn_;

  if (unwrapped_tsn <= last_cumulative_acked_tsn_) {

    UpdateAckState(AckState::kImmediate,
                   "FORWARD_TSN new_cumulative_tsn was behind");
    return false;
  }

  last_cumulative_acked_tsn_ = unwrapped_tsn;
  additional_tsn_blocks_.EraseTo(unwrapped_tsn);

  if (!additional_tsn_blocks_.empty() &&
      additional_tsn_blocks_.front().first ==
          last_cumulative_acked_tsn_.next_value()) {
    last_cumulative_acked_tsn_ = additional_tsn_blocks_.front().last;
    additional_tsn_blocks_.PopFront();
  }

  RTC_DLOG(LS_VERBOSE) << log_prefix_ << "FORWARD_TSN, cum_ack_tsn="
                       << *prev_last_cum_ack_tsn.Wrap() << "->"
                       << *new_cumulative_ack << "->"
                       << *last_cumulative_acked_tsn_.Wrap();

  if (ack_state_ == AckState::kIdle) {
    UpdateAckState(AckState::kBecomingDelayed,
                   "received FORWARD_TSN when idle");
  } else if (ack_state_ == AckState::kDelayed) {
    UpdateAckState(AckState::kImmediate,
                   "received FORWARD_TSN when already delayed");
  }
  return true;
}

SackChunk DataTracker::CreateSelectiveAck(size_t a_rwnd) {

  std::set<TSN> duplicate_tsns;
  duplicate_tsns_.swap(duplicate_tsns);

  return SackChunk(last_cumulative_acked_tsn_.Wrap(), a_rwnd,
                   CreateGapAckBlocks(), std::move(duplicate_tsns));
}

std::vector<SackChunk::GapAckBlock> DataTracker::CreateGapAckBlocks() const {
  const auto& blocks = additional_tsn_blocks_.blocks();
  std::vector<SackChunk::GapAckBlock> gap_ack_blocks;
  gap_ack_blocks.reserve(std::min(blocks.size(), kMaxGapAckBlocksReported));
  for (size_t i = 0; i < blocks.size() && i < kMaxGapAckBlocksReported; ++i) {
    auto start_diff =
        UnwrappedTSN::Difference(blocks[i].first, last_cumulative_acked_tsn_);
    auto end_diff =
        UnwrappedTSN::Difference(blocks[i].last, last_cumulative_acked_tsn_);
    gap_ack_blocks.emplace_back(static_cast<uint16_t>(start_diff),
                                static_cast<uint16_t>(end_diff));
  }

  return gap_ack_blocks;
}

bool DataTracker::ShouldSendAck(bool also_if_delayed) {
  if (ack_state_ == AckState::kImmediate ||
      (also_if_delayed && (ack_state_ == AckState::kBecomingDelayed ||
                           ack_state_ == AckState::kDelayed))) {
    UpdateAckState(AckState::kIdle, "sending SACK");
    return true;
  }

  return false;
}

bool DataTracker::will_increase_cum_ack_tsn(TSN tsn) const {
  UnwrappedTSN unwrapped = tsn_unwrapper_.PeekUnwrap(tsn);
  return unwrapped == last_cumulative_acked_tsn_.next_value();
}

void DataTracker::ForceImmediateSack() {
  ack_state_ = AckState::kImmediate;
}

void DataTracker::HandleDelayedAckTimerExpiry() {
  UpdateAckState(AckState::kImmediate, "delayed ack timer expired");
}

void DataTracker::ObservePacketEnd() {
  if (ack_state_ == AckState::kBecomingDelayed) {
    UpdateAckState(AckState::kDelayed, "packet end");
  }
}

void DataTracker::UpdateAckState(AckState new_state, absl::string_view reason) {
  if (new_state != ack_state_) {
    RTC_DLOG(LS_VERBOSE) << log_prefix_ << "State changed from "
                         << ToString(ack_state_) << " to "
                         << ToString(new_state) << " due to " << reason;
    if (ack_state_ == AckState::kDelayed) {
      delayed_ack_timer_.Stop();
    } else if (new_state == AckState::kDelayed) {
      delayed_ack_timer_.Start();
    }
    ack_state_ = new_state;
  }
}

absl::string_view DataTracker::ToString(AckState ack_state) {
  switch (ack_state) {
    case AckState::kIdle:
      return "IDLE";
    case AckState::kBecomingDelayed:
      return "BECOMING_DELAYED";
    case AckState::kDelayed:
      return "DELAYED";
    case AckState::kImmediate:
      return "IMMEDIATE";
  }
}

HandoverReadinessStatus DataTracker::GetHandoverReadiness() const {
  HandoverReadinessStatus status;
  if (!additional_tsn_blocks_.empty()) {
    status.Add(HandoverUnreadinessReason::kDataTrackerTsnBlocksPending);
  }
  return status;
}

void DataTracker::AddHandoverState(DcSctpSocketHandoverState& state) {
  state.rx.last_cumulative_acked_tsn = last_cumulative_acked_tsn().value();
  state.rx.seen_packet = seen_packet_;
}

void DataTracker::RestoreFromState(const DcSctpSocketHandoverState& state) {

  RTC_DCHECK(additional_tsn_blocks_.empty());
  RTC_DCHECK(duplicate_tsns_.empty());
  RTC_DCHECK(!seen_packet_);

  seen_packet_ = state.rx.seen_packet;
  last_cumulative_acked_tsn_ =
      tsn_unwrapper_.Unwrap(TSN(state.rx.last_cumulative_acked_tsn));
}
}
