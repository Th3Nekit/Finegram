/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_SOCKET_DCSCTP_SOCKET_H_
#define NET_DCSCTP_SOCKET_DCSCTP_SOCKET_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "net/dcsctp/packet/chunk/abort_chunk.h"
#include "net/dcsctp/packet/chunk/chunk.h"
#include "net/dcsctp/packet/chunk/cookie_ack_chunk.h"
#include "net/dcsctp/packet/chunk/cookie_echo_chunk.h"
#include "net/dcsctp/packet/chunk/data_chunk.h"
#include "net/dcsctp/packet/chunk/data_common.h"
#include "net/dcsctp/packet/chunk/error_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_common.h"
#include "net/dcsctp/packet/chunk/heartbeat_ack_chunk.h"
#include "net/dcsctp/packet/chunk/heartbeat_request_chunk.h"
#include "net/dcsctp/packet/chunk/idata_chunk.h"
#include "net/dcsctp/packet/chunk/iforward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/init_ack_chunk.h"
#include "net/dcsctp/packet/chunk/init_chunk.h"
#include "net/dcsctp/packet/chunk/reconfig_chunk.h"
#include "net/dcsctp/packet/chunk/sack_chunk.h"
#include "net/dcsctp/packet/chunk/shutdown_ack_chunk.h"
#include "net/dcsctp/packet/chunk/shutdown_chunk.h"
#include "net/dcsctp/packet/chunk/shutdown_complete_chunk.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/packet/sctp_packet.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/public/packet_observer.h"
#include "net/dcsctp/rx/data_tracker.h"
#include "net/dcsctp/rx/reassembly_queue.h"
#include "net/dcsctp/socket/callback_deferrer.h"
#include "net/dcsctp/socket/packet_sender.h"
#include "net/dcsctp/socket/state_cookie.h"
#include "net/dcsctp/socket/transmission_control_block.h"
#include "net/dcsctp/timer/timer.h"
#include "net/dcsctp/tx/retransmission_error_counter.h"
#include "net/dcsctp/tx/retransmission_queue.h"
#include "net/dcsctp/tx/retransmission_timeout.h"
#include "net/dcsctp/tx/rr_send_queue.h"

namespace dcsctp {

class DcSctpSocket : public DcSctpSocketInterface {
 public:

  DcSctpSocket(absl::string_view log_prefix,
               DcSctpSocketCallbacks& callbacks,
               std::unique_ptr<PacketObserver> packet_observer,
               const DcSctpOptions& options);

  DcSctpSocket(const DcSctpSocket&) = delete;
  DcSctpSocket& operator=(const DcSctpSocket&) = delete;

  void ReceivePacket(rtc::ArrayView<const uint8_t> data) override;
  void HandleTimeout(TimeoutID timeout_id) override;
  void Connect() override;
  void RestoreFromState(const DcSctpSocketHandoverState& state) override;
  void Shutdown() override;
  void Close() override;
  SendStatus Send(DcSctpMessage message,
                  const SendOptions& send_options) override;
  std::vector<SendStatus> SendMany(rtc::ArrayView<DcSctpMessage> messages,
                                   const SendOptions& send_options) override;
  ResetStreamsStatus ResetStreams(
      rtc::ArrayView<const StreamID> outgoing_streams) override;
  SocketState state() const override;
  const DcSctpOptions& options() const override { return options_; }
  void SetMaxMessageSize(size_t max_message_size) override;
  void SetStreamPriority(StreamID stream_id, StreamPriority priority) override;
  StreamPriority GetStreamPriority(StreamID stream_id) const override;
  size_t buffered_amount(StreamID stream_id) const override;
  size_t buffered_amount_low_threshold(StreamID stream_id) const override;
  void SetBufferedAmountLowThreshold(StreamID stream_id, size_t bytes) override;
  absl::optional<Metrics> GetMetrics() const override;
  HandoverReadinessStatus GetHandoverReadiness() const override;
  absl::optional<DcSctpSocketHandoverState> GetHandoverStateAndClose() override;
  SctpImplementation peer_implementation() const override {
    return metrics_.peer_implementation;
  }

  VerificationTag verification_tag() const {
    return tcb_ != nullptr ? tcb_->my_verification_tag() : VerificationTag(0);
  }

 private:

  struct ConnectParameters {
    TSN initial_tsn = TSN(0);
    VerificationTag verification_tag = VerificationTag(0);
  };

  enum class State {
    kClosed,
    kCookieWait,

    kCookieEchoed,
    kEstablished,
    kShutdownPending,
    kShutdownSent,
    kShutdownReceived,
    kShutdownAckSent,
  };

  std::string log_prefix() const;

  bool IsConsistent() const;
  static constexpr absl::string_view ToString(DcSctpSocket::State state);

  void CreateTransmissionControlBlock(const Capabilities& capabilities,
                                      VerificationTag my_verification_tag,
                                      TSN my_initial_tsn,
                                      VerificationTag peer_verification_tag,
                                      TSN peer_initial_tsn,
                                      size_t a_rwnd,
                                      TieTag tie_tag);

  void SetState(State state, absl::string_view reason);

  void InternalClose(ErrorKind error, absl::string_view message);

  void CloseConnectionBecauseOfTooManyTransmissionErrors();

  webrtc::TimeDelta OnInitTimerExpiry();
  webrtc::TimeDelta OnCookieTimerExpiry();
  webrtc::TimeDelta OnShutdownTimerExpiry();
  void OnSentPacket(rtc::ArrayView<const uint8_t> packet,
                    SendPacketStatus status);

  void MaybeSendShutdownOrAck();

  void MaybeSendShutdownOnPacketReceived(const SctpPacket& packet);

  void MaybeSendResetStreamsRequest();

  SendStatus InternalSend(const DcSctpMessage& message,
                          const SendOptions& send_options);

  void SendInit();

  void SendShutdown();

  void SendShutdownAck();

  bool ValidatePacket(const SctpPacket& packet);

  void DebugPrintOutgoing(rtc::ArrayView<const uint8_t> payload);

  void MaybeDeliverMessages();

  bool ValidateHasTCB();

  template <class T>
  bool ValidateParseSuccess(const absl::optional<T>& c) {
    if (c.has_value()) {
      return true;
    }

    ReportFailedToParseChunk(T::kType);
    return false;
  }

  void ReportFailedToParseChunk(int chunk_type);

  bool HandleUnrecognizedChunk(const SctpPacket::ChunkDescriptor& descriptor);

  bool Dispatch(const CommonHeader& header,
                const SctpPacket::ChunkDescriptor& descriptor);

  void HandleData(const CommonHeader& header,
                  const SctpPacket::ChunkDescriptor& descriptor);

  void HandleIData(const CommonHeader& header,
                   const SctpPacket::ChunkDescriptor& descriptor);

  void HandleDataCommon(AnyDataChunk& chunk);

  void HandleInit(const CommonHeader& header,
                  const SctpPacket::ChunkDescriptor& descriptor);

  void HandleInitAck(const CommonHeader& header,
                     const SctpPacket::ChunkDescriptor& descriptor);

  void HandleSack(const CommonHeader& header,
                  const SctpPacket::ChunkDescriptor& descriptor);

  void HandleHeartbeatRequest(const CommonHeader& header,
                              const SctpPacket::ChunkDescriptor& descriptor);

  void HandleHeartbeatAck(const CommonHeader& header,
                          const SctpPacket::ChunkDescriptor& descriptor);

  void HandleAbort(const CommonHeader& header,
                   const SctpPacket::ChunkDescriptor& descriptor);

  void HandleError(const CommonHeader& header,
                   const SctpPacket::ChunkDescriptor& descriptor);

  void HandleCookieEcho(const CommonHeader& header,
                        const SctpPacket::ChunkDescriptor& descriptor);

  bool HandleCookieEchoWithTCB(const CommonHeader& header,
                               const StateCookie& cookie);

  void HandleCookieAck(const CommonHeader& header,
                       const SctpPacket::ChunkDescriptor& descriptor);

  void HandleShutdown(const CommonHeader& header,
                      const SctpPacket::ChunkDescriptor& descriptor);

  void HandleShutdownAck(const CommonHeader& header,
                         const SctpPacket::ChunkDescriptor& descriptor);

  void HandleForwardTsn(const CommonHeader& header,
                        const SctpPacket::ChunkDescriptor& descriptor);

  void HandleIForwardTsn(const CommonHeader& header,
                         const SctpPacket::ChunkDescriptor& descriptor);

  void HandleReconfig(const CommonHeader& header,
                      const SctpPacket::ChunkDescriptor& descriptor);

  void HandleForwardTsnCommon(const AnyForwardTsnChunk& chunk);

  void HandleShutdownComplete(const CommonHeader& header,
                              const SctpPacket::ChunkDescriptor& descriptor);

  const std::string log_prefix_;
  const std::unique_ptr<PacketObserver> packet_observer_;
  Metrics metrics_;
  DcSctpOptions options_;

  CallbackDeferrer callbacks_;

  TimerManager timer_manager_;
  const std::unique_ptr<Timer> t1_init_;
  const std::unique_ptr<Timer> t1_cookie_;
  const std::unique_ptr<Timer> t2_shutdown_;

  PacketSender packet_sender_;

  RRSendQueue send_queue_;

  ConnectParameters connect_params_;

  State state_ = State::kClosed;

  std::unique_ptr<TransmissionControlBlock> tcb_;
};
}

#endif
