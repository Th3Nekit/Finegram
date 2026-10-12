/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_TX_RR_SEND_QUEUE_H_
#define NET_DCSCTP_TX_RR_SEND_QUEUE_H_

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/tx/send_queue.h"
#include "net/dcsctp/tx/stream_scheduler.h"

namespace dcsctp {

class RRSendQueue : public SendQueue {
 public:
  RRSendQueue(absl::string_view log_prefix,
              DcSctpSocketCallbacks* callbacks,
              size_t buffer_size,
              size_t mtu,
              StreamPriority default_priority,
              size_t total_buffered_amount_low_threshold);

  bool IsFull() const;

  bool IsEmpty() const;

  void Add(webrtc::Timestamp now,
           DcSctpMessage message,
           const SendOptions& send_options = {});

  absl::optional<DataToSend> Produce(webrtc::Timestamp now,
                                     size_t max_size) override;
  bool Discard(StreamID stream_id, OutgoingMessageId message_id) override;
  void PrepareResetStream(StreamID streams) override;
  bool HasStreamsReadyToBeReset() const override;
  std::vector<StreamID> GetStreamsReadyToBeReset() override;
  void CommitResetStreams() override;
  void RollbackResetStreams() override;
  void Reset() override;
  size_t buffered_amount(StreamID stream_id) const override;
  size_t total_buffered_amount() const override {
    return total_buffered_amount_.value();
  }
  size_t buffered_amount_low_threshold(StreamID stream_id) const override;
  void SetBufferedAmountLowThreshold(StreamID stream_id, size_t bytes) override;
  void EnableMessageInterleaving(bool enabled) override {
    scheduler_.EnableMessageInterleaving(enabled);
  }

  void SetStreamPriority(StreamID stream_id, StreamPriority priority);
  StreamPriority GetStreamPriority(StreamID stream_id) const;
  HandoverReadinessStatus GetHandoverReadiness() const;
  void AddHandoverState(DcSctpSocketHandoverState& state);
  void RestoreFromState(const DcSctpSocketHandoverState& state);

 private:
  struct MessageAttributes {
    IsUnordered unordered;
    MaxRetransmits max_retransmissions;
    webrtc::Timestamp expires_at;
    LifecycleId lifecycle_id;
  };

  class ThresholdWatcher {
   public:
    explicit ThresholdWatcher(std::function<void()> on_threshold_reached)
        : on_threshold_reached_(std::move(on_threshold_reached)) {}

    void Increase(size_t bytes) { value_ += bytes; }

    void Decrease(size_t bytes);

    size_t value() const { return value_; }
    size_t low_threshold() const { return low_threshold_; }
    void SetLowThreshold(size_t low_threshold);

   private:
    const std::function<void()> on_threshold_reached_;
    size_t value_ = 0;
    size_t low_threshold_ = 0;
  };

  class OutgoingStream : public StreamScheduler::StreamProducer {
   public:
    OutgoingStream(
        RRSendQueue* parent,
        StreamScheduler* scheduler,
        StreamID stream_id,
        StreamPriority priority,
        std::function<void()> on_buffered_amount_low,
        const DcSctpSocketHandoverState::OutgoingStream* state = nullptr)
        : parent_(*parent),
          scheduler_stream_(scheduler->CreateStream(this, stream_id, priority)),
          next_unordered_mid_(MID(state ? state->next_unordered_mid : 0)),
          next_ordered_mid_(MID(state ? state->next_ordered_mid : 0)),
          next_ssn_(SSN(state ? state->next_ssn : 0)),
          buffered_amount_(std::move(on_buffered_amount_low)) {}

    StreamID stream_id() const { return scheduler_stream_->stream_id(); }

    void Add(DcSctpMessage message, MessageAttributes attributes);

    absl::optional<SendQueue::DataToSend> Produce(webrtc::Timestamp now,
                                                  size_t max_size) override;
    size_t bytes_to_send_in_next_message() const override;

    const ThresholdWatcher& buffered_amount() const { return buffered_amount_; }
    ThresholdWatcher& buffered_amount() { return buffered_amount_; }

    bool Discard(OutgoingMessageId message_id);

    void Pause();

    void Resume();

    bool IsReadyToBeReset() const {
      return pause_state_ == PauseState::kPaused;
    }

    bool IsResetting() const { return pause_state_ == PauseState::kResetting; }

    void SetAsResetting() {
      RTC_DCHECK(pause_state_ == PauseState::kPaused);
      pause_state_ = PauseState::kResetting;
    }

    void Reset();

    bool has_partially_sent_message() const;

    StreamPriority priority() const { return scheduler_stream_->priority(); }
    void SetPriority(StreamPriority priority) {
      scheduler_stream_->SetPriority(priority);
    }

    void AddHandoverState(
        DcSctpSocketHandoverState::OutgoingStream& state) const;

   private:

    enum class PauseState {

      kNotPaused,

      kPending,

      kPaused,

      kResetting,
    };

    struct Item {
      explicit Item(OutgoingMessageId message_id,
                    DcSctpMessage msg,
                    MessageAttributes attributes)
          : message_id(message_id),
            message(std::move(msg)),
            attributes(std::move(attributes)),
            remaining_offset(0),
            remaining_size(message.payload().size()) {}
      OutgoingMessageId message_id;
      DcSctpMessage message;
      MessageAttributes attributes;

      size_t remaining_offset;
      size_t remaining_size;

      absl::optional<MID> mid = absl::nullopt;
      absl::optional<SSN> ssn = absl::nullopt;

      FSN current_fsn = FSN(0);
    };

    bool IsConsistent() const;
    void HandleMessageExpired(OutgoingStream::Item& item);

    RRSendQueue& parent_;

    const std::unique_ptr<StreamScheduler::Stream> scheduler_stream_;

    PauseState pause_state_ = PauseState::kNotPaused;

    MID next_unordered_mid_;
    MID next_ordered_mid_;

    SSN next_ssn_;

    std::deque<Item> items_;

    ThresholdWatcher buffered_amount_;
  };

  bool IsConsistent() const;
  OutgoingStream& GetOrCreateStreamInfo(StreamID stream_id);
  absl::optional<DataToSend> Produce(
      std::map<StreamID, OutgoingStream>::iterator it,
      webrtc::Timestamp now,
      size_t max_size);

  const absl::string_view log_prefix_;
  DcSctpSocketCallbacks& callbacks_;
  const size_t buffer_size_;
  const StreamPriority default_priority_;
  OutgoingMessageId current_message_id = OutgoingMessageId(0);
  StreamScheduler scheduler_;

  ThresholdWatcher total_buffered_amount_;

  std::map<StreamID, OutgoingStream> streams_;
};
}

#endif
