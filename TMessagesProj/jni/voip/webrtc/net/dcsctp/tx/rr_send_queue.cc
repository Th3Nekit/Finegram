/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/rr_send_queue.h"

#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/tx/send_queue.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/str_join.h"

namespace dcsctp {
using ::webrtc::TimeDelta;
using ::webrtc::Timestamp;

RRSendQueue::RRSendQueue(absl::string_view log_prefix,
                         DcSctpSocketCallbacks* callbacks,
                         size_t buffer_size,
                         size_t mtu,
                         StreamPriority default_priority,
                         size_t total_buffered_amount_low_threshold)
    : log_prefix_(log_prefix),
      callbacks_(*callbacks),
      buffer_size_(buffer_size),
      default_priority_(default_priority),
      scheduler_(log_prefix_, mtu),
      total_buffered_amount_(
          [this]() { callbacks_.OnTotalBufferedAmountLow(); }) {
  total_buffered_amount_.SetLowThreshold(total_buffered_amount_low_threshold);
}

size_t RRSendQueue::OutgoingStream::bytes_to_send_in_next_message() const {
  if (pause_state_ == PauseState::kPaused ||
      pause_state_ == PauseState::kResetting) {

    return 0;
  }

  if (items_.empty()) {
    return 0;
  }

  return items_.front().remaining_size;
}

void RRSendQueue::OutgoingStream::AddHandoverState(
    DcSctpSocketHandoverState::OutgoingStream& state) const {
  state.next_ssn = next_ssn_.value();
  state.next_ordered_mid = next_ordered_mid_.value();
  state.next_unordered_mid = next_unordered_mid_.value();
  state.priority = *scheduler_stream_->priority();
}

bool RRSendQueue::IsConsistent() const {
  std::set<StreamID> expected_active_streams;
  std::set<StreamID> actual_active_streams =
      scheduler_.ActiveStreamsForTesting();

  size_t total_buffered_amount = 0;
  for (const auto& [stream_id, stream] : streams_) {
    total_buffered_amount += stream.buffered_amount().value();
    if (stream.bytes_to_send_in_next_message() > 0) {
      expected_active_streams.emplace(stream_id);
    }
  }
  if (expected_active_streams != actual_active_streams) {
    auto fn = [&](rtc::StringBuilder& sb, const auto& p) { sb << *p; };
    RTC_DLOG(LS_ERROR) << "Active streams mismatch, is=["
                       << StrJoin(actual_active_streams, ",", fn)
                       << "], expected=["
                       << StrJoin(expected_active_streams, ",", fn) << "]";
    return false;
  }

  return total_buffered_amount == total_buffered_amount_.value();
}

bool RRSendQueue::OutgoingStream::IsConsistent() const {
  size_t bytes = 0;
  for (const auto& item : items_) {
    bytes += item.remaining_size;
  }
  return bytes == buffered_amount_.value();
}

void RRSendQueue::ThresholdWatcher::Decrease(size_t bytes) {
  RTC_DCHECK(bytes <= value_);
  size_t old_value = value_;
  value_ -= bytes;

  if (old_value > low_threshold_ && value_ <= low_threshold_) {
    on_threshold_reached_();
  }
}

void RRSendQueue::ThresholdWatcher::SetLowThreshold(size_t low_threshold) {

  if (low_threshold_ < value_ && low_threshold >= value_) {
    on_threshold_reached_();
  }
  low_threshold_ = low_threshold;
}

void RRSendQueue::OutgoingStream::Add(DcSctpMessage message,
                                      MessageAttributes attributes) {
  bool was_active = bytes_to_send_in_next_message() > 0;
  buffered_amount_.Increase(message.payload().size());
  parent_.total_buffered_amount_.Increase(message.payload().size());
  OutgoingMessageId message_id = parent_.current_message_id;
  parent_.current_message_id =
      OutgoingMessageId(*parent_.current_message_id + 1);
  items_.emplace_back(message_id, std::move(message), std::move(attributes));

  if (!was_active) {
    scheduler_stream_->MaybeMakeActive();
  }

  RTC_DCHECK(IsConsistent());
}

absl::optional<SendQueue::DataToSend> RRSendQueue::OutgoingStream::Produce(
    Timestamp now,
    size_t max_size) {
  RTC_DCHECK(pause_state_ != PauseState::kPaused &&
             pause_state_ != PauseState::kResetting);

  while (!items_.empty()) {
    Item& item = items_.front();
    DcSctpMessage& message = item.message;

    if (!item.mid.has_value()) {

      if (item.attributes.expires_at <= now) {
        HandleMessageExpired(item);
        items_.pop_front();
        continue;
      }

      MID& mid =
          item.attributes.unordered ? next_unordered_mid_ : next_ordered_mid_;
      item.mid = mid;
      mid = MID(*mid + 1);
    }
    if (!item.attributes.unordered && !item.ssn.has_value()) {
      item.ssn = next_ssn_;
      next_ssn_ = SSN(*next_ssn_ + 1);
    }

    rtc::ArrayView<const uint8_t> chunk_payload =
        item.message.payload().subview(item.remaining_offset, max_size);
    rtc::ArrayView<const uint8_t> message_payload = message.payload();
    Data::IsBeginning is_beginning(chunk_payload.data() ==
                                   message_payload.data());
    Data::IsEnd is_end((chunk_payload.data() + chunk_payload.size()) ==
                       (message_payload.data() + message_payload.size()));

    StreamID stream_id = message.stream_id();
    PPID ppid = message.ppid();

    std::vector<uint8_t> payload =
        is_beginning && is_end
            ? std::move(message).ReleasePayload()
            : std::vector<uint8_t>(chunk_payload.begin(), chunk_payload.end());

    FSN fsn(item.current_fsn);
    item.current_fsn = FSN(*item.current_fsn + 1);
    buffered_amount_.Decrease(payload.size());
    parent_.total_buffered_amount_.Decrease(payload.size());

    SendQueue::DataToSend chunk(
        item.message_id, Data(stream_id, item.ssn.value_or(SSN(0)), *item.mid,
                              fsn, ppid, std::move(payload), is_beginning,
                              is_end, item.attributes.unordered));
    chunk.max_retransmissions = item.attributes.max_retransmissions;
    chunk.expires_at = item.attributes.expires_at;
    chunk.lifecycle_id =
        is_end ? item.attributes.lifecycle_id : LifecycleId::NotSet();

    if (is_end) {

      items_.pop_front();

      if (pause_state_ == PauseState::kPending) {
        RTC_DLOG(LS_VERBOSE) << "Pause state on " << *stream_id
                             << " is moving from pending to paused";
        pause_state_ = PauseState::kPaused;
      }
    } else {
      item.remaining_offset += chunk_payload.size();
      item.remaining_size -= chunk_payload.size();
      RTC_DCHECK(item.remaining_offset + item.remaining_size ==
                 item.message.payload().size());
      RTC_DCHECK(item.remaining_size > 0);
    }
    RTC_DCHECK(IsConsistent());
    return chunk;
  }
  RTC_DCHECK(IsConsistent());
  return absl::nullopt;
}

void RRSendQueue::OutgoingStream::HandleMessageExpired(
    OutgoingStream::Item& item) {
  buffered_amount_.Decrease(item.remaining_size);
  parent_.total_buffered_amount_.Decrease(item.remaining_size);
  if (item.attributes.lifecycle_id.IsSet()) {
    RTC_DLOG(LS_VERBOSE) << "Triggering OnLifecycleMessageExpired("
                         << item.attributes.lifecycle_id.value() << ", false)";

    parent_.callbacks_.OnLifecycleMessageExpired(item.attributes.lifecycle_id,
                                                                     false);
    parent_.callbacks_.OnLifecycleEnd(item.attributes.lifecycle_id);
  }
}

bool RRSendQueue::OutgoingStream::Discard(OutgoingMessageId message_id) {
  bool result = false;
  if (!items_.empty()) {
    Item& item = items_.front();
    if (item.message_id == message_id) {
      HandleMessageExpired(item);
      items_.pop_front();

      scheduler_stream_->ForceReschedule();

      if (pause_state_ == PauseState::kPending) {
        pause_state_ = PauseState::kPaused;
        scheduler_stream_->MakeInactive();
      } else if (bytes_to_send_in_next_message() == 0) {
        scheduler_stream_->MakeInactive();
      }

      result = true;
    }
  }
  RTC_DCHECK(IsConsistent());
  return result;
}

void RRSendQueue::OutgoingStream::Pause() {
  if (pause_state_ != PauseState::kNotPaused) {

    return;
  }

  bool had_pending_items = !items_.empty();

  for (auto it = items_.begin(); it != items_.end();) {
    if (it->remaining_offset == 0) {
      HandleMessageExpired(*it);
      it = items_.erase(it);
    } else {
      ++it;
    }
  }

  pause_state_ = (items_.empty() || items_.front().remaining_offset == 0)
                     ? PauseState::kPaused
                     : PauseState::kPending;

  if (had_pending_items && pause_state_ == PauseState::kPaused) {
    RTC_DLOG(LS_VERBOSE) << "Stream " << *stream_id()
                         << " was previously active, but is now paused.";
    scheduler_stream_->MakeInactive();
  }

  RTC_DCHECK(IsConsistent());
}

void RRSendQueue::OutgoingStream::Resume() {
  RTC_DCHECK(pause_state_ == PauseState::kResetting);
  pause_state_ = PauseState::kNotPaused;
  scheduler_stream_->MaybeMakeActive();
  RTC_DCHECK(IsConsistent());
}

void RRSendQueue::OutgoingStream::Reset() {

  PauseState old_pause_state = pause_state_;
  pause_state_ = PauseState::kNotPaused;
  next_ordered_mid_ = MID(0);
  next_unordered_mid_ = MID(0);
  next_ssn_ = SSN(0);
  if (!items_.empty()) {

    auto& item = items_.front();
    buffered_amount_.Increase(item.message.payload().size() -
                              item.remaining_size);
    parent_.total_buffered_amount_.Increase(item.message.payload().size() -
                                            item.remaining_size);
    item.remaining_offset = 0;
    item.remaining_size = item.message.payload().size();
    item.mid = absl::nullopt;
    item.ssn = absl::nullopt;
    item.current_fsn = FSN(0);
    if (old_pause_state == PauseState::kPaused ||
        old_pause_state == PauseState::kResetting) {
      scheduler_stream_->MaybeMakeActive();
    }
  }
  RTC_DCHECK(IsConsistent());
}

bool RRSendQueue::OutgoingStream::has_partially_sent_message() const {
  if (items_.empty()) {
    return false;
  }
  return items_.front().mid.has_value();
}

void RRSendQueue::Add(Timestamp now,
                      DcSctpMessage message,
                      const SendOptions& send_options) {
  RTC_DCHECK(!message.payload().empty());

  MessageAttributes attributes = {
      .unordered = send_options.unordered,
      .max_retransmissions =
          send_options.max_retransmissions.has_value()
              ? MaxRetransmits(send_options.max_retransmissions.value())
              : MaxRetransmits::NoLimit(),
      .expires_at = send_options.lifetime.has_value()
                        ? now + send_options.lifetime->ToTimeDelta() +
                              TimeDelta::Millis(1)
                        : Timestamp::PlusInfinity(),
      .lifecycle_id = send_options.lifecycle_id,
  };
  StreamID stream_id = message.stream_id();
  GetOrCreateStreamInfo(stream_id).Add(std::move(message),
                                       std::move(attributes));
  RTC_DCHECK(IsConsistent());
}

bool RRSendQueue::IsFull() const {
  return total_buffered_amount() >= buffer_size_;
}

bool RRSendQueue::IsEmpty() const {
  return total_buffered_amount() == 0;
}

absl::optional<SendQueue::DataToSend> RRSendQueue::Produce(Timestamp now,
                                                           size_t max_size) {
  return scheduler_.Produce(now, max_size);
}

bool RRSendQueue::Discard(StreamID stream_id, OutgoingMessageId message_id) {
  bool has_discarded = GetOrCreateStreamInfo(stream_id).Discard(message_id);

  RTC_DCHECK(IsConsistent());
  return has_discarded;
}

void RRSendQueue::PrepareResetStream(StreamID stream_id) {
  GetOrCreateStreamInfo(stream_id).Pause();
  RTC_DCHECK(IsConsistent());
}

bool RRSendQueue::HasStreamsReadyToBeReset() const {
  for (auto& [unused, stream] : streams_) {
    if (stream.IsReadyToBeReset()) {
      return true;
    }
  }
  return false;
}
std::vector<StreamID> RRSendQueue::GetStreamsReadyToBeReset() {
  RTC_DCHECK(absl::c_count_if(streams_, [](const auto& p) {
               return p.second.IsResetting();
             }) == 0);
  std::vector<StreamID> ready;
  for (auto& [stream_id, stream] : streams_) {
    if (stream.IsReadyToBeReset()) {
      stream.SetAsResetting();
      ready.push_back(stream_id);
    }
  }
  return ready;
}

void RRSendQueue::CommitResetStreams() {
  RTC_DCHECK(absl::c_count_if(streams_, [](const auto& p) {
               return p.second.IsResetting();
             }) > 0);
  for (auto& [unused, stream] : streams_) {
    if (stream.IsResetting()) {
      stream.Reset();
    }
  }
  RTC_DCHECK(IsConsistent());
}

void RRSendQueue::RollbackResetStreams() {
  RTC_DCHECK(absl::c_count_if(streams_, [](const auto& p) {
               return p.second.IsResetting();
             }) > 0);
  for (auto& [unused, stream] : streams_) {
    if (stream.IsResetting()) {
      stream.Resume();
    }
  }
  RTC_DCHECK(IsConsistent());
}

void RRSendQueue::Reset() {

  for (auto& [unused, stream] : streams_) {
    stream.Reset();
  }
  scheduler_.ForceReschedule();
}

size_t RRSendQueue::buffered_amount(StreamID stream_id) const {
  auto it = streams_.find(stream_id);
  if (it == streams_.end()) {
    return 0;
  }
  return it->second.buffered_amount().value();
}

size_t RRSendQueue::buffered_amount_low_threshold(StreamID stream_id) const {
  auto it = streams_.find(stream_id);
  if (it == streams_.end()) {
    return 0;
  }
  return it->second.buffered_amount().low_threshold();
}

void RRSendQueue::SetBufferedAmountLowThreshold(StreamID stream_id,
                                                size_t bytes) {
  GetOrCreateStreamInfo(stream_id).buffered_amount().SetLowThreshold(bytes);
}

RRSendQueue::OutgoingStream& RRSendQueue::GetOrCreateStreamInfo(
    StreamID stream_id) {
  auto it = streams_.find(stream_id);
  if (it != streams_.end()) {
    return it->second;
  }

  return streams_
      .emplace(
          std::piecewise_construct, std::forward_as_tuple(stream_id),
          std::forward_as_tuple(this, &scheduler_, stream_id, default_priority_,
                                [this, stream_id]() {
                                  callbacks_.OnBufferedAmountLow(stream_id);
                                }))
      .first->second;
}

void RRSendQueue::SetStreamPriority(StreamID stream_id,
                                    StreamPriority priority) {
  OutgoingStream& stream = GetOrCreateStreamInfo(stream_id);

  stream.SetPriority(priority);
  RTC_DCHECK(IsConsistent());
}

StreamPriority RRSendQueue::GetStreamPriority(StreamID stream_id) const {
  auto stream_it = streams_.find(stream_id);
  if (stream_it == streams_.end()) {
    return default_priority_;
  }
  return stream_it->second.priority();
}

HandoverReadinessStatus RRSendQueue::GetHandoverReadiness() const {
  HandoverReadinessStatus status;
  if (!IsEmpty()) {
    status.Add(HandoverUnreadinessReason::kSendQueueNotEmpty);
  }
  return status;
}

void RRSendQueue::AddHandoverState(DcSctpSocketHandoverState& state) {
  for (const auto& [stream_id, stream] : streams_) {
    DcSctpSocketHandoverState::OutgoingStream state_stream;
    state_stream.id = stream_id.value();
    stream.AddHandoverState(state_stream);
    state.tx.streams.push_back(std::move(state_stream));
  }
}

void RRSendQueue::RestoreFromState(const DcSctpSocketHandoverState& state) {
  for (const DcSctpSocketHandoverState::OutgoingStream& state_stream :
       state.tx.streams) {
    StreamID stream_id(state_stream.id);
    streams_.emplace(
        std::piecewise_construct, std::forward_as_tuple(stream_id),
        std::forward_as_tuple(
            this, &scheduler_, stream_id, StreamPriority(state_stream.priority),
            [this, stream_id]() { callbacks_.OnBufferedAmountLow(stream_id); },
            &state_stream));
  }
}
}
