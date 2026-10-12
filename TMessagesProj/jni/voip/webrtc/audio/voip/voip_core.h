/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_VOIP_VOIP_CORE_H_
#define AUDIO_VOIP_VOIP_CORE_H_

#include <map>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/voip/voip_base.h"
#include "api/voip/voip_codec.h"
#include "api/voip/voip_dtmf.h"
#include "api/voip/voip_engine.h"
#include "api/voip/voip_network.h"
#include "api/voip/voip_statistics.h"
#include "api/voip/voip_volume_control.h"
#include "audio/audio_transport_impl.h"
#include "audio/voip/audio_channel.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

class VoipCore : public VoipEngine,
                 public VoipBase,
                 public VoipNetwork,
                 public VoipCodec,
                 public VoipDtmf,
                 public VoipStatistics,
                 public VoipVolumeControl {
 public:

  VoipCore(rtc::scoped_refptr<AudioEncoderFactory> encoder_factory,
           rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
           std::unique_ptr<TaskQueueFactory> task_queue_factory,
           rtc::scoped_refptr<AudioDeviceModule> audio_device_module,
           rtc::scoped_refptr<AudioProcessing> audio_processing);
  ~VoipCore() override = default;

  VoipBase& Base() override { return *this; }
  VoipNetwork& Network() override { return *this; }
  VoipCodec& Codec() override { return *this; }
  VoipDtmf& Dtmf() override { return *this; }
  VoipStatistics& Statistics() override { return *this; }
  VoipVolumeControl& VolumeControl() override { return *this; }

  ChannelId CreateChannel(Transport* transport,
                          absl::optional<uint32_t> local_ssrc) override;
  VoipResult ReleaseChannel(ChannelId channel_id) override;
  VoipResult StartSend(ChannelId channel_id) override;
  VoipResult StopSend(ChannelId channel_id) override;
  VoipResult StartPlayout(ChannelId channel_id) override;
  VoipResult StopPlayout(ChannelId channel_id) override;

  VoipResult ReceivedRTPPacket(
      ChannelId channel_id,
      rtc::ArrayView<const uint8_t> rtp_packet) override;
  VoipResult ReceivedRTCPPacket(
      ChannelId channel_id,
      rtc::ArrayView<const uint8_t> rtcp_packet) override;

  VoipResult SetSendCodec(ChannelId channel_id,
                          int payload_type,
                          const SdpAudioFormat& encoder_format) override;
  VoipResult SetReceiveCodecs(
      ChannelId channel_id,
      const std::map<int, SdpAudioFormat>& decoder_specs) override;

  VoipResult RegisterTelephoneEventType(ChannelId channel_id,
                                        int rtp_payload_type,
                                        int sample_rate_hz) override;
  VoipResult SendDtmfEvent(ChannelId channel_id,
                           DtmfEvent dtmf_event,
                           int duration_ms) override;

  VoipResult GetIngressStatistics(ChannelId channel_id,
                                  IngressStatistics& ingress_stats) override;
  VoipResult GetChannelStatistics(ChannelId channe_id,
                                  ChannelStatistics& channel_stats) override;

  VoipResult SetInputMuted(ChannelId channel_id, bool enable) override;
  VoipResult GetInputVolumeInfo(ChannelId channel_id,
                                VolumeInfo& volume_info) override;
  VoipResult GetOutputVolumeInfo(ChannelId channel_id,
                                 VolumeInfo& volume_info) override;

 private:

  bool InitializeIfNeeded();

  rtc::scoped_refptr<AudioChannel> GetChannel(ChannelId channel_id);

  bool UpdateAudioTransportWithSenders();

  rtc::scoped_refptr<AudioEncoderFactory> encoder_factory_;
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_;
  std::unique_ptr<TaskQueueFactory> task_queue_factory_;

  rtc::scoped_refptr<AudioProcessing> audio_processing_;

  rtc::scoped_refptr<AudioMixer> audio_mixer_;

  std::unique_ptr<AudioTransportImpl> audio_transport_;

  rtc::scoped_refptr<AudioDeviceModule> audio_device_module_;

  Mutex lock_;

  int next_channel_id_ RTC_GUARDED_BY(lock_) = 0;

  std::unordered_map<ChannelId, rtc::scoped_refptr<AudioChannel>> channels_
      RTC_GUARDED_BY(lock_);

  bool initialized_ RTC_GUARDED_BY(lock_) = false;
};

}

#endif
