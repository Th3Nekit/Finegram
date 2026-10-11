/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SCTP_DATA_CHANNEL_H_
#define PC_SCTP_DATA_CHANNEL_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "absl/types/optional.h"
#include "api/data_channel_interface.h"
#include "api/priority.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/transport/data_channel_transport_interface.h"
#include "pc/data_channel_utils.h"
#include "pc/sctp_utils.h"
#include "rtc_base/containers/flat_set.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {

class SctpDataChannel;

class SctpDataChannelControllerInterface {
 public:

  virtual RTCError SendData(StreamId sid,
                            const SendDataParams& params,
                            const rtc::CopyOnWriteBuffer& payload) = 0;

  virtual void AddSctpDataStream(StreamId sid) = 0;

  virtual void RemoveSctpDataStream(StreamId sid) = 0;

  virtual void OnChannelStateChanged(SctpDataChannel* data_channel,
                                     DataChannelInterface::DataState state) = 0;

 protected:
  virtual ~SctpDataChannelControllerInterface() {}
};

struct InternalDataChannelInit : public DataChannelInit {
  enum OpenHandshakeRole { kOpener, kAcker, kNone };

  InternalDataChannelInit() : open_handshake_role(kOpener) {}
  explicit InternalDataChannelInit(const DataChannelInit& base);

  bool IsValid() const;

  OpenHandshakeRole open_handshake_role;

  absl::optional<rtc::SSLRole> fallback_ssl_role;
};

class SctpSidAllocator {
 public:
  SctpSidAllocator() = default;

  StreamId AllocateSid(rtc::SSLRole role);

  bool ReserveSid(StreamId sid);

  void ReleaseSid(StreamId sid);

 private:
  flat_set<StreamId> used_sids_ RTC_GUARDED_BY(&sequence_checker_);
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_{
      SequenceChecker::kDetached};
};

class SctpDataChannel : public DataChannelInterface {
 public:
  static rtc::scoped_refptr<SctpDataChannel> Create(
      rtc::WeakPtr<SctpDataChannelControllerInterface> controller,
      const std::string& label,
      bool connected_to_transport,
      const InternalDataChannelInit& config,
      rtc::Thread* signaling_thread,
      rtc::Thread* network_thread);

  static rtc::scoped_refptr<DataChannelInterface> CreateProxy(
      rtc::scoped_refptr<SctpDataChannel> channel,
      rtc::scoped_refptr<PendingTaskSafetyFlag> signaling_safety);

  void RegisterObserver(DataChannelObserver* observer) override;
  void UnregisterObserver() override;

  std::string label() const override;
  bool reliable() const override;
  bool ordered() const override;

  uint16_t maxRetransmitTime() const override;
  uint16_t maxRetransmits() const override;

  absl::optional<int> maxPacketLifeTime() const override;
  absl::optional<int> maxRetransmitsOpt() const override;
  std::string protocol() const override;
  bool negotiated() const override;
  int id() const override;
  Priority priority() const override;

  uint64_t buffered_amount() const override;
  void Close() override;
  DataState state() const override;
  RTCError error() const override;
  uint32_t messages_sent() const override;
  uint64_t bytes_sent() const override;
  uint32_t messages_received() const override;
  uint64_t bytes_received() const override;
  bool Send(const DataBuffer& buffer) override;
  void SendAsync(DataBuffer buffer,
                 absl::AnyInvocable<void(RTCError) &&> on_complete) override;

  void CloseAbruptlyWithError(RTCError error);

  void CloseAbruptlyWithDataChannelFailure(const std::string& message);

  void OnTransportReady();

  void OnDataReceived(DataMessageType type,
                      const rtc::CopyOnWriteBuffer& payload);

  void SetSctpSid_n(StreamId sid);

  void OnClosingProcedureStartedRemotely();

  void OnClosingProcedureComplete();

  void OnTransportChannelCreated();

  void OnTransportChannelClosed(RTCError error);

  DataChannelStats GetStats() const;

  int internal_id() const { return internal_id_; }

  StreamId sid_n() const {
    RTC_DCHECK_RUN_ON(network_thread_);
    return id_n_;
  }

  static void ResetInternalIdAllocatorForTesting(int new_value);

 protected:
  SctpDataChannel(const InternalDataChannelInit& config,
                  rtc::WeakPtr<SctpDataChannelControllerInterface> controller,
                  const std::string& label,
                  bool connected_to_transport,
                  rtc::Thread* signaling_thread,
                  rtc::Thread* network_thread);
  ~SctpDataChannel() override;

 private:
  class ObserverAdapter;

  enum HandshakeState {
    kHandshakeInit,
    kHandshakeShouldSendOpen,
    kHandshakeShouldSendAck,
    kHandshakeWaitingForAck,
    kHandshakeReady
  };

  RTCError SendImpl(DataBuffer buffer) RTC_RUN_ON(network_thread_);
  void UpdateState() RTC_RUN_ON(network_thread_);
  void SetState(DataState state) RTC_RUN_ON(network_thread_);

  void DeliverQueuedReceivedData() RTC_RUN_ON(network_thread_);

  void SendQueuedDataMessages() RTC_RUN_ON(network_thread_);
  RTCError SendDataMessage(const DataBuffer& buffer, bool queue_if_blocked)
      RTC_RUN_ON(network_thread_);
  bool QueueSendDataMessage(const DataBuffer& buffer)
      RTC_RUN_ON(network_thread_);

  void SendQueuedControlMessages() RTC_RUN_ON(network_thread_);
  bool SendControlMessage(const rtc::CopyOnWriteBuffer& buffer)
      RTC_RUN_ON(network_thread_);

  bool connected_to_transport() const RTC_RUN_ON(network_thread_) {
    return network_safety_->alive();
  }

  rtc::Thread* const signaling_thread_;
  rtc::Thread* const network_thread_;
  StreamId id_n_ RTC_GUARDED_BY(network_thread_);
  const int internal_id_;
  const std::string label_;
  const std::string protocol_;
  const absl::optional<int> max_retransmit_time_;
  const absl::optional<int> max_retransmits_;
  const absl::optional<Priority> priority_;
  const bool negotiated_;
  const bool ordered_;

  DataChannelObserver* observer_ RTC_GUARDED_BY(network_thread_) = nullptr;
  std::unique_ptr<ObserverAdapter> observer_adapter_;
  DataState state_ RTC_GUARDED_BY(network_thread_) = kConnecting;
  RTCError error_ RTC_GUARDED_BY(network_thread_);
  uint32_t messages_sent_ RTC_GUARDED_BY(network_thread_) = 0;
  uint64_t bytes_sent_ RTC_GUARDED_BY(network_thread_) = 0;
  uint32_t messages_received_ RTC_GUARDED_BY(network_thread_) = 0;
  uint64_t bytes_received_ RTC_GUARDED_BY(network_thread_) = 0;
  rtc::WeakPtr<SctpDataChannelControllerInterface> controller_
      RTC_GUARDED_BY(network_thread_);
  HandshakeState handshake_state_ RTC_GUARDED_BY(network_thread_) =
      kHandshakeInit;

  bool started_closing_procedure_ RTC_GUARDED_BY(network_thread_) = false;

  PacketQueue queued_control_data_ RTC_GUARDED_BY(network_thread_);
  PacketQueue queued_received_data_ RTC_GUARDED_BY(network_thread_);
  PacketQueue queued_send_data_ RTC_GUARDED_BY(network_thread_);
  rtc::scoped_refptr<PendingTaskSafetyFlag> network_safety_ =
      PendingTaskSafetyFlag::CreateDetachedInactive();
};

}

#endif
