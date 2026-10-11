/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_PUBLIC_DCSCTP_SOCKET_H_
#define NET_DCSCTP_PUBLIC_DCSCTP_SOCKET_H_

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/timestamp.h"
#include "net/dcsctp/public/dcsctp_handover_state.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/public/packet_observer.h"
#include "net/dcsctp/public/timeout.h"
#include "net/dcsctp/public/types.h"

namespace dcsctp {

enum class SocketState {

  kClosed,

  kConnecting,

  kConnected,

  kShuttingDown,
};

struct SendOptions {

  IsUnordered unordered = IsUnordered(false);

  absl::optional<DurationMs> lifetime = absl::nullopt;

  absl::optional<size_t> max_retransmissions = absl::nullopt;

  LifecycleId lifecycle_id = LifecycleId::NotSet();
};

enum class ErrorKind {

  kNoError,

  kTooManyRetries,

  kNotConnected,

  kParseFailed,

  kWrongSequence,

  kPeerReported,

  kProtocolViolation,

  kResourceExhaustion,

  kUnsupportedOperation,
};

inline constexpr absl::string_view ToString(ErrorKind error) {
  switch (error) {
    case ErrorKind::kNoError:
      return "NO_ERROR";
    case ErrorKind::kTooManyRetries:
      return "TOO_MANY_RETRIES";
    case ErrorKind::kNotConnected:
      return "NOT_CONNECTED";
    case ErrorKind::kParseFailed:
      return "PARSE_FAILED";
    case ErrorKind::kWrongSequence:
      return "WRONG_SEQUENCE";
    case ErrorKind::kPeerReported:
      return "PEER_REPORTED";
    case ErrorKind::kProtocolViolation:
      return "PROTOCOL_VIOLATION";
    case ErrorKind::kResourceExhaustion:
      return "RESOURCE_EXHAUSTION";
    case ErrorKind::kUnsupportedOperation:
      return "UNSUPPORTED_OPERATION";
  }
}

enum class SendStatus {

  kSuccess,

  kErrorMessageEmpty,

  kErrorMessageTooLarge,

  kErrorResourceExhaustion,

  kErrorShuttingDown,
};

inline constexpr absl::string_view ToString(SendStatus error) {
  switch (error) {
    case SendStatus::kSuccess:
      return "SUCCESS";
    case SendStatus::kErrorMessageEmpty:
      return "ERROR_MESSAGE_EMPTY";
    case SendStatus::kErrorMessageTooLarge:
      return "ERROR_MESSAGE_TOO_LARGE";
    case SendStatus::kErrorResourceExhaustion:
      return "ERROR_RESOURCE_EXHAUSTION";
    case SendStatus::kErrorShuttingDown:
      return "ERROR_SHUTTING_DOWN";
  }
}

enum class ResetStreamsStatus {

  kNotConnected,

  kPerformed,

  kNotSupported,
};

inline constexpr absl::string_view ToString(ResetStreamsStatus error) {
  switch (error) {
    case ResetStreamsStatus::kNotConnected:
      return "NOT_CONNECTED";
    case ResetStreamsStatus::kPerformed:
      return "PERFORMED";
    case ResetStreamsStatus::kNotSupported:
      return "NOT_SUPPORTED";
  }
}

enum class SendPacketStatus {

  kSuccess,

  kTemporaryFailure,

  kError,
};

enum class SctpImplementation {

  kUnknown,

  kDcsctp,

  kUsrSctp,

  kOther,
};

inline constexpr absl::string_view ToString(SctpImplementation implementation) {
  switch (implementation) {
    case SctpImplementation::kUnknown:
      return "unknown";
    case SctpImplementation::kDcsctp:
      return "dcsctp";
    case SctpImplementation::kUsrSctp:
      return "usrsctp";
    case SctpImplementation::kOther:
      return "other";
  }
}

struct Metrics {

  size_t tx_packets_count = 0;

  size_t tx_messages_count = 0;

  size_t rtx_packets_count = 0;

  uint64_t rtx_bytes_count = 0;

  size_t cwnd_bytes = 0;

  int srtt_ms = 0;

  size_t unack_data_count = 0;

  size_t rx_packets_count = 0;

  size_t rx_messages_count = 0;

  uint32_t peer_rwnd_bytes = 0;

  SctpImplementation peer_implementation = SctpImplementation::kUnknown;

  bool uses_message_interleaving = false;

  bool uses_zero_checksum = false;

  uint16_t negotiated_maximum_incoming_streams = 0;
  uint16_t negotiated_maximum_outgoing_streams = 0;
};

class DcSctpSocketCallbacks {
 public:
  virtual ~DcSctpSocketCallbacks() = default;

  virtual void SendPacket(rtc::ArrayView<const uint8_t> data) {}

  virtual SendPacketStatus SendPacketWithStatus(
      rtc::ArrayView<const uint8_t> data) {
    SendPacket(data);
    return SendPacketStatus::kSuccess;
  }

  virtual std::unique_ptr<Timeout> CreateTimeout(
      webrtc::TaskQueueBase::DelayPrecision precision) {

    return CreateTimeout();
  }

  virtual std::unique_ptr<Timeout> CreateTimeout() {
    return CreateTimeout(webrtc::TaskQueueBase::DelayPrecision::kLow);
  }

  virtual TimeMs TimeMillis() { return TimeMs(0); }

  virtual webrtc::Timestamp Now() {
    return webrtc::Timestamp::Millis(*TimeMillis());
  }

  virtual uint32_t GetRandomInt(uint32_t low, uint32_t high) = 0;

  ABSL_DEPRECATED("Use OnTotalBufferedAmountLow instead")
  virtual void NotifyOutgoingMessageBufferEmpty() {}

  virtual void OnMessageReceived(DcSctpMessage message) = 0;

  virtual void OnError(ErrorKind error, absl::string_view message) = 0;

  virtual void OnAborted(ErrorKind error, absl::string_view message) = 0;

  virtual void OnConnected() = 0;

  virtual void OnClosed() = 0;

  virtual void OnConnectionRestarted() = 0;

  virtual void OnStreamsResetFailed(
      rtc::ArrayView<const StreamID> outgoing_streams,
      absl::string_view reason) = 0;

  virtual void OnStreamsResetPerformed(
      rtc::ArrayView<const StreamID> outgoing_streams) = 0;

  virtual void OnIncomingStreamsReset(
      rtc::ArrayView<const StreamID> incoming_streams) = 0;

  virtual void OnBufferedAmountLow(StreamID stream_id) {}

  virtual void OnTotalBufferedAmountLow() {}

  virtual void OnLifecycleMessageFullySent(LifecycleId lifecycle_id) {}

  virtual void OnLifecycleMessageExpired(LifecycleId lifecycle_id,
                                         bool maybe_delivered) {}

  virtual void OnLifecycleMessageDelivered(LifecycleId lifecycle_id) {}

  virtual void OnLifecycleEnd(LifecycleId lifecycle_id) {}
};

class DcSctpSocketInterface {
 public:
  virtual ~DcSctpSocketInterface() = default;

  virtual void ReceivePacket(rtc::ArrayView<const uint8_t> data) = 0;

  virtual void HandleTimeout(TimeoutID timeout_id) = 0;

  virtual void Connect() = 0;

  virtual void RestoreFromState(const DcSctpSocketHandoverState& state) = 0;

  virtual void Shutdown() = 0;

  virtual void Close() = 0;

  virtual SocketState state() const = 0;

  virtual const DcSctpOptions& options() const = 0;

  virtual void SetMaxMessageSize(size_t max_message_size) = 0;

  virtual void SetStreamPriority(StreamID stream_id,
                                 StreamPriority priority) = 0;

  virtual StreamPriority GetStreamPriority(StreamID stream_id) const = 0;

  virtual SendStatus Send(DcSctpMessage message,
                          const SendOptions& send_options) = 0;

  virtual std::vector<SendStatus> SendMany(
      rtc::ArrayView<DcSctpMessage> messages,
      const SendOptions& send_options) = 0;

  virtual ResetStreamsStatus ResetStreams(
      rtc::ArrayView<const StreamID> outgoing_streams) = 0;

  virtual size_t buffered_amount(StreamID stream_id) const = 0;

  virtual size_t buffered_amount_low_threshold(StreamID stream_id) const = 0;

  virtual void SetBufferedAmountLowThreshold(StreamID stream_id,
                                             size_t bytes) = 0;

  virtual absl::optional<Metrics> GetMetrics() const = 0;

  virtual HandoverReadinessStatus GetHandoverReadiness() const = 0;

  virtual absl::optional<DcSctpSocketHandoverState>
  GetHandoverStateAndClose() = 0;

  ABSL_DEPRECATED("See Metrics::peer_implementation instead")
  virtual SctpImplementation peer_implementation() const {
    return SctpImplementation::kUnknown;
  }
};
}

#endif
