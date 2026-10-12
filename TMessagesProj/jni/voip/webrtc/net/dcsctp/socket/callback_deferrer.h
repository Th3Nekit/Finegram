/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_SOCKET_CALLBACK_DEFERRER_H_
#define NET_DCSCTP_SOCKET_CALLBACK_DEFERRER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "api/array_view.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_base.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/dcsctp_socket.h"

namespace dcsctp {

class CallbackDeferrer : public DcSctpSocketCallbacks {
 public:
  class ScopedDeferrer {
   public:
    explicit ScopedDeferrer(CallbackDeferrer& callback_deferrer)
        : callback_deferrer_(callback_deferrer) {
      callback_deferrer_.Prepare();
    }

    ~ScopedDeferrer() { callback_deferrer_.TriggerDeferred(); }

   private:
    CallbackDeferrer& callback_deferrer_;
  };

  explicit CallbackDeferrer(DcSctpSocketCallbacks& underlying)
      : underlying_(underlying) {}

  SendPacketStatus SendPacketWithStatus(
      rtc::ArrayView<const uint8_t> data) override;
  std::unique_ptr<Timeout> CreateTimeout(
      webrtc::TaskQueueBase::DelayPrecision precision) override;
  TimeMs TimeMillis() override;
  webrtc::Timestamp Now() override { return underlying_.Now(); }
  uint32_t GetRandomInt(uint32_t low, uint32_t high) override;
  void OnMessageReceived(DcSctpMessage message) override;
  void OnError(ErrorKind error, absl::string_view message) override;
  void OnAborted(ErrorKind error, absl::string_view message) override;
  void OnConnected() override;
  void OnClosed() override;
  void OnConnectionRestarted() override;
  void OnStreamsResetFailed(rtc::ArrayView<const StreamID> outgoing_streams,
                            absl::string_view reason) override;
  void OnStreamsResetPerformed(
      rtc::ArrayView<const StreamID> outgoing_streams) override;
  void OnIncomingStreamsReset(
      rtc::ArrayView<const StreamID> incoming_streams) override;
  void OnBufferedAmountLow(StreamID stream_id) override;
  void OnTotalBufferedAmountLow() override;

  void OnLifecycleMessageExpired(LifecycleId lifecycle_id,
                                 bool maybe_delivered) override;
  void OnLifecycleMessageFullySent(LifecycleId lifecycle_id) override;
  void OnLifecycleMessageDelivered(LifecycleId lifecycle_id) override;
  void OnLifecycleEnd(LifecycleId lifecycle_id) override;

 private:
  struct Error {
    ErrorKind error;
    std::string message;
  };
  struct StreamReset {
    std::vector<StreamID> streams;
    std::string message;
  };

  using CallbackData = absl::
      variant<absl::monostate, DcSctpMessage, Error, StreamReset, StreamID>;
  using Callback = void (*)(CallbackData, DcSctpSocketCallbacks&);

  void Prepare();
  void TriggerDeferred();

  DcSctpSocketCallbacks& underlying_;
  bool prepared_ = false;
  std::vector<std::pair<Callback, CallbackData>> deferred_;
};
}

#endif
