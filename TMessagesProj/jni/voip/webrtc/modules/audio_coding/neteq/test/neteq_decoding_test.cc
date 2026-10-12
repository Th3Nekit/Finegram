/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/test/neteq_decoding_test.h"

#include "absl/strings/string_view.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/rtp_headers.h"
#include "modules/audio_coding/neteq/default_neteq_factory.h"
#include "modules/audio_coding/neteq/test/result_sink.h"
#include "rtc_base/strings/string_builder.h"
#include "test/testsupport/file_utils.h"

#ifdef WEBRTC_NETEQ_UNITTEST_BITEXACT

#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_coding/neteq/neteq_unittest.pb.h"
#else
#include "modules/audio_coding/neteq/neteq_unittest.pb.h"
#endif

#endif

namespace webrtc {

namespace {

void LoadDecoders(webrtc::NetEq* neteq) {
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(0, SdpAudioFormat("pcmu", 8000, 1)));
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(8, SdpAudioFormat("pcma", 8000, 1)));
#ifdef WEBRTC_CODEC_ILBC
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(102, SdpAudioFormat("ilbc", 8000, 1)));
#endif
#if defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(103, SdpAudioFormat("isac", 16000, 1)));
#endif
#ifdef WEBRTC_CODEC_ISAC
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(104, SdpAudioFormat("isac", 32000, 1)));
#endif
#ifdef WEBRTC_CODEC_OPUS
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(
                111, SdpAudioFormat("opus", 48000, 2, {{"stereo", "0"}})));
#endif
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(93, SdpAudioFormat("L16", 8000, 1)));
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(94, SdpAudioFormat("L16", 16000, 1)));
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(95, SdpAudioFormat("L16", 32000, 1)));
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(13, SdpAudioFormat("cn", 8000, 1)));
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(98, SdpAudioFormat("cn", 16000, 1)));
}

}

const int NetEqDecodingTest::kTimeStepMs;
const size_t NetEqDecodingTest::kBlockSize8kHz;
const size_t NetEqDecodingTest::kBlockSize16kHz;
const size_t NetEqDecodingTest::kBlockSize32kHz;
const int NetEqDecodingTest::kInitSampleRateHz;

NetEqDecodingTest::NetEqDecodingTest()
    : clock_(0),
      config_(),
      output_sample_rate_(kInitSampleRateHz),
      algorithmic_delay_ms_(0) {
  config_.sample_rate_hz = kInitSampleRateHz;
}

void NetEqDecodingTest::SetUp() {
  auto decoder_factory = CreateBuiltinAudioDecoderFactory();
  neteq_ = DefaultNetEqFactory().CreateNetEq(config_, decoder_factory, &clock_);
  NetEqNetworkStatistics stat;
  ASSERT_EQ(0, neteq_->NetworkStatistics(&stat));
  algorithmic_delay_ms_ = stat.current_buffer_size_ms;
  ASSERT_TRUE(neteq_);
  LoadDecoders(neteq_.get());
}

void NetEqDecodingTest::TearDown() {}

void NetEqDecodingTest::OpenInputFile(absl::string_view rtp_file) {
  rtp_source_.reset(test::RtpFileSource::Create(rtp_file));
}

void NetEqDecodingTest::Process() {

  while (packet_ && clock_.TimeInMilliseconds() >= packet_->time_ms()) {
    if (packet_->payload_length_bytes() > 0) {
#ifndef WEBRTC_CODEC_ISAC

      if (packet_->header().payloadType != 104)
#endif
        ASSERT_EQ(
            0, neteq_->InsertPacket(
                   packet_->header(),
                   rtc::ArrayView<const uint8_t>(
                       packet_->payload(), packet_->payload_length_bytes())));
    }

    packet_ = rtp_source_->NextPacket();
  }

  bool muted;
  ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
  ASSERT_FALSE(muted);
  ASSERT_TRUE((out_frame_.samples_per_channel_ == kBlockSize8kHz) ||
              (out_frame_.samples_per_channel_ == kBlockSize16kHz) ||
              (out_frame_.samples_per_channel_ == kBlockSize32kHz) ||
              (out_frame_.samples_per_channel_ == kBlockSize48kHz));
  output_sample_rate_ = out_frame_.sample_rate_hz_;
  EXPECT_EQ(output_sample_rate_, neteq_->last_output_sample_rate_hz());

  clock_.AdvanceTimeMilliseconds(kTimeStepMs);
}

void NetEqDecodingTest::DecodeAndCompare(
    absl::string_view rtp_file,
    absl::string_view output_checksum,
    absl::string_view network_stats_checksum,
    bool gen_ref) {
  OpenInputFile(rtp_file);

  std::string ref_out_file =
      gen_ref ? webrtc::test::OutputPath() + "neteq_universal_ref.pcm" : "";
  ResultSink output(ref_out_file);

  std::string stat_out_file =
      gen_ref ? webrtc::test::OutputPath() + "neteq_network_stats.dat" : "";
  ResultSink network_stats(stat_out_file);

  packet_ = rtp_source_->NextPacket();
  int i = 0;
  uint64_t last_concealed_samples = 0;
  uint64_t last_total_samples_received = 0;
  while (packet_) {
    rtc::StringBuilder ss;
    ss << "Lap number " << i++ << " in DecodeAndCompare while loop";
    SCOPED_TRACE(ss.str());
    ASSERT_NO_FATAL_FAILURE(Process());
    ASSERT_NO_FATAL_FAILURE(
        output.AddResult(out_frame_.data(), out_frame_.samples_per_channel_));

    if (clock_.TimeInMilliseconds() % 1000 == 0) {

      NetEqNetworkStatistics current_network_stats;
      ASSERT_EQ(0, neteq_->NetworkStatistics(&current_network_stats));
      ASSERT_NO_FATAL_FAILURE(network_stats.AddResult(current_network_stats));

      auto lifetime_stats = neteq_->GetLifetimeStatistics();
      const uint64_t delta_concealed_samples =
          lifetime_stats.concealed_samples - last_concealed_samples;
      last_concealed_samples = lifetime_stats.concealed_samples;
      const uint64_t delta_total_samples_received =
          lifetime_stats.total_samples_received - last_total_samples_received;
      last_total_samples_received = lifetime_stats.total_samples_received;

      EXPECT_NEAR(
          (delta_concealed_samples << 14) / delta_total_samples_received,
          current_network_stats.expand_rate, (2 << 14) / 100.0);
    }
  }

  SCOPED_TRACE("Check output audio.");
  output.VerifyChecksum(output_checksum);
  SCOPED_TRACE("Check network stats.");
  network_stats.VerifyChecksum(network_stats_checksum);
}

void NetEqDecodingTest::PopulateRtpInfo(int frame_index,
                                        int timestamp,
                                        RTPHeader* rtp_info) {
  rtp_info->sequenceNumber = frame_index;
  rtp_info->timestamp = timestamp;
  rtp_info->ssrc = 0x1234;
  rtp_info->payloadType = 94;
  rtp_info->markerBit = false;
}

void NetEqDecodingTest::PopulateCng(int frame_index,
                                    int timestamp,
                                    RTPHeader* rtp_info,
                                    uint8_t* payload,
                                    size_t* payload_len) {
  rtp_info->sequenceNumber = frame_index;
  rtp_info->timestamp = timestamp;
  rtp_info->ssrc = 0x1234;
  rtp_info->payloadType = 98;
  rtp_info->markerBit = false;
  payload[0] = 64;
  *payload_len = 1;
}

void NetEqDecodingTest::WrapTest(uint16_t start_seq_no,
                                 uint32_t start_timestamp,
                                 const std::set<uint16_t>& drop_seq_numbers,
                                 bool expect_seq_no_wrap,
                                 bool expect_timestamp_wrap) {
  uint16_t seq_no = start_seq_no;
  uint32_t timestamp = start_timestamp;
  const int kBlocksPerFrame = 3;
  const int kFrameSizeMs = kBlocksPerFrame * kTimeStepMs;
  const int kSamples = kBlockSize16kHz * kBlocksPerFrame;
  const size_t kPayloadBytes = kSamples * sizeof(int16_t);
  double next_input_time_ms = 0.0;

  const int kSpeechDurationMs = 2000;
  uint16_t last_seq_no;
  uint32_t last_timestamp;
  bool timestamp_wrapped = false;
  bool seq_no_wrapped = false;
  for (double t_ms = 0; t_ms < kSpeechDurationMs; t_ms += 10) {

    while (next_input_time_ms <= t_ms) {

      uint8_t payload[kPayloadBytes] = {0};
      RTPHeader rtp_info;
      PopulateRtpInfo(seq_no, timestamp, &rtp_info);
      if (drop_seq_numbers.find(seq_no) == drop_seq_numbers.end()) {

        ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload));
      }
      NetEqNetworkStatistics network_stats;
      ASSERT_EQ(0, neteq_->NetworkStatistics(&network_stats));

      EXPECT_LE(network_stats.preferred_buffer_size_ms, 80);
      EXPECT_LE(network_stats.current_buffer_size_ms,
                80 + algorithmic_delay_ms_);
      last_seq_no = seq_no;
      last_timestamp = timestamp;

      ++seq_no;
      timestamp += kSamples;
      next_input_time_ms += static_cast<double>(kFrameSizeMs);

      seq_no_wrapped |= seq_no < last_seq_no;
      timestamp_wrapped |= timestamp < last_timestamp;
    }

    AudioFrame output;
    bool muted;
    ASSERT_EQ(0, neteq_->GetAudio(&output, &muted));
    ASSERT_EQ(kBlockSize16kHz, output.samples_per_channel_);
    ASSERT_EQ(1u, output.num_channels_);

    absl::optional<uint32_t> playout_timestamp = neteq_->GetPlayoutTimestamp();
    ASSERT_TRUE(playout_timestamp);
    EXPECT_LE(timestamp - *playout_timestamp,
              static_cast<uint32_t>(kSamples * 2));
  }

  ASSERT_EQ(expect_seq_no_wrap, seq_no_wrapped);
  ASSERT_EQ(expect_timestamp_wrap, timestamp_wrapped);
}

void NetEqDecodingTest::LongCngWithClockDrift(double drift_factor,
                                              double network_freeze_ms,
                                              bool pull_audio_during_freeze,
                                              int delay_tolerance_ms,
                                              int max_time_to_speech_ms) {
  uint16_t seq_no = 0;
  uint32_t timestamp = 0;
  const int kFrameSizeMs = 30;
  const size_t kSamples = kFrameSizeMs * 16;
  const size_t kPayloadBytes = kSamples * 2;
  double next_input_time_ms = 0.0;
  double t_ms;
  bool muted;

  const int kSpeechDurationMs = 5000;
  for (t_ms = 0; t_ms < kSpeechDurationMs; t_ms += 10) {

    while (next_input_time_ms <= t_ms) {

      uint8_t payload[kPayloadBytes] = {0};
      RTPHeader rtp_info;
      PopulateRtpInfo(seq_no, timestamp, &rtp_info);
      ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload));
      ++seq_no;
      timestamp += kSamples;
      next_input_time_ms += static_cast<double>(kFrameSizeMs) * drift_factor;
    }

    ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  }

  EXPECT_EQ(AudioFrame::kNormalSpeech, out_frame_.speech_type_);
  absl::optional<uint32_t> playout_timestamp = neteq_->GetPlayoutTimestamp();
  ASSERT_TRUE(playout_timestamp);
  int32_t delay_before = timestamp - *playout_timestamp;

  const int kCngPeriodMs = 100;
  const int kCngPeriodSamples = kCngPeriodMs * 16;
  const int kCngDurationMs = 60000;
  for (; t_ms < kSpeechDurationMs + kCngDurationMs; t_ms += 10) {

    while (next_input_time_ms <= t_ms) {

      uint8_t payload[kPayloadBytes];
      size_t payload_len;
      RTPHeader rtp_info;
      PopulateCng(seq_no, timestamp, &rtp_info, payload, &payload_len);
      ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, rtc::ArrayView<const uint8_t>(
                                                      payload, payload_len)));
      ++seq_no;
      timestamp += kCngPeriodSamples;
      next_input_time_ms += static_cast<double>(kCngPeriodMs) * drift_factor;
    }

    ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  }

  EXPECT_EQ(AudioFrame::kCNG, out_frame_.speech_type_);

  if (network_freeze_ms > 0) {

    const double loop_end_time = t_ms + network_freeze_ms;
    for (; t_ms < loop_end_time; t_ms += 10) {

      ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
      ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
      EXPECT_EQ(AudioFrame::kCNG, out_frame_.speech_type_);
    }
    bool pull_once = pull_audio_during_freeze;

    double pull_time_ms = (t_ms + next_input_time_ms) / 2;
    while (next_input_time_ms <= t_ms) {
      if (pull_once && next_input_time_ms >= pull_time_ms) {
        pull_once = false;

        ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
        ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
        EXPECT_EQ(AudioFrame::kCNG, out_frame_.speech_type_);
        t_ms += 10;
      }

      uint8_t payload[kPayloadBytes];
      size_t payload_len;
      RTPHeader rtp_info;
      PopulateCng(seq_no, timestamp, &rtp_info, payload, &payload_len);
      ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, rtc::ArrayView<const uint8_t>(
                                                      payload, payload_len)));
      ++seq_no;
      timestamp += kCngPeriodSamples;
      next_input_time_ms += kCngPeriodMs * drift_factor;
    }
  }

  double speech_restart_time_ms = t_ms;
  while (out_frame_.speech_type_ != AudioFrame::kNormalSpeech) {

    while (next_input_time_ms <= t_ms) {

      uint8_t payload[kPayloadBytes] = {0};
      RTPHeader rtp_info;
      PopulateRtpInfo(seq_no, timestamp, &rtp_info);
      ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload));
      ++seq_no;
      timestamp += kSamples;
      next_input_time_ms += kFrameSizeMs * drift_factor;
    }

    ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);

    t_ms += 10;
  }

  double time_until_speech_returns_ms = t_ms - speech_restart_time_ms;
  EXPECT_LT(time_until_speech_returns_ms, max_time_to_speech_ms);
  playout_timestamp = neteq_->GetPlayoutTimestamp();
  ASSERT_TRUE(playout_timestamp);
  int32_t delay_after = timestamp - *playout_timestamp;

  EXPECT_LE(delay_after, delay_before + delay_tolerance_ms * 16);
  EXPECT_GE(delay_after, delay_before - delay_tolerance_ms * 16);
}

void NetEqDecodingTestTwoInstances::SetUp() {
  NetEqDecodingTest::SetUp();
  config2_ = config_;
}

void NetEqDecodingTestTwoInstances::CreateSecondInstance() {
  auto decoder_factory = CreateBuiltinAudioDecoderFactory();
  neteq2_ =
      DefaultNetEqFactory().CreateNetEq(config2_, decoder_factory, &clock_);
  ASSERT_TRUE(neteq2_);
  LoadDecoders(neteq2_.get());
}

}
