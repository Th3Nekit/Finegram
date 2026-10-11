/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_VOIP_AUDIO_EGRESS_H_
#define AUDIO_VOIP_AUDIO_EGRESS_H_

#include <memory>
#include <string>

#include "api/audio_codecs/audio_format.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "audio/audio_level.h"
#include "audio/utility/audio_frame_operations.h"
#include "call/audio_sender.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/rtp_rtcp/include/report_block_data.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_interface.h"
#include "modules/rtp_rtcp/source/rtp_sender_audio.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

class AudioEgress : public AudioSender, public AudioPacketizationCallback {
 public:
  AudioEgress(RtpRtcpInterface* rtp_rtcp,
              Clock* clock,
              TaskQueueFactory* task_queue_factory);
  ~AudioEgress() override;

  void SetEncoder(int payload_type,
                  const SdpAudioFormat& encoder_format,
                  std::unique_ptr<AudioEncoder> encoder);

  bool StartSend();
  void StopSend();

  bool IsSending() const;

  void SetMute(bool mute);

  absl::optional<SdpAudioFormat> GetEncoderFormat() const {
    MutexLock lock(&lock_);
    return encoder_format_;
  }

  void RegisterTelephoneEventType(int rtp_payload_type, int sample_rate_hz);

  bool SendTelephoneEvent(int dtmf_event, int duration_ms);

  int GetInputAudioLevel() const { return input_audio_level_.LevelFullRange(); }
  double GetInputTotalEnergy() const {
    return input_audio_level_.TotalEnergy();
  }
  double GetInputTotalDuration() const {
    return input_audio_level_.TotalDuration();
  }

  void SendAudioData(std::unique_ptr<AudioFrame> audio_frame) override;

  int32_t SendData(AudioFrameType frame_type,
                   uint8_t payload_type,
                   uint32_t timestamp,
                   const uint8_t* payload_data,
                   size_t payload_size) override;

 private:
  void SetEncoderFormat(const SdpAudioFormat& encoder_format) {
    MutexLock lock(&lock_);
    encoder_format_ = encoder_format;
  }

  mutable Mutex lock_;

  absl::optional<SdpAudioFormat> encoder_format_ RTC_GUARDED_BY(lock_);

  RtpRtcpInterface* const rtp_rtcp_;

  RTPSenderAudio rtp_sender_audio_;

  const std::unique_ptr<AudioCodingModule> audio_coding_;

  voe::AudioLevel input_audio_level_;

  struct EncoderContext {

    uint32_t frame_rtp_timestamp_ = 0;

    bool mute_ = false;
    bool previously_muted_ = false;
  };

  EncoderContext encoder_context_ RTC_GUARDED_BY(encoder_queue_checker_);

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> encoder_queue_;
  RTC_NO_UNIQUE_ADDRESS SequenceChecker encoder_queue_checker_;
};

}

#endif
