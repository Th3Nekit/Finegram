/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/numerics/samples_stats_counter.h"

#include <math.h>

#include <random>
#include <vector>

#include "absl/algorithm/container.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

SamplesStatsCounter CreateStatsFilledWithIntsFrom1ToN(int n) {
  std::vector<double> data;
  for (int i = 1; i <= n; i++) {
    data.push_back(i);
  }
  absl::c_shuffle(data, std::mt19937(std::random_device()()));

  SamplesStatsCounter stats;
  for (double v : data) {
    stats.AddSample(v);
  }
  return stats;
}

SamplesStatsCounter CreateStatsFromUniformDistribution(int n,
                                                       double a,
                                                       double b) {
  std::mt19937 gen{std::random_device()()};
  std::uniform_real_distribution<> dis(a, b);

  SamplesStatsCounter stats;
  for (int i = 1; i <= n; i++) {
    stats.AddSample(dis(gen));
  }
  return stats;
}

class SamplesStatsCounterTest : public ::testing::TestWithParam<int> {};

constexpr int SIZE_FOR_MERGE = 10;

}

TEST(SamplesStatsCounterTest, FullSimpleTest) {
  SamplesStatsCounter stats = CreateStatsFilledWithIntsFrom1ToN(100);

  EXPECT_TRUE(!stats.IsEmpty());
  EXPECT_DOUBLE_EQ(stats.GetMin(), 1.0);
  EXPECT_DOUBLE_EQ(stats.GetMax(), 100.0);
  EXPECT_DOUBLE_EQ(stats.GetSum(), 5050.0);
  EXPECT_NEAR(stats.GetAverage(), 50.5, 1e-6);
  for (int i = 1; i <= 100; i++) {
    double p = i / 100.0;
    EXPECT_GE(stats.GetPercentile(p), i);
    EXPECT_LT(stats.GetPercentile(p), i + 1);
  }
}

TEST(SamplesStatsCounterTest, VarianceAndDeviation) {
  SamplesStatsCounter stats;
  stats.AddSample(2);
  stats.AddSample(2);
  stats.AddSample(-1);
  stats.AddSample(5);

  EXPECT_DOUBLE_EQ(stats.GetAverage(), 2.0);
  EXPECT_DOUBLE_EQ(stats.GetVariance(), 4.5);
  EXPECT_DOUBLE_EQ(stats.GetStandardDeviation(), sqrt(4.5));
}

TEST(SamplesStatsCounterTest, FractionPercentile) {
  SamplesStatsCounter stats = CreateStatsFilledWithIntsFrom1ToN(5);

  EXPECT_DOUBLE_EQ(stats.GetPercentile(0.5), 3);
}

TEST(SamplesStatsCounterTest, TestBorderValues) {
  SamplesStatsCounter stats = CreateStatsFilledWithIntsFrom1ToN(5);

  EXPECT_GE(stats.GetPercentile(0.01), 1);
  EXPECT_LT(stats.GetPercentile(0.01), 2);
  EXPECT_DOUBLE_EQ(stats.GetPercentile(1.0), 5);
}

TEST(SamplesStatsCounterTest, VarianceFromUniformDistribution) {

  SamplesStatsCounter stats = CreateStatsFromUniformDistribution(1e6, 0, 1);

  EXPECT_NEAR(stats.GetVariance(), 1. / 12, 1e-3);
}

TEST(SamplesStatsCounterTest, NumericStabilityForVariance) {

  SamplesStatsCounter stats =
      CreateStatsFromUniformDistribution(1e6, 1e9, 1e9 + 1);

  EXPECT_NEAR(stats.GetVariance(), 1. / 12, 1e-3);
}

TEST_P(SamplesStatsCounterTest, AddSamples) {
  int data[SIZE_FOR_MERGE] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  SamplesStatsCounter stats0, stats1;
  for (int i = 0; i < GetParam(); ++i) {
    stats0.AddSample(data[i]);
  }
  for (int i = GetParam(); i < SIZE_FOR_MERGE; ++i) {
    stats1.AddSample(data[i]);
  }
  stats0.AddSamples(stats1);

  EXPECT_EQ(stats0.GetMin(), 0);
  EXPECT_EQ(stats0.GetMax(), 9);
  EXPECT_DOUBLE_EQ(stats0.GetAverage(), 4.5);
  EXPECT_DOUBLE_EQ(stats0.GetVariance(), 8.25);
  EXPECT_DOUBLE_EQ(stats0.GetStandardDeviation(), sqrt(8.25));
  EXPECT_DOUBLE_EQ(stats0.GetPercentile(0.1), 0.9);
  EXPECT_DOUBLE_EQ(stats0.GetPercentile(0.5), 4.5);
  EXPECT_DOUBLE_EQ(stats0.GetPercentile(0.9), 8.1);
}

TEST(SamplesStatsCounterTest, MultiplyRight) {
  SamplesStatsCounter stats = CreateStatsFilledWithIntsFrom1ToN(10);

  EXPECT_TRUE(!stats.IsEmpty());
  EXPECT_DOUBLE_EQ(stats.GetMin(), 1.0);
  EXPECT_DOUBLE_EQ(stats.GetMax(), 10.0);
  EXPECT_DOUBLE_EQ(stats.GetAverage(), 5.5);

  SamplesStatsCounter multiplied_stats = stats * 10;
  EXPECT_TRUE(!multiplied_stats.IsEmpty());
  EXPECT_DOUBLE_EQ(multiplied_stats.GetMin(), 10.0);
  EXPECT_DOUBLE_EQ(multiplied_stats.GetMax(), 100.0);
  EXPECT_DOUBLE_EQ(multiplied_stats.GetAverage(), 55.0);
  EXPECT_EQ(multiplied_stats.GetSamples().size(), stats.GetSamples().size());

  EXPECT_TRUE(!stats.IsEmpty());
  EXPECT_DOUBLE_EQ(stats.GetMin(), 1.0);
  EXPECT_DOUBLE_EQ(stats.GetMax(), 10.0);
  EXPECT_DOUBLE_EQ(stats.GetAverage(), 5.5);
}

TEST(SamplesStatsCounterTest, MultiplyLeft) {
  SamplesStatsCounter stats = CreateStatsFilledWithIntsFrom1ToN(10);

  EXPECT_TRUE(!stats.IsEmpty());
  EXPECT_DOUBLE_EQ(stats.GetMin(), 1.0);
  EXPECT_DOUBLE_EQ(stats.GetMax(), 10.0);
  EXPECT_DOUBLE_EQ(stats.GetAverage(), 5.5);

  SamplesStatsCounter multiplied_stats = 10 * stats;
  EXPECT_TRUE(!multiplied_stats.IsEmpty());
  EXPECT_DOUBLE_EQ(multiplied_stats.GetMin(), 10.0);
  EXPECT_DOUBLE_EQ(multiplied_stats.GetMax(), 100.0);
  EXPECT_DOUBLE_EQ(multiplied_stats.GetAverage(), 55.0);
  EXPECT_EQ(multiplied_stats.GetSamples().size(), stats.GetSamples().size());

  EXPECT_TRUE(!stats.IsEmpty());
  EXPECT_DOUBLE_EQ(stats.GetMin(), 1.0);
  EXPECT_DOUBLE_EQ(stats.GetMax(), 10.0);
  EXPECT_DOUBLE_EQ(stats.GetAverage(), 5.5);
}

TEST(SamplesStatsCounterTest, Divide) {
  SamplesStatsCounter stats;
  for (int i = 1; i <= 10; i++) {
    stats.AddSample(i * 10);
  }

  EXPECT_TRUE(!stats.IsEmpty());
  EXPECT_DOUBLE_EQ(stats.GetMin(), 10.0);
  EXPECT_DOUBLE_EQ(stats.GetMax(), 100.0);
  EXPECT_DOUBLE_EQ(stats.GetAverage(), 55.0);

  SamplesStatsCounter divided_stats = stats / 10;
  EXPECT_TRUE(!divided_stats.IsEmpty());
  EXPECT_DOUBLE_EQ(divided_stats.GetMin(), 1.0);
  EXPECT_DOUBLE_EQ(divided_stats.GetMax(), 10.0);
  EXPECT_DOUBLE_EQ(divided_stats.GetAverage(), 5.5);
  EXPECT_EQ(divided_stats.GetSamples().size(), stats.GetSamples().size());

  EXPECT_TRUE(!stats.IsEmpty());
  EXPECT_DOUBLE_EQ(stats.GetMin(), 10.0);
  EXPECT_DOUBLE_EQ(stats.GetMax(), 100.0);
  EXPECT_DOUBLE_EQ(stats.GetAverage(), 55.0);
}

INSTANTIATE_TEST_SUITE_P(SamplesStatsCounterTests,
                         SamplesStatsCounterTest,
                         ::testing::Range(0, SIZE_FOR_MERGE + 1));

}
