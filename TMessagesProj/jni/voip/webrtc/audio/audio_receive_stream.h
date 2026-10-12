/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_AUDIO_RECEIVE_STREAM_H_
#define AUDIO_AUDIO_RECEIVE_STREAM_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/audio/audio_mixer.h"
#include "api/neteq/neteq_factory.h"
#include "api/rtp_headers.h"
#include "api/sequence_checker.h"
#include "audio/audio_state.h"
#include "call/audio_receive_stream.h"
#include "call/syncable.h"
#include "modules/rtp_rtcp/source/source_tracker.h"
#include "rtc_base/system/no_unique_address.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
class PacketRouter;
class RtcEventLog;
class RtpStreamReceiverControllerInterface;
class RtpStreamReceiverInterface;

namespace voe {
class ChannelReceiveInterface;
}

namespace internal {
class AudioSendStream;
}

class AudioReceiveStreamImpl final : public webrtc::AudioReceiveStreamInterface,
                                     public AudioMixer::Source,
                                     public Syncable {
 public:
  AudioReceiveStreamImpl(
      Clock* clock,
      PacketRouter* packet_router,
      NetEqFactory* neteq_factory,
      const webrtc::AudioReceiveStreamInterface::Config& config,
      const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
      webrtc::RtcEventLog* event_log);

  AudioReceiveStreamImpl(
      Clock* clock,
      PacketRouter* packet_router,
      const webrtc::AudioReceiveStreamInterface::Config& config,
      const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
      webrtc::RtcEventLog* event_log,
      std::unique_ptr<voe::ChannelReceiveInterface> channel_receive);

  AudioReceiveStreamImpl() = delete;
  AudioReceiveStreamImpl(const AudioReceiveStreamImpl&) = delete;
  AudioReceiveStreamImpl& operator=(const AudioReceiveStreamImpl&) = delete;

  ~AudioReceiveStreamImpl() override;

  void RegisterWithTransport(
      RtpStreamReceiverControllerInterface* receiver_controller);

  void UnregisterFromTransport();

  void Start() override;
  void Stop() override;
  bool IsRunning() const override;
  void SetDepacketizerToDecoderFrameTransformer(
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer)
      override;
  void SetDecoderMap(std::map<int, SdpAudioFormat> decoder_map) override;
  void SetNackHistory(int history_ms) override;
  void SetNonSenderRttMeasurement(bool enabled) override;
  void SetFrameDecryptor(rtc::scoped_refptr<webrtc::FrameDecryptorInterface>
                             frame_decryptor) override;

  webrtc::AudioReceiveStreamInterface::Stats GetStats(
      bool get_and_clear_legacy_stats) const override;
  void SetSink(AudioSinkInterface* sink) override;
  void SetGain(float gain) override;
  bool SetBaseMinimumPlayoutDelayMs(int delay_ms) override;
  int GetBaseMinimumPlayoutDelayMs() const override;
  std::vector<webrtc::RtpSource> GetSources() const override;

  AudioFrameInfo GetAudioFrameWithInfo(int sample_rate_hz,
                                       AudioFrame* audio_frame) override;
  int Ssrc() const override;
  int PreferredSampleRate() const override;

  uint32_t id() const override;
  absl::optional<Syncable::Info> GetInfo() const override;
  bool GetPlayoutRtpTimestamp(uint32_t* rtp_timestamp,
                              int64_t* time_ms) const override;
  void SetEstimatedPlayoutNtpTimestampMs(int64_t ntp_timestamp_ms,
                                         int64_t time_ms) override;
  bool SetMinimumPlayoutDelay(int delay_ms) override;

  void AssociateSendStream(internal::AudioSendStream* send_stream);
  void DeliverRtcp(const uint8_t* packet, size_t length);

  void SetSyncGroup(absl::string_view sync_group);

  void SetLocalSsrc(uint32_t local_ssrc);

  uint32_t local_ssrc() const;

  uint32_t remote_ssrc() const override {

    return config_.rtp.remote_ssrc;
  }

  const std::string& sync_group() const;

  const AudioSendStream* GetAssociatedSendStreamForTesting() const;

  void ReconfigureForTesting(
      const webrtc::AudioReceiveStreamInterface::Config& config);

 private:
  internal::AudioState* audio_state() const;

  RTC_NO_UNIQUE_ADDRESS SequenceChecker worker_thread_checker_;

  RTC_NO_UNIQUE_ADDRESS SequenceChecker packet_sequence_checker_{
      SequenceChecker::kDetached};
  webrtc::AudioReceiveStreamInterface::Config config_;
  rtc::scoped_refptr<webrtc::AudioState> audio_state_;
  SourceTracker source_tracker_;
  const std::unique_ptr<voe::ChannelReceiveInterface> channel_receive_;
  AudioSendStream* associated_send_stream_
      RTC_GUARDED_BY(packet_sequence_checker_) = nullptr;

  bool playing_ RTC_GUARDED_BY(worker_thread_checker_) = false;

  std::unique_ptr<RtpStreamReceiverInterface> rtp_stream_receiver_
      RTC_GUARDED_BY(packet_sequence_checker_);
};
}

#endif
