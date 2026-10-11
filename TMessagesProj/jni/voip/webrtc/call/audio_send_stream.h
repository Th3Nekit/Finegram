/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_AUDIO_SEND_STREAM_H_
#define CALL_AUDIO_SEND_STREAM_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio_codecs/audio_codec_pair_id.h"
#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/audio_format.h"
#include "api/call/transport.h"
#include "api/crypto/crypto_options.h"
#include "api/crypto/frame_encryptor_interface.h"
#include "api/frame_transformer_interface.h"
#include "api/rtp_parameters.h"
#include "api/rtp_sender_interface.h"
#include "api/scoped_refptr.h"
#include "call/audio_sender.h"
#include "call/rtp_config.h"
#include "modules/audio_processing/include/audio_processing_statistics.h"
#include "modules/rtp_rtcp/include/report_block_data.h"

namespace webrtc {

class AudioSendStream : public AudioSender {
 public:
  struct Stats {
    Stats();
    ~Stats();

    uint32_t local_ssrc = 0;
    int64_t payload_bytes_sent = 0;
    int64_t header_and_padding_bytes_sent = 0;

    uint64_t retransmitted_bytes_sent = 0;
    int32_t packets_sent = 0;

    TimeDelta total_packet_send_delay = TimeDelta::Zero();

    uint64_t retransmitted_packets_sent = 0;
    int32_t packets_lost = -1;
    float fraction_lost = -1.0f;
    std::string codec_name;
    absl::optional<int> codec_payload_type;
    int32_t jitter_ms = -1;
    int64_t rtt_ms = -1;
    int16_t audio_level = 0;

    double total_input_energy = 0.0;
    double total_input_duration = 0.0;

    ANAStats ana_statistics;
    AudioProcessingStats apm_statistics;

    int64_t target_bitrate_bps = 0;

    std::vector<ReportBlockData> report_block_datas;
    uint32_t nacks_received = 0;
  };

  struct Config {
    Config() = delete;
    explicit Config(Transport* send_transport);
    ~Config();
    std::string ToString() const;

    struct Rtp {
      Rtp();
      ~Rtp();
      std::string ToString() const;

      uint32_t ssrc = 0;

      std::string rid;

      std::string mid;

      bool extmap_allow_mixed = false;

      std::vector<RtpExtension> extensions;

      std::string c_name;
    } rtp;

    int rtcp_report_interval_ms = 5000;

    Transport* send_transport = nullptr;

    int min_bitrate_bps = -1;
    int max_bitrate_bps = -1;

    double bitrate_priority = 1.0;
    bool has_dscp = false;

    absl::optional<std::string> audio_network_adaptor_config;

    struct SendCodecSpec {
      SendCodecSpec(int payload_type, const SdpAudioFormat& format);
      ~SendCodecSpec();
      std::string ToString() const;

      bool operator==(const SendCodecSpec& rhs) const;
      bool operator!=(const SendCodecSpec& rhs) const {
        return !(*this == rhs);
      }

      int payload_type;
      SdpAudioFormat format;
      bool nack_enabled = false;
      bool transport_cc_enabled = false;
      bool enable_non_sender_rtt = false;
      absl::optional<int> cng_payload_type;
      absl::optional<int> red_payload_type;

      absl::optional<int> target_bitrate_bps;
    };

    absl::optional<SendCodecSpec> send_codec_spec;
    rtc::scoped_refptr<AudioEncoderFactory> encoder_factory;
    absl::optional<AudioCodecPairId> codec_pair_id;

    std::string track_id;

    webrtc::CryptoOptions crypto_options;

    rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor;

    rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer;
  };

  virtual ~AudioSendStream() = default;

  virtual const webrtc::AudioSendStream::Config& GetConfig() const = 0;

  virtual void Reconfigure(const Config& config,
                           SetParametersCallback callback) = 0;

  virtual void Start() = 0;

  virtual void Stop() = 0;

  virtual bool SendTelephoneEvent(int payload_type,
                                  int payload_frequency,
                                  int event,
                                  int duration_ms) = 0;

  virtual void SetMuted(bool muted) = 0;

  virtual Stats GetStats() const = 0;
  virtual Stats GetStats(bool has_remote_tracks) const = 0;
};

}

#endif
