/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_RTP_TRANSMISSION_MANAGER_H_
#define PC_RTP_TRANSMISSION_MANAGER_H_

#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "media/base/media_channel.h"
#include "pc/legacy_stats_collector_interface.h"
#include "pc/rtp_receiver.h"
#include "pc/rtp_receiver_proxy.h"
#include "pc/rtp_sender.h"
#include "pc/rtp_sender_proxy.h"
#include "pc/rtp_transceiver.h"
#include "pc/transceiver_list.h"
#include "pc/usage_pattern.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/weak_ptr.h"

namespace rtc {
class Thread;
}

namespace webrtc {

struct RtpSenderInfo {
  RtpSenderInfo() : first_ssrc(0) {}
  RtpSenderInfo(const std::string& stream_id,
                const std::string& sender_id,
                uint32_t ssrc)
      : stream_id(stream_id), sender_id(sender_id), first_ssrc(ssrc) {}
  bool operator==(const RtpSenderInfo& other) {
    return this->stream_id == other.stream_id &&
           this->sender_id == other.sender_id &&
           this->first_ssrc == other.first_ssrc;
  }
  std::string stream_id;
  std::string sender_id;

  uint32_t first_ssrc;
};

class RtpTransmissionManager : public RtpSenderBase::SetStreamsObserver {
 public:
  RtpTransmissionManager(bool is_unified_plan,
                         ConnectionContext* context,
                         UsagePattern* usage_pattern,
                         PeerConnectionObserver* observer,
                         LegacyStatsCollectorInterface* legacy_stats,
                         std::function<void()> on_negotiation_needed);

  RtpTransmissionManager(const RtpTransmissionManager&) = delete;
  RtpTransmissionManager& operator=(const RtpTransmissionManager&) = delete;

  void Close();

  void OnSetStreams() override;

  RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids,
      const std::vector<RtpEncodingParameters>* init_send_encodings);

  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
  CreateSender(cricket::MediaType media_type,
               const std::string& id,
               rtc::scoped_refptr<MediaStreamTrackInterface> track,
               const std::vector<std::string>& stream_ids,
               const std::vector<RtpEncodingParameters>& send_encodings);

  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
  CreateReceiver(cricket::MediaType media_type, const std::string& receiver_id);

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  CreateAndAddTransceiver(
      rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender,
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
          receiver);

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  FindFirstTransceiverForAddedTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<RtpEncodingParameters>* init_send_encodings);

  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
  GetSendersInternal() const;

  std::vector<
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>>
  GetReceiversInternal() const;

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  GetAudioTransceiver() const;

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  GetVideoTransceiver() const;

  void AddAudioTrack(AudioTrackInterface* track, MediaStreamInterface* stream);

  void RemoveAudioTrack(AudioTrackInterface* track,
                        MediaStreamInterface* stream);

  void AddVideoTrack(VideoTrackInterface* track, MediaStreamInterface* stream);

  void RemoveVideoTrack(VideoTrackInterface* track,
                        MediaStreamInterface* stream);

  void OnRemoteSenderAdded(const RtpSenderInfo& sender_info,
                           MediaStreamInterface* stream,
                           cricket::MediaType media_type);

  void OnRemoteSenderRemoved(const RtpSenderInfo& sender_info,
                             MediaStreamInterface* stream,
                             cricket::MediaType media_type);

  void OnLocalSenderAdded(const RtpSenderInfo& sender_info,
                          cricket::MediaType media_type);

  void OnLocalSenderRemoved(const RtpSenderInfo& sender_info,
                            cricket::MediaType media_type);

  std::vector<RtpSenderInfo>* GetRemoteSenderInfos(
      cricket::MediaType media_type);
  std::vector<RtpSenderInfo>* GetLocalSenderInfos(
      cricket::MediaType media_type);
  const RtpSenderInfo* FindSenderInfo(const std::vector<RtpSenderInfo>& infos,
                                      const std::string& stream_id,
                                      const std::string& sender_id) const;

  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
  FindSenderForTrack(MediaStreamTrackInterface* track) const;

  rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>
  FindSenderById(const std::string& sender_id) const;

  rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
  FindReceiverById(const std::string& receiver_id) const;

  TransceiverList* transceivers() { return &transceivers_; }
  const TransceiverList* transceivers() const { return &transceivers_; }

  cricket::VoiceMediaSendChannelInterface* voice_media_send_channel() const;
  cricket::VideoMediaSendChannelInterface* video_media_send_channel() const;
  cricket::VoiceMediaReceiveChannelInterface* voice_media_receive_channel()
      const;
  cricket::VideoMediaReceiveChannelInterface* video_media_receive_channel()
      const;

 private:
  rtc::Thread* signaling_thread() const { return context_->signaling_thread(); }
  rtc::Thread* worker_thread() const { return context_->worker_thread(); }
  bool IsUnifiedPlan() const { return is_unified_plan_; }
  void NoteUsageEvent(UsageEvent event) {
    usage_pattern_->NoteUsageEvent(event);
  }

  RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrackUnifiedPlan(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids,
      const std::vector<RtpEncodingParameters>* init_send_encodings);

  RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrackPlanB(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids,
      const std::vector<RtpEncodingParameters>* init_send_encodings);

  void CreateAudioReceiver(MediaStreamInterface* stream,
                           const RtpSenderInfo& remote_sender_info)
      RTC_RUN_ON(signaling_thread());

  void CreateVideoReceiver(MediaStreamInterface* stream,
                           const RtpSenderInfo& remote_sender_info)
      RTC_RUN_ON(signaling_thread());
  rtc::scoped_refptr<RtpReceiverInterface> RemoveAndStopReceiver(
      const RtpSenderInfo& remote_sender_info) RTC_RUN_ON(signaling_thread());

  PeerConnectionObserver* Observer() const;
  void OnNegotiationNeeded();

  cricket::MediaEngineInterface* media_engine() const;

  rtc::UniqueRandomIdGenerator* ssrc_generator() const {
    return context_->ssrc_generator();
  }

  TransceiverList transceivers_;

  std::vector<RtpSenderInfo> remote_audio_sender_infos_
      RTC_GUARDED_BY(signaling_thread());
  std::vector<RtpSenderInfo> remote_video_sender_infos_
      RTC_GUARDED_BY(signaling_thread());
  std::vector<RtpSenderInfo> local_audio_sender_infos_
      RTC_GUARDED_BY(signaling_thread());
  std::vector<RtpSenderInfo> local_video_sender_infos_
      RTC_GUARDED_BY(signaling_thread());

  bool closed_ = false;
  bool const is_unified_plan_;
  ConnectionContext* context_;
  UsagePattern* usage_pattern_;
  PeerConnectionObserver* observer_;
  LegacyStatsCollectorInterface* const legacy_stats_;
  std::function<void()> on_negotiation_needed_;
  rtc::WeakPtrFactory<RtpTransmissionManager> weak_ptr_factory_
      RTC_GUARDED_BY(signaling_thread());
};

}

#endif
