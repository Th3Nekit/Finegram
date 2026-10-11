/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/ilbc/audio_decoder_ilbc.h"
#include "modules/audio_coding/codecs/ilbc/audio_encoder_ilbc.h"
#include "modules/audio_coding/codecs/legacy_encoded_audio_frame.h"
#include "test/gtest.h"

namespace webrtc {

TEST(IlbcTest, BadPacket) {

  AudioEncoderIlbcConfig config;
  config.frame_size_ms = 20;

  AudioEncoderIlbcImpl encoder(config, 102);
  std::vector<int16_t> samples(encoder.SampleRateHz() / 100, 4711);
  rtc::Buffer packet;
  int num_10ms_chunks = 0;
  while (packet.size() == 0) {
    encoder.Encode(0, samples, &packet);
    num_10ms_chunks += 1;
  }

  EXPECT_EQ(38u, packet.size());
  rtc::Buffer bad_packet(packet.data(), packet.size());
  bad_packet[29] |= 0x3f;
  bad_packet[30] |= 0x80;

  AudioDecoderIlbcImpl decoder;
  std::vector<int16_t> decoded_samples(num_10ms_chunks * samples.size());
  AudioDecoder::SpeechType speech_type;
  EXPECT_EQ(-1, decoder.Decode(bad_packet.data(), bad_packet.size(),
                               encoder.SampleRateHz(),
                               sizeof(int16_t) * decoded_samples.size(),
                               decoded_samples.data(), &speech_type));

  EXPECT_EQ(static_cast<int>(decoded_samples.size()),
            decoder.Decode(packet.data(), packet.size(), encoder.SampleRateHz(),
                           sizeof(int16_t) * decoded_samples.size(),
                           decoded_samples.data(), &speech_type));
}

class SplitIlbcTest : public ::testing::TestWithParam<std::pair<int, int> > {
 protected:
  virtual void SetUp() {
    const std::pair<int, int> parameters = GetParam();
    num_frames_ = parameters.first;
    frame_length_ms_ = parameters.second;
    frame_length_bytes_ = (frame_length_ms_ == 20) ? 38 : 50;
  }
  size_t num_frames_;
  int frame_length_ms_;
  size_t frame_length_bytes_;
};

TEST_P(SplitIlbcTest, NumFrames) {
  AudioDecoderIlbcImpl decoder;
  const size_t frame_length_samples = frame_length_ms_ * 8;
  const auto generate_payload = [](size_t payload_length_bytes) {
    rtc::Buffer payload(payload_length_bytes);

    for (size_t i = 0; i < payload.size(); ++i) {
      payload[i] = static_cast<uint8_t>(i);
    }
    return payload;
  };

  const auto results = decoder.ParsePayload(
      generate_payload(frame_length_bytes_ * num_frames_), 0);
  EXPECT_EQ(num_frames_, results.size());

  size_t frame_num = 0;
  uint8_t payload_value = 0;
  for (const auto& result : results) {
    EXPECT_EQ(frame_length_samples * frame_num, result.timestamp);
    const LegacyEncodedAudioFrame* frame =
        static_cast<const LegacyEncodedAudioFrame*>(result.frame.get());
    const rtc::Buffer& payload = frame->payload();
    EXPECT_EQ(frame_length_bytes_, payload.size());
    for (size_t i = 0; i < payload.size(); ++i, ++payload_value) {
      EXPECT_EQ(payload_value, payload[i]);
    }
    ++frame_num;
  }
}

INSTANTIATE_TEST_SUITE_P(
    IlbcTest,
    SplitIlbcTest,
    ::testing::Values(std::pair<int, int>(1, 20),
                      std::pair<int, int>(2, 20),
                      std::pair<int, int>(3, 20),
                      std::pair<int, int>(4, 20),
                      std::pair<int, int>(5, 20),
                      std::pair<int, int>(24, 20),
                      std::pair<int, int>(1, 30),
                      std::pair<int, int>(2, 30),
                      std::pair<int, int>(3, 30),
                      std::pair<int, int>(4, 30),
                      std::pair<int, int>(5, 30),
                      std::pair<int, int>(18, 30)));

TEST(IlbcTest, SplitTooLargePayload) {
  AudioDecoderIlbcImpl decoder;
  constexpr size_t kPayloadLengthBytes = 950;
  const auto results =
      decoder.ParsePayload(rtc::Buffer(kPayloadLengthBytes), 0);
  EXPECT_TRUE(results.empty());
}

TEST(IlbcTest, SplitUnevenPayload) {
  AudioDecoderIlbcImpl decoder;
  constexpr size_t kPayloadLengthBytes = 39;
  const auto results =
      decoder.ParsePayload(rtc::Buffer(kPayloadLengthBytes), 0);
  EXPECT_TRUE(results.empty());
}

}
