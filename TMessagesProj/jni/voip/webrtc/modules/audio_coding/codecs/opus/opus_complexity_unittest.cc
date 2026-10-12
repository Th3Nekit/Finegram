/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "api/test/metrics/global_metrics_logger_and_exporter.h"
#include "api/test/metrics/metric.h"
#include "modules/audio_coding/neteq/tools/audio_loop.h"
#include "rtc_base/time_utils.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace {

using ::webrtc::test::GetGlobalMetricsLogger;
using ::webrtc::test::ImprovementDirection;
using ::webrtc::test::Unit;

int64_t RunComplexityTest(const AudioEncoderOpusConfig& config) {

  constexpr int payload_type = 17;
  const auto encoder = AudioEncoderOpus::MakeAudioEncoder(config, payload_type);

  const std::string kInputFileName =
      webrtc::test::ResourcePath("audio_coding/speech_mono_32_48kHz", "pcm");
  test::AudioLoop audio_loop;
  constexpr int kSampleRateHz = 48000;
  EXPECT_EQ(kSampleRateHz, encoder->SampleRateHz());
  constexpr size_t kMaxLoopLengthSamples =
      kSampleRateHz * 10;
  constexpr size_t kInputBlockSizeSamples =
      10 * kSampleRateHz / 1000;
  EXPECT_TRUE(audio_loop.Init(kInputFileName, kMaxLoopLengthSamples,
                              kInputBlockSizeSamples));

  const int64_t start_time_ms = rtc::TimeMillis();
  AudioEncoder::EncodedInfo info;
  rtc::Buffer encoded(500);
  uint32_t rtp_timestamp = 0u;
  for (size_t i = 0; i < 10000; ++i) {
    encoded.Clear();
    info = encoder->Encode(rtp_timestamp, audio_loop.GetNextBlock(), &encoded);
    rtp_timestamp += kInputBlockSizeSamples;
  }
  return rtc::TimeMillis() - start_time_ms;
}

TEST(AudioEncoderOpusComplexityAdaptationTest, Adaptation_On) {

  AudioEncoderOpusConfig config;

  config.bitrate_bps = 11000 - 1;
  config.low_rate_complexity = 9;
  int64_t runtime_10999bps = RunComplexityTest(config);

  config.bitrate_bps = 15500;
  int64_t runtime_15500bps = RunComplexityTest(config);

  GetGlobalMetricsLogger()->LogSingleValueMetric(
      "opus_encoding_complexity_ratio", "adaptation_on",
      100.0 * runtime_10999bps / runtime_15500bps, Unit::kPercent,
      ImprovementDirection::kNeitherIsBetter);
}

TEST(AudioEncoderOpusComplexityAdaptationTest, Adaptation_Off) {

  AudioEncoderOpusConfig config;

  config.bitrate_bps = 11000 - 1;
  int64_t runtime_10999bps = RunComplexityTest(config);

  config.bitrate_bps = 15500;
  int64_t runtime_15500bps = RunComplexityTest(config);

  GetGlobalMetricsLogger()->LogSingleValueMetric(
      "opus_encoding_complexity_ratio", "adaptation_off",
      100.0 * runtime_10999bps / runtime_15500bps, Unit::kPercent,
      ImprovementDirection::kNeitherIsBetter);
}

}
}
