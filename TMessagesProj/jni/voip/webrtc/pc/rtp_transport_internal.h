/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_RTP_TRANSPORT_INTERNAL_H_
#define PC_RTP_TRANSPORT_INTERNAL_H_

#include <string>
#include <utility>

#include "call/rtp_demuxer.h"
#include "p2p/base/ice_transport_internal.h"
#include "pc/session_description.h"
#include "rtc_base/callback_list.h"
#include "rtc_base/network_route.h"
#include "rtc_base/ssl_stream_adapter.h"

namespace rtc {
class CopyOnWriteBuffer;
struct PacketOptions;
}

namespace webrtc {

class RtpTransportInternal : public sigslot::has_slots<> {
 public:
  virtual ~RtpTransportInternal() = default;

  virtual void SetRtcpMuxEnabled(bool enable) = 0;

  virtual const std::string& transport_name() const = 0;

  virtual int SetRtpOption(rtc::Socket::Option opt, int value) = 0;
  virtual int SetRtcpOption(rtc::Socket::Option opt, int value) = 0;

  virtual bool rtcp_mux_enabled() const = 0;

  virtual bool IsReadyToSend() const = 0;

  void SubscribeReadyToSend(const void* tag,
                            absl::AnyInvocable<void(bool)> callback) {
    callback_list_ready_to_send_.AddReceiver(tag, std::move(callback));
  }
  void UnsubscribeReadyToSend(const void* tag) {
    callback_list_ready_to_send_.RemoveReceivers(tag);
  }

  void SubscribeRtcpPacketReceived(
      const void* tag,
      absl::AnyInvocable<void(rtc::CopyOnWriteBuffer*, int64_t)> callback) {
    callback_list_rtcp_packet_received_.AddReceiver(tag, std::move(callback));
  }

  void SetUnDemuxableRtpPacketReceivedHandler(
      absl::AnyInvocable<void(RtpPacketReceived&)> callback) {
    callback_undemuxable_rtp_packet_received_ = std::move(callback);
  }

  void SubscribeNetworkRouteChanged(
      const void* tag,
      absl::AnyInvocable<void(absl::optional<rtc::NetworkRoute>)> callback) {
    callback_list_network_route_changed_.AddReceiver(tag, std::move(callback));
  }
  void UnsubscribeNetworkRouteChanged(const void* tag) {
    callback_list_network_route_changed_.RemoveReceivers(tag);
  }

  void SubscribeWritableState(const void* tag,
                              absl::AnyInvocable<void(bool)> callback) {
    callback_list_writable_state_.AddReceiver(tag, std::move(callback));
  }
  void UnsubscribeWritableState(const void* tag) {
    callback_list_writable_state_.RemoveReceivers(tag);
  }
  void SubscribeSentPacket(
      const void* tag,
      absl::AnyInvocable<void(const rtc::SentPacket&)> callback) {
    callback_list_sent_packet_.AddReceiver(tag, std::move(callback));
  }
  void UnsubscribeSentPacket(const void* tag) {
    callback_list_sent_packet_.RemoveReceivers(tag);
  }

  virtual bool IsWritable(bool rtcp) const = 0;

  virtual bool SendRtpPacket(rtc::CopyOnWriteBuffer* packet,
                             const rtc::PacketOptions& options,
                             int flags) = 0;

  virtual bool SendRtcpPacket(rtc::CopyOnWriteBuffer* packet,
                              const rtc::PacketOptions& options,
                              int flags) = 0;

  virtual void UpdateRtpHeaderExtensionMap(
      const cricket::RtpHeaderExtensions& header_extensions) = 0;

  virtual bool IsSrtpActive() const = 0;

  virtual bool RegisterRtpDemuxerSink(const RtpDemuxerCriteria& criteria,
                                      RtpPacketSinkInterface* sink) = 0;

  virtual bool UnregisterRtpDemuxerSink(RtpPacketSinkInterface* sink) = 0;

 protected:
  void SendReadyToSend(bool arg) { callback_list_ready_to_send_.Send(arg); }
  void SendRtcpPacketReceived(rtc::CopyOnWriteBuffer* buffer,
                              int64_t packet_time_us) {
    callback_list_rtcp_packet_received_.Send(buffer, packet_time_us);
  }
  void NotifyUnDemuxableRtpPacketReceived(RtpPacketReceived& packet) {
    callback_undemuxable_rtp_packet_received_(packet);
  }
  void SendNetworkRouteChanged(absl::optional<rtc::NetworkRoute> route) {
    callback_list_network_route_changed_.Send(route);
  }
  void SendWritableState(bool state) {
    callback_list_writable_state_.Send(state);
  }
  void SendSentPacket(const rtc::SentPacket& packet) {
    callback_list_sent_packet_.Send(packet);
  }

 private:
  CallbackList<bool> callback_list_ready_to_send_;
  CallbackList<rtc::CopyOnWriteBuffer*, int64_t>
      callback_list_rtcp_packet_received_;
  absl::AnyInvocable<void(RtpPacketReceived&)>
      callback_undemuxable_rtp_packet_received_ =
          [](RtpPacketReceived& packet) {};
  CallbackList<absl::optional<rtc::NetworkRoute>>
      callback_list_network_route_changed_;
  CallbackList<bool> callback_list_writable_state_;
  CallbackList<const rtc::SentPacket&> callback_list_sent_packet_;
};

}

#endif
