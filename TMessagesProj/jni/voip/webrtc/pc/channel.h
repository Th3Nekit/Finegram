/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_CHANNEL_H_
#define PC_CHANNEL_H_

#include <stdint.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/crypto/crypto_options.h"
#include "api/jsep.h"
#include "api/media_types.h"
#include "api/rtp_parameters.h"
#include "api/rtp_transceiver_direction.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "call/rtp_demuxer.h"
#include "call/rtp_packet_sink_interface.h"
#include "media/base/media_channel.h"
#include "media/base/media_channel_impl.h"
#include "media/base/stream_params.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "pc/channel_interface.h"
#include "pc/rtp_transport_internal.h"
#include "pc/session_description.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_set.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/network_route.h"
#include "rtc_base/socket.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/unique_id_generator.h"

namespace cricket {

class VideoChannel;
class VoiceChannel;

class BaseChannel : public ChannelInterface,

                    public MediaChannelNetworkInterface,
                    public webrtc::RtpPacketSinkInterface {
 public:

  BaseChannel(
      webrtc::TaskQueueBase* worker_thread,
      rtc::Thread* network_thread,
      webrtc::TaskQueueBase* signaling_thread,
      std::unique_ptr<MediaSendChannelInterface> media_send_channel,
      std::unique_ptr<MediaReceiveChannelInterface> media_receive_channel,
      absl::string_view mid,
      bool srtp_required,
      webrtc::CryptoOptions crypto_options,
      rtc::UniqueRandomIdGenerator* ssrc_generator);
  virtual ~BaseChannel();

  webrtc::TaskQueueBase* worker_thread() const { return worker_thread_; }
  rtc::Thread* network_thread() const { return network_thread_; }
  const std::string& mid() const override { return demuxer_criteria_.mid(); }

  absl::string_view transport_name() const override {
    RTC_DCHECK_RUN_ON(network_thread());
    if (rtp_transport_)
      return rtp_transport_->transport_name();
    return "";
  }

  bool srtp_active() const {
    RTC_DCHECK_RUN_ON(network_thread());
    return rtp_transport_ && rtp_transport_->IsSrtpActive();
  }

  bool SetRtpTransport(webrtc::RtpTransportInternal* rtp_transport) override;

  webrtc::RtpTransportInternal* rtp_transport() const {
    RTC_DCHECK_RUN_ON(network_thread());
    return rtp_transport_;
  }

  bool SetLocalContent(const MediaContentDescription* content,
                       webrtc::SdpType type,
                       std::string& error_desc) override;
  bool SetRemoteContent(const MediaContentDescription* content,
                        webrtc::SdpType type,
                        std::string& error_desc) override;

  bool SetPayloadTypeDemuxingEnabled(bool enabled) override;

  void Enable(bool enable) override;

  const std::vector<StreamParams>& local_streams() const override {
    return local_streams_;
  }
  const std::vector<StreamParams>& remote_streams() const override {
    return remote_streams_;
  }

  void SetFirstPacketReceivedCallback(std::function<void()> callback) override;

  void OnTransportReadyToSend(bool ready);

  int SetOption(SocketType type, rtc::Socket::Option o, int val) override;

  void OnRtpPacket(const webrtc::RtpPacketReceived& packet) override;

  VideoMediaSendChannelInterface* video_media_send_channel() override {
    RTC_CHECK(false) << "Attempt to fetch video channel from non-video";
    return nullptr;
  }
  VoiceMediaSendChannelInterface* voice_media_send_channel() override {
    RTC_CHECK(false) << "Attempt to fetch voice channel from non-voice";
    return nullptr;
  }
  VideoMediaReceiveChannelInterface* video_media_receive_channel() override {
    RTC_CHECK(false) << "Attempt to fetch video channel from non-video";
    return nullptr;
  }
  VoiceMediaReceiveChannelInterface* voice_media_receive_channel() override {
    RTC_CHECK(false) << "Attempt to fetch voice channel from non-voice";
    return nullptr;
  }

 protected:
  void set_local_content_direction(webrtc::RtpTransceiverDirection direction)
      RTC_RUN_ON(worker_thread()) {
    local_content_direction_ = direction;
  }

  webrtc::RtpTransceiverDirection local_content_direction() const
      RTC_RUN_ON(worker_thread()) {
    return local_content_direction_;
  }

  void set_remote_content_direction(webrtc::RtpTransceiverDirection direction)
      RTC_RUN_ON(worker_thread()) {
    remote_content_direction_ = direction;
  }

  webrtc::RtpTransceiverDirection remote_content_direction() const
      RTC_RUN_ON(worker_thread()) {
    return remote_content_direction_;
  }

  webrtc::RtpExtension::Filter extensions_filter() const {
    return extensions_filter_;
  }

  bool network_initialized() RTC_RUN_ON(network_thread()) {
    return media_send_channel()->HasNetworkInterface();
  }

  bool enabled() const RTC_RUN_ON(worker_thread()) { return enabled_; }
  webrtc::TaskQueueBase* signaling_thread() const { return signaling_thread_; }

  bool IsReadyToSendMedia_w() const RTC_RUN_ON(worker_thread());

  bool SendPacket(rtc::CopyOnWriteBuffer* packet,
                  const rtc::PacketOptions& options) override;
  bool SendRtcp(rtc::CopyOnWriteBuffer* packet,
                const rtc::PacketOptions& options) override;

  void OnWritableState(bool writable);

  void OnNetworkRouteChanged(absl::optional<rtc::NetworkRoute> network_route);

  bool SendPacket(bool rtcp,
                  rtc::CopyOnWriteBuffer* packet,
                  const rtc::PacketOptions& options);

  void EnableMedia_w() RTC_RUN_ON(worker_thread());
  void DisableMedia_w() RTC_RUN_ON(worker_thread());

  void UpdateWritableState_n() RTC_RUN_ON(network_thread());
  void ChannelWritable_n() RTC_RUN_ON(network_thread());
  void ChannelNotWritable_n() RTC_RUN_ON(network_thread());

  bool SetPayloadTypeDemuxingEnabled_w(bool enabled)
      RTC_RUN_ON(worker_thread());

  virtual void UpdateMediaSendRecvState_w() RTC_RUN_ON(worker_thread()) = 0;

  bool UpdateLocalStreams_w(const std::vector<StreamParams>& streams,
                            webrtc::SdpType type,
                            std::string& error_desc)
      RTC_RUN_ON(worker_thread());
  bool UpdateRemoteStreams_w(const MediaContentDescription* content,
                             webrtc::SdpType type,
                             std::string& error_desc)
      RTC_RUN_ON(worker_thread());
  virtual bool SetLocalContent_w(const MediaContentDescription* content,
                                 webrtc::SdpType type,
                                 std::string& error_desc)
      RTC_RUN_ON(worker_thread()) = 0;
  virtual bool SetRemoteContent_w(const MediaContentDescription* content,
                                  webrtc::SdpType type,
                                  std::string& error_desc)
      RTC_RUN_ON(worker_thread()) = 0;

  RtpHeaderExtensions GetDeduplicatedRtpHeaderExtensions(
      const RtpHeaderExtensions& extensions);

  bool MaybeAddHandledPayloadType(int payload_type) RTC_RUN_ON(worker_thread());

  bool ClearHandledPayloadTypes() RTC_RUN_ON(worker_thread());

  bool MaybeUpdateDemuxerAndRtpExtensions_w(
      bool update_demuxer,
      absl::optional<RtpHeaderExtensions> extensions,
      std::string& error_desc) RTC_RUN_ON(worker_thread());

  bool RegisterRtpDemuxerSink_w() RTC_RUN_ON(worker_thread());

  std::string ToString() const;

  const std::unique_ptr<MediaSendChannelInterface> media_send_channel_;
  const std::unique_ptr<MediaReceiveChannelInterface> media_receive_channel_;

 private:
  bool ConnectToRtpTransport_n() RTC_RUN_ON(network_thread());
  void DisconnectFromRtpTransport_n() RTC_RUN_ON(network_thread());
  void SignalSentPacket_n(const rtc::SentPacket& sent_packet);

  webrtc::TaskQueueBase* const worker_thread_;
  rtc::Thread* const network_thread_;
  webrtc::TaskQueueBase* const signaling_thread_;
  rtc::scoped_refptr<webrtc::PendingTaskSafetyFlag> alive_;

  std::function<void()> on_first_packet_received_
      RTC_GUARDED_BY(network_thread());

  webrtc::RtpTransportInternal* rtp_transport_
      RTC_GUARDED_BY(network_thread()) = nullptr;

  std::vector<std::pair<rtc::Socket::Option, int> > socket_options_
      RTC_GUARDED_BY(network_thread());
  std::vector<std::pair<rtc::Socket::Option, int> > rtcp_socket_options_
      RTC_GUARDED_BY(network_thread());
  bool writable_ RTC_GUARDED_BY(network_thread()) = false;
  bool was_ever_writable_n_ RTC_GUARDED_BY(network_thread()) = false;
  bool was_ever_writable_ RTC_GUARDED_BY(worker_thread()) = false;
  const bool srtp_required_ = true;

  const webrtc::RtpExtension::Filter extensions_filter_;

  bool enabled_ RTC_GUARDED_BY(worker_thread()) = false;
  bool enabled_s_ RTC_GUARDED_BY(signaling_thread()) = false;
  bool payload_type_demuxing_enabled_ RTC_GUARDED_BY(worker_thread()) = true;
  std::vector<StreamParams> local_streams_ RTC_GUARDED_BY(worker_thread());
  std::vector<StreamParams> remote_streams_ RTC_GUARDED_BY(worker_thread());
  webrtc::RtpTransceiverDirection local_content_direction_ RTC_GUARDED_BY(
      worker_thread()) = webrtc::RtpTransceiverDirection::kInactive;
  webrtc::RtpTransceiverDirection remote_content_direction_ RTC_GUARDED_BY(
      worker_thread()) = webrtc::RtpTransceiverDirection::kInactive;

  webrtc::flat_set<uint8_t> payload_types_ RTC_GUARDED_BY(worker_thread());

  RtpHeaderExtensions rtp_header_extensions_ RTC_GUARDED_BY(worker_thread());

  webrtc::RtpDemuxerCriteria demuxer_criteria_;

  rtc::UniqueRandomIdGenerator* const ssrc_generator_;
};

class VoiceChannel : public BaseChannel {
 public:
  VoiceChannel(
      webrtc::TaskQueueBase* worker_thread,
      rtc::Thread* network_thread,
      webrtc::TaskQueueBase* signaling_thread,
      std::unique_ptr<VoiceMediaSendChannelInterface> send_channel_impl,
      std::unique_ptr<VoiceMediaReceiveChannelInterface> receive_channel_impl,
      absl::string_view mid,
      bool srtp_required,
      webrtc::CryptoOptions crypto_options,
      rtc::UniqueRandomIdGenerator* ssrc_generator);

  ~VoiceChannel();

  VideoChannel* AsVideoChannel() override {
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }
  VoiceChannel* AsVoiceChannel() override { return this; }

  VoiceMediaSendChannelInterface* send_channel() {
    return media_send_channel_->AsVoiceSendChannel();
  }

  VoiceMediaReceiveChannelInterface* receive_channel() {
    return media_receive_channel_->AsVoiceReceiveChannel();
  }

  VoiceMediaSendChannelInterface* media_send_channel() override {
    return send_channel();
  }

  VoiceMediaSendChannelInterface* voice_media_send_channel() override {
    return send_channel();
  }

  VoiceMediaReceiveChannelInterface* media_receive_channel() override {
    return receive_channel();
  }

  VoiceMediaReceiveChannelInterface* voice_media_receive_channel() override {
    return receive_channel();
  }

  cricket::MediaType media_type() const override {
    return cricket::MEDIA_TYPE_AUDIO;
  }

 private:

  void UpdateMediaSendRecvState_w() RTC_RUN_ON(worker_thread()) override;
  bool SetLocalContent_w(const MediaContentDescription* content,
                         webrtc::SdpType type,
                         std::string& error_desc)
      RTC_RUN_ON(worker_thread()) override;
  bool SetRemoteContent_w(const MediaContentDescription* content,
                          webrtc::SdpType type,
                          std::string& error_desc)
      RTC_RUN_ON(worker_thread()) override;

  AudioSenderParameter last_send_params_ RTC_GUARDED_BY(worker_thread());

  AudioReceiverParameters last_recv_params_ RTC_GUARDED_BY(worker_thread());
};

class VideoChannel : public BaseChannel {
 public:
  VideoChannel(
      webrtc::TaskQueueBase* worker_thread,
      rtc::Thread* network_thread,
      webrtc::TaskQueueBase* signaling_thread,
      std::unique_ptr<VideoMediaSendChannelInterface> media_send_channel,
      std::unique_ptr<VideoMediaReceiveChannelInterface> media_receive_channel,
      absl::string_view mid,
      bool srtp_required,
      webrtc::CryptoOptions crypto_options,
      rtc::UniqueRandomIdGenerator* ssrc_generator);
  ~VideoChannel();

  VideoChannel* AsVideoChannel() override { return this; }
  VoiceChannel* AsVoiceChannel() override {
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }

  VideoMediaSendChannelInterface* send_channel() {
    return media_send_channel_->AsVideoSendChannel();
  }

  VideoMediaReceiveChannelInterface* receive_channel() {
    return media_receive_channel_->AsVideoReceiveChannel();
  }

  VideoMediaSendChannelInterface* media_send_channel() override {
    return send_channel();
  }

  VideoMediaSendChannelInterface* video_media_send_channel() override {
    return send_channel();
  }

  VideoMediaReceiveChannelInterface* media_receive_channel() override {
    return receive_channel();
  }

  VideoMediaReceiveChannelInterface* video_media_receive_channel() override {
    return receive_channel();
  }

  cricket::MediaType media_type() const override {
    return cricket::MEDIA_TYPE_VIDEO;
  }

 private:

  void UpdateMediaSendRecvState_w() RTC_RUN_ON(worker_thread()) override;
  bool SetLocalContent_w(const MediaContentDescription* content,
                         webrtc::SdpType type,
                         std::string& error_desc)
      RTC_RUN_ON(worker_thread()) override;
  bool SetRemoteContent_w(const MediaContentDescription* content,
                          webrtc::SdpType type,
                          std::string& error_desc)
      RTC_RUN_ON(worker_thread()) override;

  VideoSenderParameters last_send_params_ RTC_GUARDED_BY(worker_thread());

  VideoReceiverParameters last_recv_params_ RTC_GUARDED_BY(worker_thread());
};

}

#endif
