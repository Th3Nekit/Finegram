/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_RTP_TRANSCEIVER_H_
#define PC_RTP_TRANSCEIVER_H_

#include <stddef.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/audio_options.h"
#include "api/crypto/crypto_options.h"
#include "api/jsep.h"
#include "api/media_types.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_direction.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "media/base/media_channel.h"
#include "media/base/media_config.h"
#include "media/base/media_engine.h"
#include "pc/channel_interface.h"
#include "pc/connection_context.h"
#include "pc/proxy.h"
#include "pc/rtp_receiver.h"
#include "pc/rtp_receiver_proxy.h"
#include "pc/rtp_sender.h"
#include "pc/rtp_sender_proxy.h"
#include "pc/rtp_transport_internal.h"
#include "pc/session_description.h"
#include "rtc_base/thread_annotations.h"

namespace cricket {
class MediaEngineInterface;
}

namespace webrtc {

class PeerConnectionSdpMethods;

class RtpTransceiver : public RtpTransceiverInterface {
 public:

  RtpTransceiver(cricket::MediaType media_type, ConnectionContext* context);

  RtpTransceiver(
      rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender,
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
          receiver,
      ConnectionContext* context,
      std::vector<RtpHeaderExtensionCapability> HeaderExtensionsToNegotiate,
      std::function<void()> on_negotiation_needed);
  ~RtpTransceiver() override;

  RtpTransceiver(const RtpTransceiver&) = delete;
  RtpTransceiver& operator=(const RtpTransceiver&) = delete;
  RtpTransceiver(RtpTransceiver&&) = delete;
  RtpTransceiver& operator=(RtpTransceiver&&) = delete;

  cricket::ChannelInterface* channel() const { return channel_.get(); }

  RTCError CreateChannel(
      absl::string_view mid,
      Call* call_ptr,
      const cricket::MediaConfig& media_config,
      bool srtp_required,
      CryptoOptions crypto_options,
      const cricket::AudioOptions& audio_options,
      const cricket::VideoOptions& video_options,
      VideoBitrateAllocatorFactory* video_bitrate_allocator_factory,
      std::function<RtpTransportInternal*(absl::string_view)> transport_lookup);

  void SetChannel(std::unique_ptr<cricket::ChannelInterface> channel,
                  std::function<RtpTransportInternal*(const std::string&)>
                      transport_lookup);

  void ClearChannel();

  void AddSender(
      rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender);

  bool RemoveSender(RtpSenderInterface* sender);

  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
  senders() const {
    return senders_;
  }

  void AddReceiver(
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
          receiver);

  bool RemoveReceiver(RtpReceiverInterface* receiver);

  std::vector<
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>>
  receivers() const {
    return receivers_;
  }

  rtc::scoped_refptr<RtpSenderInternal> sender_internal() const;

  rtc::scoped_refptr<RtpReceiverInternal> receiver_internal() const;

  absl::optional<size_t> mline_index() const { return mline_index_; }
  void set_mline_index(absl::optional<size_t> mline_index) {
    mline_index_ = mline_index;
  }

  void set_mid(const absl::optional<std::string>& mid) { mid_ = mid; }

  void set_direction(RtpTransceiverDirection direction) {
    direction_ = direction;
  }

  void set_current_direction(RtpTransceiverDirection direction);

  void set_fired_direction(absl::optional<RtpTransceiverDirection> direction);

  void set_created_by_addtrack(bool created_by_addtrack) {
    created_by_addtrack_ = created_by_addtrack;
  }

  void set_reused_for_addtrack(bool reused_for_addtrack) {
    reused_for_addtrack_ = reused_for_addtrack;
  }

  bool created_by_addtrack() const { return created_by_addtrack_; }

  bool reused_for_addtrack() const { return reused_for_addtrack_; }

  bool has_ever_been_used_to_send() const {
    return has_ever_been_used_to_send_;
  }

  void SetPeerConnectionClosed();

  void StopTransceiverProcedure();

  cricket::MediaType media_type() const override;
  absl::optional<std::string> mid() const override;
  rtc::scoped_refptr<RtpSenderInterface> sender() const override;
  rtc::scoped_refptr<RtpReceiverInterface> receiver() const override;
  bool stopped() const override;
  bool stopping() const override;
  RtpTransceiverDirection direction() const override;
  RTCError SetDirectionWithError(
      RtpTransceiverDirection new_direction) override;
  absl::optional<RtpTransceiverDirection> current_direction() const override;
  absl::optional<RtpTransceiverDirection> fired_direction() const override;
  RTCError StopStandard() override;
  void StopInternal() override;
  RTCError SetCodecPreferences(
      rtc::ArrayView<RtpCodecCapability> codecs) override;
  std::vector<RtpCodecCapability> codec_preferences() const override {
    return codec_preferences_;
  }
  std::vector<RtpHeaderExtensionCapability> GetHeaderExtensionsToNegotiate()
      const override;
  std::vector<RtpHeaderExtensionCapability> GetNegotiatedHeaderExtensions()
      const override;
  RTCError SetHeaderExtensionsToNegotiate(
      rtc::ArrayView<const RtpHeaderExtensionCapability> header_extensions)
      override;

  void OnNegotiationUpdate(SdpType sdp_type,
                           const cricket::MediaContentDescription* content);

 private:
  cricket::MediaEngineInterface* media_engine() const {
    return context_->media_engine();
  }
  ConnectionContext* context() const { return context_; }
  void OnFirstPacketReceived();
  void StopSendingAndReceiving();

  void PushNewMediaChannelAndDeleteChannel(
      std::unique_ptr<cricket::ChannelInterface> channel_to_delete);

  TaskQueueBase* const thread_;
  const bool unified_plan_;
  const cricket::MediaType media_type_;
  rtc::scoped_refptr<PendingTaskSafetyFlag> signaling_thread_safety_;
  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
      senders_;
  std::vector<
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>>
      receivers_;

  bool stopped_ RTC_GUARDED_BY(thread_) = false;
  bool stopping_ RTC_GUARDED_BY(thread_) = false;
  bool is_pc_closed_ = false;
  RtpTransceiverDirection direction_ = RtpTransceiverDirection::kInactive;
  absl::optional<RtpTransceiverDirection> current_direction_;
  absl::optional<RtpTransceiverDirection> fired_direction_;
  absl::optional<std::string> mid_;
  absl::optional<size_t> mline_index_;
  bool created_by_addtrack_ = false;
  bool reused_for_addtrack_ = false;
  bool has_ever_been_used_to_send_ = false;

  std::unique_ptr<cricket::ChannelInterface> channel_ = nullptr;
  ConnectionContext* const context_;
  std::vector<RtpCodecCapability> codec_preferences_;
  std::vector<RtpHeaderExtensionCapability> header_extensions_to_negotiate_;

  cricket::RtpHeaderExtensions negotiated_header_extensions_
      RTC_GUARDED_BY(thread_);

  const std::function<void()> on_negotiation_needed_;
};

BEGIN_PRIMARY_PROXY_MAP(RtpTransceiver)

PROXY_PRIMARY_THREAD_DESTRUCTOR()
BYPASS_PROXY_CONSTMETHOD0(cricket::MediaType, media_type)
PROXY_CONSTMETHOD0(absl::optional<std::string>, mid)
PROXY_CONSTMETHOD0(rtc::scoped_refptr<RtpSenderInterface>, sender)
PROXY_CONSTMETHOD0(rtc::scoped_refptr<RtpReceiverInterface>, receiver)
PROXY_CONSTMETHOD0(bool, stopped)
PROXY_CONSTMETHOD0(bool, stopping)
PROXY_CONSTMETHOD0(RtpTransceiverDirection, direction)
PROXY_METHOD1(RTCError, SetDirectionWithError, RtpTransceiverDirection)
PROXY_CONSTMETHOD0(absl::optional<RtpTransceiverDirection>, current_direction)
PROXY_CONSTMETHOD0(absl::optional<RtpTransceiverDirection>, fired_direction)
PROXY_METHOD0(RTCError, StopStandard)
PROXY_METHOD0(void, StopInternal)
PROXY_METHOD1(RTCError, SetCodecPreferences, rtc::ArrayView<RtpCodecCapability>)
PROXY_CONSTMETHOD0(std::vector<RtpCodecCapability>, codec_preferences)
PROXY_CONSTMETHOD0(std::vector<RtpHeaderExtensionCapability>,
                   GetHeaderExtensionsToNegotiate)
PROXY_CONSTMETHOD0(std::vector<RtpHeaderExtensionCapability>,
                   GetNegotiatedHeaderExtensions)
PROXY_METHOD1(RTCError,
              SetHeaderExtensionsToNegotiate,
              rtc::ArrayView<const RtpHeaderExtensionCapability>)
END_PROXY_MAP(RtpTransceiver)

}

#endif
