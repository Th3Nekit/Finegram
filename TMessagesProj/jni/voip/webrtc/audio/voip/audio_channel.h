/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_VOIP_AUDIO_CHANNEL_H_
#define AUDIO_VOIP_AUDIO_CHANNEL_H_

#include <map>
#include <memory>
#include <queue>
#include <utility>

#include "api/task_queue/task_queue_factory.h"
#include "api/voip/voip_base.h"
#include "api/voip/voip_statistics.h"
#include "audio/voip/audio_egress.h"
#include "audio/voip/audio_ingress.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl2.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class AudioChannel : public rtc::RefCountInterface {
 public:
  AudioChannel(Transport* transport,
               uint32_t local_ssrc,
               TaskQueueFactory* task_queue_factory,
               AudioMixer* audio_mixer,
               rtc::scoped_refptr<AudioDecoderFactory> decoder_factory);
  ~AudioChannel() override;

  void SetId(ChannelId id) { id_ = id; }
  ChannelId GetId() const { return id_; }

  bool StartSend();
  void StopSend();
  bool StartPlay();
  void StopPlay();

  bool IsSendingMedia() const { return egress_->IsSending(); }
  AudioSender* GetAudioSender() { return egress_.get(); }
  void SetEncoder(int payload_type,
                  const SdpAudioFormat& encoder_format,
                  std::unique_ptr<AudioEncoder> encoder) {
    egress_->SetEncoder(payload_type, encoder_format, std::move(encoder));
  }
  absl::optional<SdpAudioFormat> GetEncoderFormat() const {
    return egress_->GetEncoderFormat();
  }
  void RegisterTelephoneEventType(int rtp_payload_type, int sample_rate_hz) {
    egress_->RegisterTelephoneEventType(rtp_payload_type, sample_rate_hz);
  }
  bool SendTelephoneEvent(int dtmf_event, int duration_ms) {
    return egress_->SendTelephoneEvent(dtmf_event, duration_ms);
  }
  void SetMute(bool enable) { egress_->SetMute(enable); }

  bool IsPlaying() const { return ingress_->IsPlaying(); }
  void ReceivedRTPPacket(rtc::ArrayView<const uint8_t> rtp_packet) {
    ingress_->ReceivedRTPPacket(rtp_packet);
  }
  void ReceivedRTCPPacket(rtc::ArrayView<const uint8_t> rtcp_packet) {
    ingress_->ReceivedRTCPPacket(rtcp_packet);
  }
  void SetReceiveCodecs(const std::map<int, SdpAudioFormat>& codecs) {
    ingress_->SetReceiveCodecs(codecs);
  }
  IngressStatistics GetIngressStatistics();
  ChannelStatistics GetChannelStatistics();

  double GetInputAudioLevel() const {
    return egress_->GetInputAudioLevel() / 32767.0;
  }
  double GetInputTotalEnergy() const { return egress_->GetInputTotalEnergy(); }
  double GetInputTotalDuration() const {
    return egress_->GetInputTotalDuration();
  }
  double GetOutputAudioLevel() const {
    return ingress_->GetOutputAudioLevel() / 32767.0;
  }
  double GetOutputTotalEnergy() const {
    return ingress_->GetOutputTotalEnergy();
  }
  double GetOutputTotalDuration() const {
    return ingress_->GetOutputTotalDuration();
  }

  void SendRTCPReportForTesting(RTCPPacketType type) {
    int32_t result = rtp_rtcp_->SendRTCP(type);
    RTC_DCHECK(result == 0);
  }

 private:

  ChannelId id_;

  AudioMixer* audio_mixer_;

  std::unique_ptr<ReceiveStatistics> receive_statistics_;
  std::unique_ptr<ModuleRtpRtcpImpl2> rtp_rtcp_;
  std::unique_ptr<AudioIngress> ingress_;
  std::unique_ptr<AudioEgress> egress_;
};

}

#endif
