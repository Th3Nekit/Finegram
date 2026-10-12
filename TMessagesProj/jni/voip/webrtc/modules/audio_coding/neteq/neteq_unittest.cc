/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/neteq/neteq.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "api/audio/audio_frame.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "modules/audio_coding/codecs/pcm16b/pcm16b.h"
#include "modules/audio_coding/neteq/test/neteq_decoding_test.h"
#include "modules/audio_coding/neteq/tools/audio_loop.h"
#include "modules/audio_coding/neteq/tools/neteq_rtp_dump_input.h"
#include "modules/audio_coding/neteq/tools/neteq_test.h"
#include "modules/include/module_common_types_public.h"
#include "modules/rtp_rtcp/include/rtcp_statistics.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/message_digest.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/arch.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

ABSL_FLAG(bool, gen_ref, false, "Generate reference files.");

namespace webrtc {

#if defined(WEBRTC_LINUX) && defined(WEBRTC_ARCH_X86_64) &&         \
    defined(WEBRTC_NETEQ_UNITTEST_BITEXACT) &&                      \
    (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)) && \
    defined(WEBRTC_CODEC_ILBC)
#define MAYBE_TestBitExactness TestBitExactness
#else
#define MAYBE_TestBitExactness DISABLED_TestBitExactness
#endif
TEST_F(NetEqDecodingTest, MAYBE_TestBitExactness) {
  const std::string input_rtp_file =
      webrtc::test::ResourcePath("audio_coding/neteq_universal_new", "rtp");

  const std::string output_checksum =
      "dee7a10ab92526876a70a85bc48a4906901af3df";

  const std::string network_stats_checksum =
      "911dbf5fd97f48d25b8f0967286eb73c9d6f6158";

  DecodeAndCompare(input_rtp_file, output_checksum, network_stats_checksum,
                   absl::GetFlag(FLAGS_gen_ref));
}

#if defined(WEBRTC_LINUX) && defined(WEBRTC_ARCH_X86_64) && \
    defined(WEBRTC_NETEQ_UNITTEST_BITEXACT) && defined(WEBRTC_CODEC_OPUS)
#define MAYBE_TestOpusBitExactness TestOpusBitExactness
#else
#define MAYBE_TestOpusBitExactness DISABLED_TestOpusBitExactness
#endif
TEST_F(NetEqDecodingTest, MAYBE_TestOpusBitExactness) {
  const std::string input_rtp_file =
      webrtc::test::ResourcePath("audio_coding/neteq_opus", "rtp");

  const std::string output_checksum =
      "434bdc4ec08546510ee903d001c8be1a01c44e24|"
      "4336be0091e2faad7a194c16ee0a05e727325727|"
      "cefd2de4adfa8f6a9b66a3639ad63c2f6779d0cd";

  const std::string network_stats_checksum =
      "5f2c8e3dff9cff55dd7a9f4167939de001566d95|"
      "80ab17c17da030d4f2dfbf314ac44aacdadd7f0c";

  DecodeAndCompare(input_rtp_file, output_checksum, network_stats_checksum,
                   absl::GetFlag(FLAGS_gen_ref));
}

#if defined(WEBRTC_LINUX) && defined(WEBRTC_ARCH_X86_64) && \
    defined(WEBRTC_NETEQ_UNITTEST_BITEXACT) && defined(WEBRTC_CODEC_OPUS)
#define MAYBE_TestOpusDtxBitExactness TestOpusDtxBitExactness
#else
#define MAYBE_TestOpusDtxBitExactness DISABLED_TestOpusDtxBitExactness
#endif
TEST_F(NetEqDecodingTest, MAYBE_TestOpusDtxBitExactness) {
  const std::string input_rtp_file =
      webrtc::test::ResourcePath("audio_coding/neteq_opus_dtx", "rtp");

  const std::string output_checksum =
      "7eddce841cbfa500964c91cdae78b01b9f448948|"
      "5d13affec87bf4cc8c7667f0cd0d25e1ad09c7c3";

  const std::string network_stats_checksum =
      "92b0fdcbf8bb9354d40140b7312f2fb76a078555";

  DecodeAndCompare(input_rtp_file, output_checksum, network_stats_checksum,
                   absl::GetFlag(FLAGS_gen_ref));
}

class NetEqDecodingTestFaxMode : public NetEqDecodingTest {
 protected:
  NetEqDecodingTestFaxMode() : NetEqDecodingTest() {
    config_.for_test_no_time_stretching = true;
  }
  void TestJitterBufferDelay(bool apply_packet_loss);
};

TEST_F(NetEqDecodingTestFaxMode, TestFrameWaitingTimeStatistics) {

  size_t num_frames = 30;
  const size_t kSamples = 10 * 16;
  const size_t kPayloadBytes = kSamples * 2;
  for (size_t i = 0; i < num_frames; ++i) {
    const uint8_t payload[kPayloadBytes] = {0};
    RTPHeader rtp_info;
    rtp_info.sequenceNumber = rtc::checked_cast<uint16_t>(i);
    rtp_info.timestamp = rtc::checked_cast<uint32_t>(i * kSamples);
    rtp_info.ssrc = 0x1234;
    rtp_info.payloadType = 94;
    rtp_info.markerBit = 0;
    ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload));
  }

  for (size_t i = 0; i < num_frames; ++i) {
    bool muted;
    ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  }

  NetEqNetworkStatistics stats;
  EXPECT_EQ(0, neteq_->NetworkStatistics(&stats));

  EXPECT_EQ(155, stats.mean_waiting_time_ms);
  EXPECT_EQ(155, stats.median_waiting_time_ms);
  EXPECT_EQ(10, stats.min_waiting_time_ms);
  EXPECT_EQ(300, stats.max_waiting_time_ms);

  EXPECT_EQ(0, neteq_->NetworkStatistics(&stats));
  EXPECT_EQ(-1, stats.mean_waiting_time_ms);
  EXPECT_EQ(-1, stats.median_waiting_time_ms);
  EXPECT_EQ(-1, stats.min_waiting_time_ms);
  EXPECT_EQ(-1, stats.max_waiting_time_ms);
}

TEST_F(NetEqDecodingTest, LongCngWithNegativeClockDrift) {

  const double kDriftFactor = 1000.0 / (1000.0 + 25.0);
  const double kNetworkFreezeTimeMs = 0.0;
  const bool kGetAudioDuringFreezeRecovery = false;
  const int kDelayToleranceMs = 60;
  const int kMaxTimeToSpeechMs = 100;
  LongCngWithClockDrift(kDriftFactor, kNetworkFreezeTimeMs,
                        kGetAudioDuringFreezeRecovery, kDelayToleranceMs,
                        kMaxTimeToSpeechMs);
}

TEST_F(NetEqDecodingTest, LongCngWithPositiveClockDrift) {

  const double kDriftFactor = 1000.0 / (1000.0 - 25.0);
  const double kNetworkFreezeTimeMs = 0.0;
  const bool kGetAudioDuringFreezeRecovery = false;
  const int kDelayToleranceMs = 40;
  const int kMaxTimeToSpeechMs = 100;
  LongCngWithClockDrift(kDriftFactor, kNetworkFreezeTimeMs,
                        kGetAudioDuringFreezeRecovery, kDelayToleranceMs,
                        kMaxTimeToSpeechMs);
}

TEST_F(NetEqDecodingTest, LongCngWithNegativeClockDriftNetworkFreeze) {

  const double kDriftFactor = 1000.0 / (1000.0 + 25.0);
  const double kNetworkFreezeTimeMs = 5000.0;
  const bool kGetAudioDuringFreezeRecovery = false;
  const int kDelayToleranceMs = 60;
  const int kMaxTimeToSpeechMs = 200;
  LongCngWithClockDrift(kDriftFactor, kNetworkFreezeTimeMs,
                        kGetAudioDuringFreezeRecovery, kDelayToleranceMs,
                        kMaxTimeToSpeechMs);
}

TEST_F(NetEqDecodingTest, LongCngWithPositiveClockDriftNetworkFreeze) {

  const double kDriftFactor = 1000.0 / (1000.0 - 25.0);
  const double kNetworkFreezeTimeMs = 5000.0;
  const bool kGetAudioDuringFreezeRecovery = false;
  const int kDelayToleranceMs = 40;
  const int kMaxTimeToSpeechMs = 100;
  LongCngWithClockDrift(kDriftFactor, kNetworkFreezeTimeMs,
                        kGetAudioDuringFreezeRecovery, kDelayToleranceMs,
                        kMaxTimeToSpeechMs);
}

TEST_F(NetEqDecodingTest, LongCngWithPositiveClockDriftNetworkFreezeExtraPull) {

  const double kDriftFactor = 1000.0 / (1000.0 - 25.0);
  const double kNetworkFreezeTimeMs = 5000.0;
  const bool kGetAudioDuringFreezeRecovery = true;
  const int kDelayToleranceMs = 40;
  const int kMaxTimeToSpeechMs = 100;
  LongCngWithClockDrift(kDriftFactor, kNetworkFreezeTimeMs,
                        kGetAudioDuringFreezeRecovery, kDelayToleranceMs,
                        kMaxTimeToSpeechMs);
}

TEST_F(NetEqDecodingTest, LongCngWithoutClockDrift) {
  const double kDriftFactor = 1.0;
  const double kNetworkFreezeTimeMs = 0.0;
  const bool kGetAudioDuringFreezeRecovery = false;
  const int kDelayToleranceMs = 10;
  const int kMaxTimeToSpeechMs = 50;
  LongCngWithClockDrift(kDriftFactor, kNetworkFreezeTimeMs,
                        kGetAudioDuringFreezeRecovery, kDelayToleranceMs,
                        kMaxTimeToSpeechMs);
}

TEST_F(NetEqDecodingTest, UnknownPayloadType) {
  const size_t kPayloadBytes = 100;
  uint8_t payload[kPayloadBytes] = {0};
  RTPHeader rtp_info;
  PopulateRtpInfo(0, 0, &rtp_info);
  rtp_info.payloadType = 1;
  EXPECT_EQ(NetEq::kFail, neteq_->InsertPacket(rtp_info, payload));
}

#if defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)
#define MAYBE_DecoderError DecoderError
#else
#define MAYBE_DecoderError DISABLED_DecoderError
#endif

TEST_F(NetEqDecodingTest, MAYBE_DecoderError) {
  const size_t kPayloadBytes = 100;
  uint8_t payload[kPayloadBytes] = {0};
  RTPHeader rtp_info;
  PopulateRtpInfo(0, 0, &rtp_info);
  rtp_info.payloadType = 103;
  EXPECT_EQ(0, neteq_->InsertPacket(rtp_info, payload));

  int16_t* out_frame_data = out_frame_.mutable_data();
  for (size_t i = 0; i < AudioFrame::kMaxDataSizeSamples; ++i) {
    out_frame_data[i] = 1;
  }
  bool muted;
  EXPECT_EQ(NetEq::kFail, neteq_->GetAudio(&out_frame_, &muted));
  ASSERT_FALSE(muted);

  static const int kExpectedOutputLength = 160;
  const int16_t* const_out_frame_data = out_frame_.data();
  for (int i = 0; i < kExpectedOutputLength; ++i) {
    rtc::StringBuilder ss;
    ss << "i = " << i;
    SCOPED_TRACE(ss.str());
    EXPECT_EQ(0, const_out_frame_data[i]);
  }
}

TEST_F(NetEqDecodingTest, GetAudioBeforeInsertPacket) {

  int16_t* out_frame_data = out_frame_.mutable_data();
  for (size_t i = 0; i < AudioFrame::kMaxDataSizeSamples; ++i) {
    out_frame_data[i] = 1;
  }
  bool muted;
  EXPECT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
  ASSERT_FALSE(muted);

  static const int kExpectedOutputLength =
      kInitSampleRateHz / 100;
  const int16_t* const_out_frame_data = out_frame_.data();
  for (int i = 0; i < kExpectedOutputLength; ++i) {
    rtc::StringBuilder ss;
    ss << "i = " << i;
    SCOPED_TRACE(ss.str());
    EXPECT_EQ(0, const_out_frame_data[i]);
  }

  EXPECT_EQ(config_.sample_rate_hz, neteq_->last_output_sample_rate_hz());
}

class NetEqBgnTest : public NetEqDecodingTest {
 protected:
  void CheckBgn(int sampling_rate_hz) {
    size_t expected_samples_per_channel = 0;
    uint8_t payload_type = 0xFF;
    if (sampling_rate_hz == 8000) {
      expected_samples_per_channel = kBlockSize8kHz;
      payload_type = 93;
    } else if (sampling_rate_hz == 16000) {
      expected_samples_per_channel = kBlockSize16kHz;
      payload_type = 94;
    } else if (sampling_rate_hz == 32000) {
      expected_samples_per_channel = kBlockSize32kHz;
      payload_type = 95;
    } else {
      ASSERT_TRUE(false);
    }

    AudioFrame output;
    test::AudioLoop input;

    ASSERT_TRUE(input.Init(
        webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm"),
        10 * sampling_rate_hz,
        expected_samples_per_channel));

    uint8_t payload[kBlockSize32kHz * sizeof(int16_t)];
    RTPHeader rtp_info;
    PopulateRtpInfo(0, 0, &rtp_info);
    rtp_info.payloadType = payload_type;

    bool muted;
    for (int n = 0; n < 10; ++n) {
      auto block = input.GetNextBlock();
      ASSERT_EQ(expected_samples_per_channel, block.size());
      size_t enc_len_bytes =
          WebRtcPcm16b_Encode(block.data(), block.size(), payload);
      ASSERT_EQ(enc_len_bytes, expected_samples_per_channel * 2);

      ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, rtc::ArrayView<const uint8_t>(
                                                      payload, enc_len_bytes)));
      output.Reset();
      ASSERT_EQ(0, neteq_->GetAudio(&output, &muted));
      ASSERT_EQ(1u, output.num_channels_);
      ASSERT_EQ(expected_samples_per_channel, output.samples_per_channel_);
      ASSERT_EQ(AudioFrame::kNormalSpeech, output.speech_type_);

      rtp_info.timestamp +=
          rtc::checked_cast<uint32_t>(expected_samples_per_channel);
      rtp_info.sequenceNumber++;
    }

    output.Reset();

    ASSERT_EQ(0, neteq_->GetAudio(&output, &muted));
    ASSERT_EQ(1u, output.num_channels_);
    ASSERT_EQ(expected_samples_per_channel, output.samples_per_channel_);

    const int kFadingThreshold = 611;

    const int kNumPlcToCngTestFrames = 20;
    bool plc_to_cng = false;
    for (int n = 0; n < kFadingThreshold + kNumPlcToCngTestFrames; ++n) {
      output.Reset();

      memset(output.mutable_data(), 1, AudioFrame::kMaxDataSizeBytes);
      ASSERT_EQ(0, neteq_->GetAudio(&output, &muted));
      ASSERT_FALSE(muted);
      ASSERT_EQ(1u, output.num_channels_);
      ASSERT_EQ(expected_samples_per_channel, output.samples_per_channel_);
      if (output.speech_type_ == AudioFrame::kPLCCNG) {
        plc_to_cng = true;
        double sum_squared = 0;
        const int16_t* output_data = output.data();
        for (size_t k = 0;
             k < output.num_channels_ * output.samples_per_channel_; ++k)
          sum_squared += output_data[k] * output_data[k];
        EXPECT_EQ(0, sum_squared);
      } else {
        EXPECT_EQ(AudioFrame::kPLC, output.speech_type_);
      }
    }
    EXPECT_TRUE(plc_to_cng);
  }
};

TEST_F(NetEqBgnTest, RunTest) {
  CheckBgn(8000);
  CheckBgn(16000);
  CheckBgn(32000);
}

TEST_F(NetEqDecodingTest, SequenceNumberWrap) {

  std::set<uint16_t> drop_seq_numbers;
  WrapTest(0xFFFF - 10, 0, drop_seq_numbers, true, false);
}

TEST_F(NetEqDecodingTest, SequenceNumberWrapAndDrop) {

  std::set<uint16_t> drop_seq_numbers;
  drop_seq_numbers.insert(0xFFFF);
  drop_seq_numbers.insert(0x0);
  WrapTest(0xFFFF - 10, 0, drop_seq_numbers, true, false);
}

TEST_F(NetEqDecodingTest, TimestampWrap) {

  std::set<uint16_t> drop_seq_numbers;
  WrapTest(0, 0xFFFFFFFF - 3000, drop_seq_numbers, false, true);
}

TEST_F(NetEqDecodingTest, TimestampAndSequenceNumberWrap) {

  std::set<uint16_t> drop_seq_numbers;
  WrapTest(0xFFFF - 10, 0xFFFFFFFF - 5000, drop_seq_numbers, true, true);
}

TEST_F(NetEqDecodingTest, DiscardDuplicateCng) {
  uint16_t seq_no = 0;
  uint32_t timestamp = 0;
  const int kFrameSizeMs = 10;
  const int kSampleRateKhz = 16;
  const int kSamples = kFrameSizeMs * kSampleRateKhz;
  const size_t kPayloadBytes = kSamples * 2;

  const int algorithmic_delay_samples =
      std::max(algorithmic_delay_ms_ * kSampleRateKhz, 5 * kSampleRateKhz / 8);

  uint8_t payload[kPayloadBytes] = {0};
  RTPHeader rtp_info;
  bool muted;
  for (int i = 0; i < 3; ++i) {
    PopulateRtpInfo(seq_no, timestamp, &rtp_info);
    ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload));
    ++seq_no;
    timestamp += kSamples;

    ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  }

  EXPECT_EQ(AudioFrame::kNormalSpeech, out_frame_.speech_type_);

  const int kCngPeriodMs = 100;
  const int kCngPeriodSamples = kCngPeriodMs * kSampleRateKhz;
  size_t payload_len;
  PopulateCng(seq_no, timestamp, &rtp_info, payload, &payload_len);

  ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, rtc::ArrayView<const uint8_t>(
                                                  payload, payload_len)));

  ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
  ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  EXPECT_EQ(AudioFrame::kCNG, out_frame_.speech_type_);
  EXPECT_FALSE(
      neteq_->GetPlayoutTimestamp());
  EXPECT_EQ(timestamp - algorithmic_delay_samples,
            out_frame_.timestamp_ + out_frame_.samples_per_channel_);

  ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, rtc::ArrayView<const uint8_t>(
                                                  payload, payload_len)));

  for (int cng_time_ms = 10; cng_time_ms < kCngPeriodMs; cng_time_ms += 10) {
    ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
    EXPECT_EQ(AudioFrame::kCNG, out_frame_.speech_type_);
    EXPECT_FALSE(
        neteq_->GetPlayoutTimestamp());
    EXPECT_EQ(timestamp - algorithmic_delay_samples,
              out_frame_.timestamp_ + out_frame_.samples_per_channel_);
  }

  ++seq_no;
  timestamp += kCngPeriodSamples;
  uint32_t first_speech_timestamp = timestamp;

  for (int i = 0; i < 4; ++i) {
    PopulateRtpInfo(seq_no, timestamp, &rtp_info);
    ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload));
    ++seq_no;
    timestamp += kSamples;
  }

  ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
  ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  EXPECT_EQ(AudioFrame::kNormalSpeech, out_frame_.speech_type_);
  absl::optional<uint32_t> playout_timestamp = neteq_->GetPlayoutTimestamp();
  ASSERT_TRUE(playout_timestamp);
  EXPECT_EQ(first_speech_timestamp + kSamples - algorithmic_delay_samples,
            *playout_timestamp);
}

TEST_F(NetEqDecodingTest, CngFirst) {
  uint16_t seq_no = 0;
  uint32_t timestamp = 0;
  const int kFrameSizeMs = 10;
  const int kSampleRateKhz = 16;
  const int kSamples = kFrameSizeMs * kSampleRateKhz;
  const int kPayloadBytes = kSamples * 2;
  const int kCngPeriodMs = 100;
  const int kCngPeriodSamples = kCngPeriodMs * kSampleRateKhz;
  size_t payload_len;

  uint8_t payload[kPayloadBytes] = {0};
  RTPHeader rtp_info;

  PopulateCng(seq_no, timestamp, &rtp_info, payload, &payload_len);
  ASSERT_EQ(NetEq::kOK,
            neteq_->InsertPacket(
                rtp_info, rtc::ArrayView<const uint8_t>(payload, payload_len)));
  ++seq_no;
  timestamp += kCngPeriodSamples;

  bool muted;
  ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
  ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  EXPECT_EQ(AudioFrame::kCNG, out_frame_.speech_type_);

  const uint32_t first_speech_timestamp = timestamp;
  int timeout_counter = 0;
  do {
    ASSERT_LT(timeout_counter++, 20) << "Test timed out";
    PopulateRtpInfo(seq_no, timestamp, &rtp_info);
    ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload));
    ++seq_no;
    timestamp += kSamples;

    ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  } while (!IsNewerTimestamp(out_frame_.timestamp_, first_speech_timestamp));

  EXPECT_EQ(AudioFrame::kNormalSpeech, out_frame_.speech_type_);
}

class NetEqDecodingTestWithMutedState : public NetEqDecodingTest {
 public:
  NetEqDecodingTestWithMutedState() : NetEqDecodingTest() {
    config_.enable_muted_state = true;
  }

 protected:
  static constexpr size_t kSamples = 10 * 16;
  static constexpr size_t kPayloadBytes = kSamples * 2;

  void InsertPacket(uint32_t rtp_timestamp) {
    uint8_t payload[kPayloadBytes] = {0};
    RTPHeader rtp_info;
    PopulateRtpInfo(0, rtp_timestamp, &rtp_info);
    EXPECT_EQ(0, neteq_->InsertPacket(rtp_info, payload));
  }

  void InsertCngPacket(uint32_t rtp_timestamp) {
    uint8_t payload[kPayloadBytes] = {0};
    RTPHeader rtp_info;
    size_t payload_len;
    PopulateCng(0, rtp_timestamp, &rtp_info, payload, &payload_len);
    EXPECT_EQ(NetEq::kOK,
              neteq_->InsertPacket(rtp_info, rtc::ArrayView<const uint8_t>(
                                                 payload, payload_len)));
  }

  bool GetAudioReturnMuted() {
    bool muted;
    EXPECT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    return muted;
  }

  void GetAudioUntilMuted() {
    while (!GetAudioReturnMuted()) {
      ASSERT_LT(counter_++, 1000) << "Test timed out";
    }
  }

  void GetAudioUntilNormal() {
    bool muted = false;
    while (out_frame_.speech_type_ != AudioFrame::kNormalSpeech) {
      EXPECT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
      ASSERT_LT(counter_++, 1000) << "Test timed out";
    }
    EXPECT_FALSE(muted);
  }

  int counter_ = 0;
};

TEST_F(NetEqDecodingTestWithMutedState, MutedState) {

  InsertPacket(0);

  EXPECT_FALSE(GetAudioReturnMuted());

  GetAudioUntilMuted();
  EXPECT_TRUE(out_frame_.muted());

  AudioFrame new_frame;
  int16_t* frame_data = new_frame.mutable_data();
  for (size_t i = 0; i < AudioFrame::kMaxDataSizeSamples; i++) {
    frame_data[i] = 17;
  }
  bool muted;
  EXPECT_EQ(0, neteq_->GetAudio(&new_frame, &muted));
  EXPECT_TRUE(muted);
  EXPECT_TRUE(out_frame_.muted());
  for (size_t i = 0; i < AudioFrame::kMaxDataSizeSamples; i++) {
    EXPECT_EQ(17, frame_data[i]);
  }
  EXPECT_EQ(out_frame_.timestamp_ + out_frame_.samples_per_channel_,
            new_frame.timestamp_);
  EXPECT_EQ(out_frame_.samples_per_channel_, new_frame.samples_per_channel_);
  EXPECT_EQ(out_frame_.sample_rate_hz_, new_frame.sample_rate_hz_);
  EXPECT_EQ(out_frame_.num_channels_, new_frame.num_channels_);
  EXPECT_EQ(out_frame_.speech_type_, new_frame.speech_type_);
  EXPECT_EQ(out_frame_.vad_activity_, new_frame.vad_activity_);

  InsertPacket(kSamples * counter_);
  GetAudioUntilNormal();
  EXPECT_FALSE(out_frame_.muted());

  NetEqNetworkStatistics stats;
  EXPECT_EQ(0, neteq_->NetworkStatistics(&stats));

  EXPECT_GT(stats.expand_rate, 14000);

  EXPECT_GT(stats.expand_rate, stats.speech_expand_rate);
}

TEST_F(NetEqDecodingTestWithMutedState, MutedStateDelayedPacket) {

  InsertPacket(0);

  EXPECT_FALSE(GetAudioReturnMuted());

  GetAudioUntilMuted();

  InsertPacket(kSamples * counter_ / 2);
  GetAudioUntilNormal();
}

TEST_F(NetEqDecodingTestWithMutedState, MutedStateFuturePacket) {

  InsertPacket(0);

  EXPECT_FALSE(GetAudioReturnMuted());

  GetAudioUntilMuted();

  InsertPacket(kSamples * counter_ * 2);
  GetAudioUntilNormal();
}

TEST_F(NetEqDecodingTestWithMutedState, MutedStateOldPacket) {

  InsertPacket(0);

  EXPECT_FALSE(GetAudioReturnMuted());

  GetAudioUntilMuted();

  EXPECT_NE(AudioFrame::kNormalSpeech, out_frame_.speech_type_);

  for (int i = 0; i < 5; ++i) {
    InsertPacket(kSamples * (i - 1000));
  }
  GetAudioUntilNormal();
}

TEST_F(NetEqDecodingTestWithMutedState, DoNotMuteExtendedCngWithoutPackets) {

  InsertCngPacket(0);

  for (int i = 0; i < 1000; ++i) {
    bool muted;
    EXPECT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_FALSE(muted);
  }
  EXPECT_EQ(AudioFrame::kCNG, out_frame_.speech_type_);
}

TEST_F(NetEqDecodingTestWithMutedState, RecoverAfterExtendedCngWithoutPackets) {

  InsertCngPacket(0);

  for (int i = 0; i < 1000; ++i) {
    bool muted;
    EXPECT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
  }

  InsertPacket(kSamples * counter_);
  GetAudioUntilNormal();
}

namespace {
::testing::AssertionResult AudioFramesEqualExceptData(const AudioFrame& a,
                                                      const AudioFrame& b) {
  if (a.timestamp_ != b.timestamp_)
    return ::testing::AssertionFailure() << "timestamp_ diff (" << a.timestamp_
                                         << " != " << b.timestamp_ << ")";
  if (a.sample_rate_hz_ != b.sample_rate_hz_)
    return ::testing::AssertionFailure()
           << "sample_rate_hz_ diff (" << a.sample_rate_hz_
           << " != " << b.sample_rate_hz_ << ")";
  if (a.samples_per_channel_ != b.samples_per_channel_)
    return ::testing::AssertionFailure()
           << "samples_per_channel_ diff (" << a.samples_per_channel_
           << " != " << b.samples_per_channel_ << ")";
  if (a.num_channels_ != b.num_channels_)
    return ::testing::AssertionFailure()
           << "num_channels_ diff (" << a.num_channels_
           << " != " << b.num_channels_ << ")";
  if (a.speech_type_ != b.speech_type_)
    return ::testing::AssertionFailure()
           << "speech_type_ diff (" << a.speech_type_
           << " != " << b.speech_type_ << ")";
  if (a.vad_activity_ != b.vad_activity_)
    return ::testing::AssertionFailure()
           << "vad_activity_ diff (" << a.vad_activity_
           << " != " << b.vad_activity_ << ")";
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult AudioFramesEqual(const AudioFrame& a,
                                            const AudioFrame& b) {
  ::testing::AssertionResult res = AudioFramesEqualExceptData(a, b);
  if (!res)
    return res;
  if (memcmp(a.data(), b.data(),
             a.samples_per_channel_ * a.num_channels_ * sizeof(*a.data())) !=
      0) {
    return ::testing::AssertionFailure() << "data_ diff";
  }
  return ::testing::AssertionSuccess();
}

}

TEST_F(NetEqDecodingTestTwoInstances, CompareMutedStateOnOff) {
  ASSERT_FALSE(config_.enable_muted_state);
  config2_.enable_muted_state = true;
  CreateSecondInstance();

  const size_t kSamples = 10 * 16;
  const size_t kPayloadBytes = kSamples * 2;
  uint8_t payload[kPayloadBytes] = {0};
  RTPHeader rtp_info;
  PopulateRtpInfo(0, 0, &rtp_info);
  EXPECT_EQ(0, neteq_->InsertPacket(rtp_info, payload));
  EXPECT_EQ(0, neteq2_->InsertPacket(rtp_info, payload));

  AudioFrame out_frame1, out_frame2;
  bool muted;
  for (int i = 0; i < 1000; ++i) {
    rtc::StringBuilder ss;
    ss << "i = " << i;
    SCOPED_TRACE(ss.str());
    EXPECT_EQ(0, neteq_->GetAudio(&out_frame1, &muted));
    EXPECT_FALSE(muted);
    EXPECT_EQ(0, neteq2_->GetAudio(&out_frame2, &muted));
    if (muted) {
      EXPECT_TRUE(AudioFramesEqualExceptData(out_frame1, out_frame2));
    } else {
      EXPECT_TRUE(AudioFramesEqual(out_frame1, out_frame2));
    }
  }
  EXPECT_TRUE(muted);

  for (int i = 0; i < 5; ++i) {
    PopulateRtpInfo(0, kSamples * 1000 + kSamples * i, &rtp_info);
    EXPECT_EQ(0, neteq_->InsertPacket(rtp_info, payload));
    EXPECT_EQ(0, neteq2_->InsertPacket(rtp_info, payload));
  }

  int counter = 0;
  while (out_frame1.speech_type_ != AudioFrame::kNormalSpeech) {
    ASSERT_LT(counter++, 1000) << "Test timed out";
    rtc::StringBuilder ss;
    ss << "counter = " << counter;
    SCOPED_TRACE(ss.str());
    EXPECT_EQ(0, neteq_->GetAudio(&out_frame1, &muted));
    EXPECT_FALSE(muted);
    EXPECT_EQ(0, neteq2_->GetAudio(&out_frame2, &muted));
    if (muted) {
      EXPECT_TRUE(AudioFramesEqualExceptData(out_frame1, out_frame2));
    } else {
      EXPECT_TRUE(AudioFramesEqual(out_frame1, out_frame2));
    }
  }
  EXPECT_FALSE(muted);
}

TEST_F(NetEqDecodingTest, TestConcealmentEvents) {
  const int kNumConcealmentEvents = 19;
  const size_t kSamples = 10 * 16;
  const size_t kPayloadBytes = kSamples * 2;
  int seq_no = 0;
  RTPHeader rtp_info;
  rtp_info.ssrc = 0x1234;
  rtp_info.payloadType = 94;
  rtp_info.markerBit = 0;
  const uint8_t payload[kPayloadBytes] = {0};
  bool muted;

  for (int i = 0; i < kNumConcealmentEvents; i++) {

    for (int j = 0; j < 10; j++) {
      rtp_info.sequenceNumber = seq_no++;
      rtp_info.timestamp = rtp_info.sequenceNumber * kSamples;
      neteq_->InsertPacket(rtp_info, payload);
      neteq_->GetAudio(&out_frame_, &muted);
    }

    int num_lost = 1 + i;
    for (int j = 0; j < num_lost; j++) {
      seq_no++;
      neteq_->GetAudio(&out_frame_, &muted);
    }
  }

  NetEqLifetimeStatistics stats = neteq_->GetLifetimeStatistics();
  EXPECT_EQ(kNumConcealmentEvents, static_cast<int>(stats.concealment_events));
}

void NetEqDecodingTestFaxMode::TestJitterBufferDelay(bool apply_packet_loss) {
  const int kNumPackets = 10;
  const int kDelayInNumPackets = 2;
  const int kPacketLenMs = 10;
  const size_t kSamples = kPacketLenMs * 16;
  const size_t kPayloadBytes = kSamples * 2;
  RTPHeader rtp_info;
  rtp_info.ssrc = 0x1234;
  rtp_info.payloadType = 94;
  rtp_info.markerBit = 0;
  const uint8_t payload[kPayloadBytes] = {0};
  bool muted;
  int packets_sent = 0;
  int packets_received = 0;
  int expected_delay = 0;
  int expected_target_delay = 0;
  uint64_t expected_emitted_count = 0;
  while (packets_received < kNumPackets) {

    if (packets_sent < kNumPackets) {
      rtp_info.sequenceNumber = packets_sent++;
      rtp_info.timestamp = rtp_info.sequenceNumber * kSamples;
      neteq_->InsertPacket(rtp_info, payload);
    }

    if (packets_sent > kDelayInNumPackets) {
      neteq_->GetAudio(&out_frame_, &muted);
      packets_received++;

      int packets_delay = std::min(packets_received, kDelayInNumPackets + 1);

      int current_delay_ms = packets_delay * kPacketLenMs;
      expected_delay += current_delay_ms * kSamples;
      expected_target_delay += neteq_->TargetDelayMs() * kSamples;
      expected_emitted_count += kSamples;
    }
  }

  if (apply_packet_loss) {

    neteq_->GetAudio(&out_frame_, &muted);
  }

  NetEqLifetimeStatistics stats = neteq_->GetLifetimeStatistics();
  EXPECT_EQ(expected_delay,
            rtc::checked_cast<int>(stats.jitter_buffer_delay_ms));
  EXPECT_EQ(expected_emitted_count, stats.jitter_buffer_emitted_count);
  EXPECT_EQ(expected_target_delay,
            rtc::checked_cast<int>(stats.jitter_buffer_target_delay_ms));
}

TEST_F(NetEqDecodingTestFaxMode, TestJitterBufferDelayWithoutLoss) {
  TestJitterBufferDelay(false);
}

TEST_F(NetEqDecodingTestFaxMode, TestJitterBufferDelayWithLoss) {
  TestJitterBufferDelay(true);
}

TEST_F(NetEqDecodingTestFaxMode, TestJitterBufferDelayWithAcceleration) {
  const int kPacketLenMs = 10;
  const size_t kSamples = kPacketLenMs * 16;
  const size_t kPayloadBytes = kSamples * 2;
  RTPHeader rtp_info;
  rtp_info.ssrc = 0x1234;
  rtp_info.payloadType = 94;
  rtp_info.markerBit = 0;
  const uint8_t payload[kPayloadBytes] = {0};

  int expected_target_delay = neteq_->TargetDelayMs() * kSamples;
  neteq_->InsertPacket(rtp_info, payload);

  bool muted;
  neteq_->GetAudio(&out_frame_, &muted);

  rtp_info.sequenceNumber += 1;
  rtp_info.timestamp += kSamples;
  neteq_->InsertPacket(rtp_info, payload);
  rtp_info.sequenceNumber += 1;
  rtp_info.timestamp += kSamples;
  neteq_->InsertPacket(rtp_info, payload);

  expected_target_delay += neteq_->TargetDelayMs() * 2 * kSamples;

  neteq_->GetAudio(&out_frame_, &muted, nullptr, NetEq::Operation::kAccelerate);

  NetEqLifetimeStatistics stats = neteq_->GetLifetimeStatistics();
  EXPECT_EQ(10 * kSamples * 3, stats.jitter_buffer_delay_ms);
  EXPECT_EQ(kSamples * 3, stats.jitter_buffer_emitted_count);
  EXPECT_EQ(expected_target_delay,
            rtc::checked_cast<int>(stats.jitter_buffer_target_delay_ms));
}

namespace test {
TEST(NetEqNoTimeStretchingMode, RunTest) {
  NetEq::Config config;
  config.for_test_no_time_stretching = true;
  auto codecs = NetEqTest::StandardDecoderMap();
  std::map<int, RTPExtensionType> rtp_ext_map = {
      {1, kRtpExtensionAudioLevel},
      {3, kRtpExtensionAbsoluteSendTime},
      {5, kRtpExtensionTransportSequenceNumber},
      {7, kRtpExtensionVideoContentType},
      {8, kRtpExtensionVideoTiming}};
  std::unique_ptr<NetEqInput> input = CreateNetEqRtpDumpInput(
      webrtc::test::ResourcePath("audio_coding/neteq_universal_new", "rtp"),
      rtp_ext_map, absl::nullopt                   );
  std::unique_ptr<TimeLimitedNetEqInput> input_time_limit(
      new TimeLimitedNetEqInput(std::move(input), 20000));
  std::unique_ptr<AudioSink> output(new VoidAudioSink);
  NetEqTest::Callbacks callbacks;
  NetEqTest test(config, CreateBuiltinAudioDecoderFactory(), codecs,
                              nullptr,                   nullptr,
                           std::move(input_time_limit), std::move(output),
                 callbacks);
  test.Run();
  const auto stats = test.SimulationStats();
  EXPECT_EQ(0, stats.accelerate_rate);
  EXPECT_EQ(0, stats.preemptive_rate);
}

}
}
