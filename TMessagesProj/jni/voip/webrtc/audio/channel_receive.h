/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_CHANNEL_RECEIVE_H_
#define AUDIO_CHANNEL_RECEIVE_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/call/audio_sink.h"
#include "api/call/transport.h"
#include "api/crypto/crypto_options.h"
#include "api/frame_transformer_interface.h"
#include "api/neteq/neteq_factory.h"
#include "api/transport/rtp/rtp_source.h"
#include "call/rtp_packet_sink_interface.h"
#include "call/syncable.h"
#include "modules/audio_coding/include/audio_coding_module_typedefs.h"
#include "modules/rtp_rtcp/source/source_tracker.h"
#include "system_wrappers/include/clock.h"

namespace rtc {
class TimestampWrapAroundHandler;
}

namespace webrtc {

class AudioDeviceModule;
class FrameDecryptorInterface;
class PacketRouter;
class RateLimiter;
class ReceiveStatistics;
class RtcEventLog;
class RtpPacketReceived;
class RtpRtcp;

struct CallReceiveStatistics {
  int cumulativeLost;
  unsigned int jitterSamples;
  int64_t payload_bytes_received = 0;
  int64_t header_and_padding_bytes_received = 0;
  int packetsReceived;
  uint32_t nacks_sent = 0;

  int64_t capture_start_ntp_time_ms_;

  absl::optional<Timestamp> last_packet_received;

  absl::optional<int64_t> last_sender_report_timestamp_ms;
  absl::optional<int64_t> last_sender_report_remote_timestamp_ms;
  uint64_t sender_reports_packets_sent = 0;
  uint64_t sender_reports_bytes_sent = 0;
  uint64_t sender_reports_reports_count = 0;
  absl::optional<TimeDelta> round_trip_time;
  TimeDelta total_round_trip_time = TimeDelta::Zero();
  int round_trip_time_measurements;
};

namespace voe {

class ChannelSendInterface;

class ChannelReceiveInterface : public RtpPacketSinkInterface {
 public:
  virtual ~ChannelReceiveInterface() = default;

  virtual void SetSink(AudioSinkInterface* sink) = 0;

  virtual void SetReceiveCodecs(
      const std::map<int, SdpAudioFormat>& codecs) = 0;

  virtual void StartPlayout() = 0;
  virtual void StopPlayout() = 0;

  virtual absl::optional<std::pair<int, SdpAudioFormat>> GetReceiveCodec()
      const = 0;

  virtual void ReceivedRTCPPacket(const uint8_t* data, size_t length) = 0;

  virtual void SetChannelOutputVolumeScaling(float scaling) = 0;
  virtual int GetSpeechOutputLevelFullRange() const = 0;

  virtual double GetTotalOutputEnergy() const = 0;
  virtual double GetTotalOutputDuration() const = 0;

  virtual NetworkStatistics GetNetworkStatistics(
      bool get_and_clear_legacy_stats) const = 0;
  virtual AudioDecodingCallStats GetDecodingCallStatistics() const = 0;

  virtual uint32_t GetDelayEstimate() const = 0;
  virtual bool SetMinimumPlayoutDelay(int delay_ms) = 0;
  virtual bool GetPlayoutRtpTimestamp(uint32_t* rtp_timestamp,
                                      int64_t* time_ms) const = 0;
  virtual void SetEstimatedPlayoutNtpTimestampMs(int64_t ntp_timestamp_ms,
                                                 int64_t time_ms) = 0;
  virtual absl::optional<int64_t> GetCurrentEstimatedPlayoutNtpTimestampMs(
      int64_t now_ms) const = 0;

  virtual bool SetBaseMinimumPlayoutDelayMs(int delay_ms) = 0;
  virtual int GetBaseMinimumPlayoutDelayMs() const = 0;

  virtual absl::optional<Syncable::Info> GetSyncInfo() const = 0;

  virtual void RegisterReceiverCongestionControlObjects(
      PacketRouter* packet_router) = 0;
  virtual void ResetReceiverCongestionControlObjects() = 0;

  virtual CallReceiveStatistics GetRTCPStatistics() const = 0;
  virtual void SetNACKStatus(bool enable, int max_packets) = 0;
  virtual void SetNonSenderRttMeasurement(bool enabled) = 0;

  virtual AudioMixer::Source::AudioFrameInfo GetAudioFrameWithInfo(
      int sample_rate_hz,
      AudioFrame* audio_frame) = 0;

  virtual int PreferredSampleRate() const = 0;

  virtual void SetSourceTracker(SourceTracker* source_tracker) = 0;

  virtual void SetAssociatedSendChannel(
      const ChannelSendInterface* channel) = 0;

  virtual void SetDepacketizerToDecoderFrameTransformer(
      rtc::scoped_refptr<webrtc::FrameTransformerInterface>
          frame_transformer) = 0;

  virtual void SetFrameDecryptor(
      rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor) = 0;

  virtual void OnLocalSsrcChange(uint32_t local_ssrc) = 0;
  virtual uint32_t GetLocalSsrc() const = 0;
};

std::unique_ptr<ChannelReceiveInterface> CreateChannelReceive(
    Clock* clock,
    NetEqFactory* neteq_factory,
    AudioDeviceModule* audio_device_module,
    Transport* rtcp_send_transport,
    RtcEventLog* rtc_event_log,
    uint32_t local_ssrc,
    uint32_t remote_ssrc,
    size_t jitter_buffer_max_packets,
    bool jitter_buffer_fast_playout,
    int jitter_buffer_min_delay_ms,
    bool enable_non_sender_rtt,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
    absl::optional<AudioCodecPairId> codec_pair_id,
    rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor,
    const webrtc::CryptoOptions& crypto_options,
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer);

}
}

#endif
