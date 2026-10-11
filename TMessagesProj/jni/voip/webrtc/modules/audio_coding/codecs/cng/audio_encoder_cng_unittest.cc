/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/cng/audio_encoder_cng.h"

#include <memory>
#include <vector>

#include "common_audio/vad/mock/mock_vad.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "test/gtest.h"
#include "test/mock_audio_encoder.h"
#include "test/testsupport/rtc_expect_death.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace webrtc {

namespace {
static const size_t kMaxNumSamples = 48 * 10 * 2;
static const size_t kMockReturnEncodedBytes = 17;
static const int kCngPayloadType = 18;
}

class AudioEncoderCngTest : public ::testing::Test {
 protected:
  AudioEncoderCngTest()
      : mock_encoder_owner_(new MockAudioEncoder),
        mock_encoder_(mock_encoder_owner_.get()),
        mock_vad_(new MockVad),
        timestamp_(4711),
        num_audio_samples_10ms_(0),
        sample_rate_hz_(8000) {
    memset(audio_, 0, kMaxNumSamples * 2);
    EXPECT_CALL(*mock_encoder_, NumChannels()).WillRepeatedly(Return(1));
  }

  AudioEncoderCngTest(const AudioEncoderCngTest&) = delete;
  AudioEncoderCngTest& operator=(const AudioEncoderCngTest&) = delete;

  void TearDown() override {
    EXPECT_CALL(*mock_vad_, Die()).Times(1);
    cng_.reset();
  }

  AudioEncoderCngConfig MakeCngConfig() {
    AudioEncoderCngConfig config;
    config.speech_encoder = std::move(mock_encoder_owner_);
    EXPECT_TRUE(config.speech_encoder);

    config.vad = mock_vad_;
    config.payload_type = kCngPayloadType;

    return config;
  }

  void CreateCng(AudioEncoderCngConfig&& config) {
    num_audio_samples_10ms_ = static_cast<size_t>(10 * sample_rate_hz_ / 1000);
    ASSERT_LE(num_audio_samples_10ms_, kMaxNumSamples);
    if (config.speech_encoder) {
      EXPECT_CALL(*mock_encoder_, SampleRateHz())
          .WillRepeatedly(Return(sample_rate_hz_));

      EXPECT_CALL(*mock_encoder_, Max10MsFramesInAPacket())
          .WillOnce(Return(1u));
    }
    cng_ = CreateComfortNoiseEncoder(std::move(config));
  }

  void Encode() {
    ASSERT_TRUE(cng_) << "Must call CreateCng() first.";
    encoded_info_ = cng_->Encode(
        timestamp_,
        rtc::ArrayView<const int16_t>(audio_, num_audio_samples_10ms_),
        &encoded_);
    timestamp_ += static_cast<uint32_t>(num_audio_samples_10ms_);
  }

  void ExpectEncodeCalls(size_t num_calls) {
    InSequence s;
    AudioEncoder::EncodedInfo info;
    for (size_t j = 0; j < num_calls - 1; ++j) {
      EXPECT_CALL(*mock_encoder_, EncodeImpl(_, _, _)).WillOnce(Return(info));
    }
    info.encoded_bytes = kMockReturnEncodedBytes;
    EXPECT_CALL(*mock_encoder_, EncodeImpl(_, _, _))
        .WillOnce(
            Invoke(MockAudioEncoder::FakeEncoding(kMockReturnEncodedBytes)));
  }

  void CheckBlockGrouping(size_t blocks_per_frame, bool active_speech) {
    EXPECT_CALL(*mock_encoder_, Num10MsFramesInNextPacket())
        .WillRepeatedly(Return(blocks_per_frame));
    auto config = MakeCngConfig();
    const int num_cng_coefficients = config.num_cng_coefficients;
    CreateCng(std::move(config));
    EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
        .WillRepeatedly(Return(active_speech ? Vad::kActive : Vad::kPassive));

    EXPECT_CALL(*mock_encoder_, EncodeImpl(_, _, _)).Times(0);
    for (size_t i = 0; i < blocks_per_frame - 1; ++i) {
      Encode();
      EXPECT_EQ(0u, encoded_info_.encoded_bytes);
    }
    if (active_speech)
      ExpectEncodeCalls(blocks_per_frame);
    Encode();
    if (active_speech) {
      EXPECT_EQ(kMockReturnEncodedBytes, encoded_info_.encoded_bytes);
    } else {
      EXPECT_EQ(static_cast<size_t>(num_cng_coefficients + 1),
                encoded_info_.encoded_bytes);
    }
  }

  void CheckVadInputSize(int input_frame_size_ms,
                         int expected_first_block_size_ms,
                         int expected_second_block_size_ms) {
    const size_t blocks_per_frame =
        static_cast<size_t>(input_frame_size_ms / 10);

    EXPECT_CALL(*mock_encoder_, Num10MsFramesInNextPacket())
        .WillRepeatedly(Return(blocks_per_frame));

    EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _)).Times(0);
    for (size_t i = 0; i < blocks_per_frame - 1; ++i) {
      Encode();
    }

    InSequence s;
    EXPECT_CALL(
        *mock_vad_,
        VoiceActivity(_, expected_first_block_size_ms * sample_rate_hz_ / 1000,
                      sample_rate_hz_))
        .WillOnce(Return(Vad::kPassive));
    if (expected_second_block_size_ms > 0) {
      EXPECT_CALL(*mock_vad_,
                  VoiceActivity(
                      _, expected_second_block_size_ms * sample_rate_hz_ / 1000,
                      sample_rate_hz_))
          .WillOnce(Return(Vad::kPassive));
    }

    Encode();
  }

  bool CheckMixedActivePassive(Vad::Activity first_type,
                               Vad::Activity second_type) {

    const size_t blocks_per_frame = 6;
    EXPECT_CALL(*mock_encoder_, Num10MsFramesInNextPacket())
        .WillRepeatedly(Return(blocks_per_frame));
    InSequence s;
    EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
        .WillOnce(Return(first_type));
    if (first_type == Vad::kPassive) {

      EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
          .WillOnce(Return(second_type));
    }
    encoded_info_.payload_type = 0;
    for (size_t i = 0; i < blocks_per_frame; ++i) {
      Encode();
    }
    return encoded_info_.payload_type != kCngPayloadType;
  }

  std::unique_ptr<AudioEncoder> cng_;
  std::unique_ptr<MockAudioEncoder> mock_encoder_owner_;
  MockAudioEncoder* mock_encoder_;
  MockVad* mock_vad_;
  uint32_t timestamp_;
  int16_t audio_[kMaxNumSamples];
  size_t num_audio_samples_10ms_;
  rtc::Buffer encoded_;
  AudioEncoder::EncodedInfo encoded_info_;
  int sample_rate_hz_;
};

TEST_F(AudioEncoderCngTest, CreateAndDestroy) {
  CreateCng(MakeCngConfig());
}

TEST_F(AudioEncoderCngTest, CheckFrameSizePropagation) {
  CreateCng(MakeCngConfig());
  EXPECT_CALL(*mock_encoder_, Num10MsFramesInNextPacket())
      .WillOnce(Return(17U));
  EXPECT_EQ(17U, cng_->Num10MsFramesInNextPacket());
}

TEST_F(AudioEncoderCngTest, CheckTargetAudioBitratePropagation) {
  CreateCng(MakeCngConfig());
  EXPECT_CALL(*mock_encoder_,
              OnReceivedUplinkBandwidth(4711, absl::optional<int64_t>()));
  cng_->OnReceivedUplinkBandwidth(4711, absl::nullopt);
}

TEST_F(AudioEncoderCngTest, CheckPacketLossFractionPropagation) {
  CreateCng(MakeCngConfig());
  EXPECT_CALL(*mock_encoder_, OnReceivedUplinkPacketLossFraction(0.5));
  cng_->OnReceivedUplinkPacketLossFraction(0.5);
}

TEST_F(AudioEncoderCngTest, CheckGetFrameLengthRangePropagation) {
  CreateCng(MakeCngConfig());
  auto expected_range =
      std::make_pair(TimeDelta::Millis(20), TimeDelta::Millis(20));
  EXPECT_CALL(*mock_encoder_, GetFrameLengthRange())
      .WillRepeatedly(Return(absl::make_optional(expected_range)));
  EXPECT_THAT(cng_->GetFrameLengthRange(), Optional(Eq(expected_range)));
}

TEST_F(AudioEncoderCngTest, EncodeCallsVad) {
  EXPECT_CALL(*mock_encoder_, Num10MsFramesInNextPacket())
      .WillRepeatedly(Return(1U));
  CreateCng(MakeCngConfig());
  EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
      .WillOnce(Return(Vad::kPassive));
  Encode();
}

TEST_F(AudioEncoderCngTest, EncodeCollects1BlockPassiveSpeech) {
  CheckBlockGrouping(1, false);
}

TEST_F(AudioEncoderCngTest, EncodeCollects2BlocksPassiveSpeech) {
  CheckBlockGrouping(2, false);
}

TEST_F(AudioEncoderCngTest, EncodeCollects3BlocksPassiveSpeech) {
  CheckBlockGrouping(3, false);
}

TEST_F(AudioEncoderCngTest, EncodeCollects1BlockActiveSpeech) {
  CheckBlockGrouping(1, true);
}

TEST_F(AudioEncoderCngTest, EncodeCollects2BlocksActiveSpeech) {
  CheckBlockGrouping(2, true);
}

TEST_F(AudioEncoderCngTest, EncodeCollects3BlocksActiveSpeech) {
  CheckBlockGrouping(3, true);
}

TEST_F(AudioEncoderCngTest, EncodePassive) {
  const size_t kBlocksPerFrame = 3;
  EXPECT_CALL(*mock_encoder_, Num10MsFramesInNextPacket())
      .WillRepeatedly(Return(kBlocksPerFrame));
  auto config = MakeCngConfig();
  const auto sid_frame_interval_ms = config.sid_frame_interval_ms;
  const auto num_cng_coefficients = config.num_cng_coefficients;
  CreateCng(std::move(config));
  EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
      .WillRepeatedly(Return(Vad::kPassive));

  EXPECT_CALL(*mock_encoder_, EncodeImpl(_, _, _)).Times(0);
  uint32_t expected_timestamp = timestamp_;
  for (size_t i = 0; i < 100; ++i) {
    Encode();

    if ((i + 1) % kBlocksPerFrame == 0) {

      if ((i % (sid_frame_interval_ms / 10)) < kBlocksPerFrame) {

        EXPECT_EQ(kCngPayloadType, encoded_info_.payload_type);
        EXPECT_FALSE(encoded_info_.speech);
        EXPECT_EQ(static_cast<size_t>(num_cng_coefficients) + 1,
                  encoded_info_.encoded_bytes);
        EXPECT_EQ(expected_timestamp, encoded_info_.encoded_timestamp);
      }
      expected_timestamp += rtc::checked_cast<uint32_t>(
          kBlocksPerFrame * num_audio_samples_10ms_);
    } else {

      EXPECT_EQ(0u, encoded_info_.encoded_bytes);
    }
  }
}

TEST_F(AudioEncoderCngTest, MixedActivePassive) {
  CreateCng(MakeCngConfig());

  ExpectEncodeCalls(6);
  EXPECT_TRUE(CheckMixedActivePassive(Vad::kActive, Vad::kActive));
  EXPECT_TRUE(encoded_info_.speech);

  ExpectEncodeCalls(6);
  EXPECT_TRUE(CheckMixedActivePassive(Vad::kActive, Vad::kPassive));
  EXPECT_TRUE(encoded_info_.speech);

  ExpectEncodeCalls(6);
  EXPECT_TRUE(CheckMixedActivePassive(Vad::kPassive, Vad::kActive));
  EXPECT_TRUE(encoded_info_.speech);

  EXPECT_FALSE(CheckMixedActivePassive(Vad::kPassive, Vad::kPassive));
  EXPECT_FALSE(encoded_info_.speech);
}

TEST_F(AudioEncoderCngTest, VadInputSize10Ms) {
  CreateCng(MakeCngConfig());
  CheckVadInputSize(10, 10, 0);
}
TEST_F(AudioEncoderCngTest, VadInputSize20Ms) {
  CreateCng(MakeCngConfig());
  CheckVadInputSize(20, 20, 0);
}
TEST_F(AudioEncoderCngTest, VadInputSize30Ms) {
  CreateCng(MakeCngConfig());
  CheckVadInputSize(30, 30, 0);
}
TEST_F(AudioEncoderCngTest, VadInputSize40Ms) {
  CreateCng(MakeCngConfig());
  CheckVadInputSize(40, 20, 20);
}
TEST_F(AudioEncoderCngTest, VadInputSize50Ms) {
  CreateCng(MakeCngConfig());
  CheckVadInputSize(50, 30, 20);
}
TEST_F(AudioEncoderCngTest, VadInputSize60Ms) {
  CreateCng(MakeCngConfig());
  CheckVadInputSize(60, 30, 30);
}

TEST_F(AudioEncoderCngTest, VerifyCngPayloadType) {
  CreateCng(MakeCngConfig());
  EXPECT_CALL(*mock_encoder_, EncodeImpl(_, _, _)).Times(0);
  EXPECT_CALL(*mock_encoder_, Num10MsFramesInNextPacket()).WillOnce(Return(1U));
  EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
      .WillOnce(Return(Vad::kPassive));
  encoded_info_.payload_type = 0;
  Encode();
  EXPECT_EQ(kCngPayloadType, encoded_info_.payload_type);
}

TEST_F(AudioEncoderCngTest, VerifySidFrameAfterSpeech) {
  auto config = MakeCngConfig();
  const auto num_cng_coefficients = config.num_cng_coefficients;
  CreateCng(std::move(config));
  EXPECT_CALL(*mock_encoder_, Num10MsFramesInNextPacket())
      .WillRepeatedly(Return(1U));

  EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
      .Times(2)
      .WillRepeatedly(Return(Vad::kPassive));
  Encode();
  EXPECT_EQ(kCngPayloadType, encoded_info_.payload_type);
  EXPECT_EQ(static_cast<size_t>(num_cng_coefficients) + 1,
            encoded_info_.encoded_bytes);

  Encode();
  EXPECT_EQ(0u, encoded_info_.encoded_bytes);

  encoded_info_.payload_type = 0;
  EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
      .WillOnce(Return(Vad::kActive));
  EXPECT_CALL(*mock_encoder_, EncodeImpl(_, _, _))
      .WillOnce(
          Invoke(MockAudioEncoder::FakeEncoding(kMockReturnEncodedBytes)));
  Encode();
  EXPECT_EQ(kMockReturnEncodedBytes, encoded_info_.encoded_bytes);

  EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
      .WillOnce(Return(Vad::kPassive));
  Encode();
  EXPECT_EQ(kCngPayloadType, encoded_info_.payload_type);
  EXPECT_EQ(static_cast<size_t>(num_cng_coefficients) + 1,
            encoded_info_.encoded_bytes);
}

TEST_F(AudioEncoderCngTest, Reset) {
  CreateCng(MakeCngConfig());
  EXPECT_CALL(*mock_encoder_, Reset()).Times(1);
  EXPECT_CALL(*mock_vad_, Reset()).Times(1);
  cng_->Reset();
}

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

class AudioEncoderCngDeathTest : public AudioEncoderCngTest {
 protected:
  AudioEncoderCngDeathTest() : AudioEncoderCngTest() {
    EXPECT_CALL(*mock_vad_, Die()).Times(1);
    delete mock_vad_;
    mock_vad_ = nullptr;
  }

  void TearDown() override { cng_.reset(); }

  AudioEncoderCngConfig MakeCngConfig() {

    auto config = AudioEncoderCngTest::MakeCngConfig();
    config.vad = nullptr;
    return config;
  }

  void TryWrongNumCoefficients(int num) {
    RTC_EXPECT_DEATH(
        [&] {
          auto config = MakeCngConfig();
          config.num_cng_coefficients = num;
          CreateCng(std::move(config));
        }(),
        "Invalid configuration");
  }
};

TEST_F(AudioEncoderCngDeathTest, WrongFrameSize) {
  CreateCng(MakeCngConfig());
  num_audio_samples_10ms_ *= 2;
  RTC_EXPECT_DEATH(Encode(), "");
  num_audio_samples_10ms_ = 0;
  RTC_EXPECT_DEATH(Encode(), "");
}

TEST_F(AudioEncoderCngDeathTest, WrongNumCoefficientsA) {
  TryWrongNumCoefficients(-1);
}

TEST_F(AudioEncoderCngDeathTest, WrongNumCoefficientsB) {
  TryWrongNumCoefficients(0);
}

TEST_F(AudioEncoderCngDeathTest, WrongNumCoefficientsC) {
  TryWrongNumCoefficients(13);
}

TEST_F(AudioEncoderCngDeathTest, NullSpeechEncoder) {
  auto config = MakeCngConfig();
  config.speech_encoder = nullptr;
  RTC_EXPECT_DEATH(CreateCng(std::move(config)), "");
}

TEST_F(AudioEncoderCngDeathTest, StereoEncoder) {
  EXPECT_CALL(*mock_encoder_, NumChannels()).WillRepeatedly(Return(2));
  RTC_EXPECT_DEATH(CreateCng(MakeCngConfig()), "Invalid configuration");
}

TEST_F(AudioEncoderCngDeathTest, StereoConfig) {
  RTC_EXPECT_DEATH(
      [&] {
        auto config = MakeCngConfig();
        config.num_channels = 2;
        CreateCng(std::move(config));
      }(),
      "Invalid configuration");
}

TEST_F(AudioEncoderCngDeathTest, EncoderFrameSizeTooLarge) {
  CreateCng(MakeCngConfig());
  EXPECT_CALL(*mock_encoder_, Num10MsFramesInNextPacket())
      .WillRepeatedly(Return(7U));
  for (int i = 0; i < 6; ++i)
    Encode();
  RTC_EXPECT_DEATH(
      Encode(), "Frame size cannot be larger than 60 ms when using VAD/CNG.");
}

#endif

}
