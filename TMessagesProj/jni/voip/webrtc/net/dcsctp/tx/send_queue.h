/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_TX_SEND_QUEUE_H_
#define NET_DCSCTP_TX_SEND_QUEUE_H_

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/units/timestamp.h"
#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/public/types.h"

namespace dcsctp {

class SendQueue {
 public:

  struct DataToSend {
    DataToSend(OutgoingMessageId message_id, Data data)
        : message_id(message_id), data(std::move(data)) {}

    OutgoingMessageId message_id;

    Data data;

    MaxRetransmits max_retransmissions = MaxRetransmits::NoLimit();
    webrtc::Timestamp expires_at = webrtc::Timestamp::PlusInfinity();

    LifecycleId lifecycle_id = LifecycleId::NotSet();
  };

  virtual ~SendQueue() = default;

  virtual absl::optional<DataToSend> Produce(webrtc::Timestamp now,
                                             size_t max_size) = 0;

  virtual bool Discard(StreamID stream_id, OutgoingMessageId message_id) = 0;

  virtual void PrepareResetStream(StreamID stream_id) = 0;

  virtual bool HasStreamsReadyToBeReset() const = 0;

  virtual std::vector<StreamID> GetStreamsReadyToBeReset() = 0;

  virtual void CommitResetStreams() = 0;

  virtual void RollbackResetStreams() = 0;

  virtual void Reset() = 0;

  virtual size_t buffered_amount(StreamID stream_id) const = 0;

  virtual size_t total_buffered_amount() const = 0;

  virtual size_t buffered_amount_low_threshold(StreamID stream_id) const = 0;

  virtual void SetBufferedAmountLowThreshold(StreamID stream_id,
                                             size_t bytes) = 0;

  virtual void EnableMessageInterleaving(bool enabled) = 0;
};
}

#endif
