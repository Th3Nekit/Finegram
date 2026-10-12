/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES
#include "modules/audio_processing/rms_level.h"

#include <cmath>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
constexpr int kSampleRateHz = 48000;
constexpr size_t kBlockSizeSamples = kSampleRateHz / 100;

std::unique_ptr<RmsLevel> RunTest(rtc::ArrayView<const int16_t> input) {
  std::unique_ptr<RmsLevel> level(new RmsLevel);
  for (size_t n = 0; n + kBlockSizeSamples <= input.size();
       n += kBlockSizeSamples) {
    level->Analyze(input.subview(n, kBlockSizeSamples));
  }
  return level;
}

std::unique_ptr<RmsLevel> RunTest(rtc::ArrayView<const float> input) {
  std::unique_ptr<RmsLevel> level(new RmsLevel);
  for (size_t n = 0; n + kBlockSizeSamples <= input.size();
       n += kBlockSizeSamples) {
    level->Analyze(input.subview(n, kBlockSizeSamples));
  }
  return level;
}

std::vector<int16_t> CreateInt16Sinusoid(int frequency_hz,
                                         int amplitude,
                                         size_t num_samples) {
  std::vector<int16_t> x(num_samples);
  for (size_t n = 0; n < num_samples; ++n) {
    x[n] = rtc::saturated_cast<int16_t>(
        amplitude * std::sin(2 * M_PI * n * frequency_hz / kSampleRateHz));
  }
  return x;
}

std::vector<float> CreateFloatSinusoid(int frequency_hz,
                                       int amplitude,
                                       size_t num_samples) {
  std::vector<int16_t> x16 =
      CreateInt16Sinusoid(frequency_hz, amplitude, num_samples);
  std::vector<float> x(x16.size());
  for (size_t n = 0; n < x.size(); ++n) {
    x[n] = x16[n];
  }
  return x;
}

}

TEST(RmsLevelTest, VerifyIndentityBetweenFloatAndFix) {
  auto x_f = CreateFloatSinusoid(1000, INT16_MAX, kSampleRateHz);
  auto x_i = CreateFloatSinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level_f = RunTest(x_f);
  auto level_i = RunTest(x_i);
  int avg_i = level_i->Average();
  int avg_f = level_f->Average();
  EXPECT_EQ(3, avg_i);
  EXPECT_EQ(avg_f, avg_i);
}

TEST(RmsLevelTest, Run1000HzFullScale) {
  auto x = CreateInt16Sinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level = RunTest(x);
  EXPECT_EQ(3, level->Average());
}

TEST(RmsLevelTest, Run1000HzFullScaleAverageAndPeak) {
  auto x = CreateInt16Sinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level = RunTest(x);
  auto stats = level->AverageAndPeak();
  EXPECT_EQ(3, stats.average);
  EXPECT_EQ(3, stats.peak);
}

TEST(RmsLevelTest, Run1000HzHalfScale) {
  auto x = CreateInt16Sinusoid(1000, INT16_MAX / 2, kSampleRateHz);
  auto level = RunTest(x);
  EXPECT_EQ(9, level->Average());
}

TEST(RmsLevelTest, RunZeros) {
  std::vector<int16_t> x(kSampleRateHz, 0);
  auto level = RunTest(x);
  EXPECT_EQ(127, level->Average());
}

TEST(RmsLevelTest, RunZerosAverageAndPeak) {
  std::vector<int16_t> x(kSampleRateHz, 0);
  auto level = RunTest(x);
  auto stats = level->AverageAndPeak();
  EXPECT_EQ(127, stats.average);
  EXPECT_EQ(127, stats.peak);
}

TEST(RmsLevelTest, NoSamples) {
  RmsLevel level;
  EXPECT_EQ(127, level.Average());
}

TEST(RmsLevelTest, NoSamplesAverageAndPeak) {
  RmsLevel level;
  auto stats = level.AverageAndPeak();
  EXPECT_EQ(127, stats.average);
  EXPECT_EQ(127, stats.peak);
}

TEST(RmsLevelTest, PollTwice) {
  auto x = CreateInt16Sinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level = RunTest(x);
  level->Average();
  EXPECT_EQ(127, level->Average());
}

TEST(RmsLevelTest, Reset) {
  auto x = CreateInt16Sinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level = RunTest(x);
  level->Reset();
  EXPECT_EQ(127, level->Average());
}

TEST(RmsLevelTest, ProcessMuted) {
  auto x = CreateInt16Sinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level = RunTest(x);
  const size_t kBlocksPerSecond = rtc::CheckedDivExact(
      static_cast<size_t>(kSampleRateHz), kBlockSizeSamples);
  for (size_t i = 0; i < kBlocksPerSecond; ++i) {
    level->AnalyzeMuted(kBlockSizeSamples);
  }
  EXPECT_EQ(6, level->Average());
}

TEST(RmsLevelTest, OnlyDigitalSilenceIs127) {
  std::vector<int16_t> test_buffer(kSampleRateHz, 0);
  auto level = RunTest(test_buffer);
  EXPECT_EQ(127, level->Average());

  test_buffer[0] = 1;
  level = RunTest(test_buffer);
  EXPECT_LT(level->Average(), 127);
}

TEST(RmsLevelTest, RunHalfScaleAndInsertFullScale) {
  auto half_scale = CreateInt16Sinusoid(1000, INT16_MAX / 2, kSampleRateHz);
  auto full_scale = CreateInt16Sinusoid(1000, INT16_MAX, kSampleRateHz / 100);
  auto x = half_scale;
  x.insert(x.end(), full_scale.begin(), full_scale.end());
  x.insert(x.end(), half_scale.begin(), half_scale.end());
  ASSERT_EQ(static_cast<size_t>(2 * kSampleRateHz + kSampleRateHz / 100),
            x.size());
  auto level = RunTest(x);
  auto stats = level->AverageAndPeak();
  EXPECT_EQ(9, stats.average);
  EXPECT_EQ(3, stats.peak);
}

TEST(RmsLevelTest, ResetOnBlockSizeChange) {
  auto x = CreateInt16Sinusoid(1000, INT16_MAX, kSampleRateHz);
  auto level = RunTest(x);

  auto y = CreateInt16Sinusoid(1000, INT16_MAX / 2, kBlockSizeSamples * 2);
  level->Analyze(y);
  auto stats = level->AverageAndPeak();

  EXPECT_EQ(9, stats.average);
  EXPECT_EQ(9, stats.peak);
}

}
