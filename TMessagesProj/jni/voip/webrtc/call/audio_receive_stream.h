/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_AUDIO_RECEIVE_STREAM_H_
#define CALL_AUDIO_RECEIVE_STREAM_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/call/transport.h"
#include "api/crypto/crypto_options.h"
#include "api/rtp_parameters.h"
#include "call/receive_stream.h"
#include "call/rtp_config.h"

namespace webrtc {
class AudioSinkInterface;

class AudioReceiveStreamInterface : public MediaReceiveStreamInterface {
 public:
  struct Stats {
    Stats();
    ~Stats();
    uint32_t remote_ssrc = 0;
    int64_t payload_bytes_received = 0;
    int64_t header_and_padding_bytes_received = 0;
    uint32_t packets_received = 0;
    uint64_t fec_packets_received = 0;
    uint64_t fec_packets_discarded = 0;
    int32_t packets_lost = 0;
    uint64_t packets_discarded = 0;
    uint32_t nacks_sent = 0;
    std::string codec_name;
    absl::optional<int> codec_payload_type;
    uint32_t jitter_ms = 0;
    uint32_t jitter_buffer_ms = 0;
    uint32_t jitter_buffer_preferred_ms = 0;
    uint32_t delay_estimate_ms = 0;
    int32_t audio_level = -1;

    double total_output_energy = 0.0;
    uint64_t total_samples_received = 0;
    double total_output_duration = 0.0;
    uint64_t concealed_samples = 0;
    uint64_t silent_concealed_samples = 0;
    uint64_t concealment_events = 0;
    double jitter_buffer_delay_seconds = 0.0;
    uint64_t jitter_buffer_emitted_count = 0;
    double jitter_buffer_target_delay_seconds = 0.0;
    double jitter_buffer_minimum_delay_seconds = 0.0;
    uint64_t inserted_samples_for_deceleration = 0;
    uint64_t removed_samples_for_acceleration = 0;

    float expand_rate = 0.0f;
    float speech_expand_rate = 0.0f;
    float secondary_decoded_rate = 0.0f;
    float secondary_discarded_rate = 0.0f;
    float accelerate_rate = 0.0f;
    float preemptive_expand_rate = 0.0f;
    uint64_t delayed_packet_outage_samples = 0;
    int32_t decoding_calls_to_silence_generator = 0;
    int32_t decoding_calls_to_neteq = 0;
    int32_t decoding_normal = 0;

    int32_t decoding_plc = 0;
    int32_t decoding_codec_plc = 0;
    int32_t decoding_cng = 0;
    int32_t decoding_plc_cng = 0;
    int32_t decoding_muted_output = 0;
    int64_t capture_start_ntp_time_ms = 0;

    absl::optional<Timestamp> last_packet_received;
    uint64_t jitter_buffer_flushes = 0;
    double relative_packet_arrival_delay_seconds = 0.0;
    int32_t interruption_count = 0;
    int32_t total_interruption_duration_ms = 0;

    absl::optional<int64_t> estimated_playout_ntp_timestamp_ms;

    absl::optional<int64_t> last_sender_report_timestamp_ms;
    absl::optional<int64_t> last_sender_report_remote_timestamp_ms;
    uint64_t sender_reports_packets_sent = 0;
    uint64_t sender_reports_bytes_sent = 0;
    uint64_t sender_reports_reports_count = 0;
    absl::optional<TimeDelta> round_trip_time;
    TimeDelta total_round_trip_time = TimeDelta::Zero();
    int round_trip_time_measurements = 0;
  };

  struct Config {
    Config();
    ~Config();

    std::string ToString() const;

    struct Rtp : public ReceiveStreamRtpConfig {
      Rtp();
      ~Rtp();

      std::string ToString() const;

      NackConfig nack;
    } rtp;

    bool enable_non_sender_rtt = false;

    Transport* rtcp_send_transport = nullptr;

    size_t jitter_buffer_max_packets = 200;
    bool jitter_buffer_fast_accelerate = false;
    int jitter_buffer_min_delay_ms = 0;

    std::string sync_group;

    std::map<int, SdpAudioFormat> decoder_map;

    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory;

    absl::optional<AudioCodecPairId> codec_pair_id;

    webrtc::CryptoOptions crypto_options;

    rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor;

    rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer;
  };

  virtual void SetDecoderMap(std::map<int, SdpAudioFormat> decoder_map) = 0;
  virtual void SetNackHistory(int history_ms) = 0;
  virtual void SetNonSenderRttMeasurement(bool enabled) = 0;

  virtual bool IsRunning() const = 0;

  virtual Stats GetStats(bool get_and_clear_legacy_stats) const = 0;
  Stats GetStats() { return GetStats(                               true); }

  virtual void SetSink(AudioSinkInterface* sink) = 0;

  virtual void SetGain(float gain) = 0;

  virtual bool SetBaseMinimumPlayoutDelayMs(int delay_ms) = 0;

  virtual int GetBaseMinimumPlayoutDelayMs() const = 0;

  virtual uint32_t remote_ssrc() const = 0;

 protected:
  virtual ~AudioReceiveStreamInterface() {}
};

}

#endif
