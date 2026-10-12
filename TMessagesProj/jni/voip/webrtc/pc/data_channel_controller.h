/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_DATA_CHANNEL_CONTROLLER_H_
#define PC_DATA_CHANNEL_CONTROLLER_H_

#include <string>
#include <vector>

#include "api/data_channel_interface.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/transport/data_channel_transport_interface.h"
#include "pc/data_channel_utils.h"
#include "pc/sctp_data_channel.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {

class PeerConnectionInternal;

class DataChannelController : public SctpDataChannelControllerInterface,
                              public DataChannelSink {
 public:
  explicit DataChannelController(PeerConnectionInternal* pc) : pc_(pc) {}
  ~DataChannelController();

  DataChannelController(DataChannelController&) = delete;
  DataChannelController& operator=(const DataChannelController& other) = delete;
  DataChannelController(DataChannelController&&) = delete;
  DataChannelController& operator=(DataChannelController&& other) = delete;

  RTCError SendData(StreamId sid,
                    const SendDataParams& params,
                    const rtc::CopyOnWriteBuffer& payload) override;
  void AddSctpDataStream(StreamId sid) override;
  void RemoveSctpDataStream(StreamId sid) override;
  void OnChannelStateChanged(SctpDataChannel* channel,
                             DataChannelInterface::DataState state) override;

  void OnDataReceived(int channel_id,
                      DataMessageType type,
                      const rtc::CopyOnWriteBuffer& buffer) override;
  void OnChannelClosing(int channel_id) override;
  void OnChannelClosed(int channel_id) override;
  void OnReadyToSend() override;
  void OnTransportClosed(RTCError error) override;

  void PrepareForShutdown();

  void SetupDataChannelTransport_n(DataChannelTransportInterface* transport);

  void TeardownDataChannelTransport_n(RTCError error);

  void OnTransportChanged(
      DataChannelTransportInterface* data_channel_transport);

  std::vector<DataChannelStats> GetDataChannelStats() const;

  RTCErrorOr<rtc::scoped_refptr<DataChannelInterface>>
  InternalCreateDataChannelWithProxy(const std::string& label,
                                     const InternalDataChannelInit& config);
  void AllocateSctpSids(rtc::SSLRole role);

  bool HasDataChannels() const;

  bool HasUsedDataChannels() const;

 protected:
  rtc::Thread* network_thread() const;
  rtc::Thread* signaling_thread() const;

 private:
  void OnSctpDataChannelClosed(SctpDataChannel* channel);

  RTCErrorOr<rtc::scoped_refptr<SctpDataChannel>> CreateDataChannel(
      const std::string& label,
      InternalDataChannelInit& config) RTC_RUN_ON(network_thread());

  bool HandleOpenMessage_n(int channel_id,
                           DataMessageType type,
                           const rtc::CopyOnWriteBuffer& buffer)
      RTC_RUN_ON(network_thread());

  void OnDataChannelOpenMessage(rtc::scoped_refptr<SctpDataChannel> channel,
                                bool ready_to_send)
      RTC_RUN_ON(signaling_thread());

  RTCError ReserveOrAllocateSid(StreamId& sid,
                                absl::optional<rtc::SSLRole> fallback_ssl_role)
      RTC_RUN_ON(network_thread());

  void NotifyDataChannelsOfTransportCreated();

  void set_data_channel_transport(DataChannelTransportInterface* transport);

  DataChannelTransportInterface* data_channel_transport_
      RTC_GUARDED_BY(network_thread()) = nullptr;
  SctpSidAllocator sid_allocator_ RTC_GUARDED_BY(network_thread());
  std::vector<rtc::scoped_refptr<SctpDataChannel>> sctp_data_channels_n_
      RTC_GUARDED_BY(network_thread());
  enum class DataChannelUsage : uint8_t {
    kNeverUsed = 0,
    kHaveBeenUsed,
    kInUse
  };
  DataChannelUsage channel_usage_ RTC_GUARDED_BY(signaling_thread()) =
      DataChannelUsage::kNeverUsed;

  PeerConnectionInternal* const pc_;

  rtc::WeakPtrFactory<DataChannelController> weak_factory_
      RTC_GUARDED_BY(network_thread()){this};
  ScopedTaskSafety signaling_safety_;
};

}

#endif
