/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/statistics_calculator.h"

#include "test/gtest.h"

namespace webrtc {

TEST(LifetimeStatistics, TotalSamplesReceived) {
  StatisticsCalculator stats;
  for (int i = 0; i < 10; ++i) {
    stats.IncreaseCounter(480, 48000);
  }
  EXPECT_EQ(10 * 480u, stats.GetLifetimeStatistics().total_samples_received);
}

TEST(LifetimeStatistics, SamplesConcealed) {
  StatisticsCalculator stats;
  stats.ExpandedVoiceSamples(100, false);
  stats.ExpandedNoiseSamples(17, false);
  EXPECT_EQ(100u + 17u, stats.GetLifetimeStatistics().concealed_samples);
}

TEST(LifetimeStatistics, SamplesConcealedCorrection) {
  StatisticsCalculator stats;
  stats.ExpandedVoiceSamples(100, false);
  EXPECT_EQ(100u, stats.GetLifetimeStatistics().concealed_samples);
  stats.ExpandedVoiceSamplesCorrection(-10);

  EXPECT_EQ(100u, stats.GetLifetimeStatistics().concealed_samples);
  stats.ExpandedVoiceSamplesCorrection(20);

  EXPECT_EQ(110u, stats.GetLifetimeStatistics().concealed_samples);

  stats.ExpandedVoiceSamplesCorrection(-17);
  EXPECT_EQ(110u, stats.GetLifetimeStatistics().concealed_samples);
  stats.ExpandedVoiceSamples(100, false);
  EXPECT_EQ(110u + 100u - 17u, stats.GetLifetimeStatistics().concealed_samples);
}

TEST(LifetimeStatistics, NoUpdateOnTimeStretch) {
  StatisticsCalculator stats;
  stats.ExpandedVoiceSamples(100, false);
  stats.AcceleratedSamples(4711);
  stats.PreemptiveExpandedSamples(17);
  stats.ExpandedVoiceSamples(100, false);
  EXPECT_EQ(200u, stats.GetLifetimeStatistics().concealed_samples);
}

TEST(StatisticsCalculator, ExpandedSamplesCorrection) {
  StatisticsCalculator stats;
  NetEqNetworkStatistics stats_output;
  constexpr int kSampleRateHz = 48000;
  constexpr int k10MsSamples = kSampleRateHz / 100;
  constexpr int kPacketSizeMs = 20;
  constexpr size_t kSamplesPerPacket = kPacketSizeMs * kSampleRateHz / 1000;

  stats.IncreaseCounter(k10MsSamples, kSampleRateHz);

  stats.GetNetworkStatistics(kSamplesPerPacket, &stats_output);

  EXPECT_EQ(0u, stats_output.expand_rate);
  EXPECT_EQ(0u, stats_output.speech_expand_rate);

  stats.ExpandedVoiceSamplesCorrection(-100);
  stats.ExpandedNoiseSamplesCorrection(-100);
  stats.IncreaseCounter(k10MsSamples, kSampleRateHz);
  stats.GetNetworkStatistics(kSamplesPerPacket, &stats_output);

  EXPECT_EQ(0u, stats_output.expand_rate);
  EXPECT_EQ(0u, stats_output.speech_expand_rate);

  stats.ExpandedVoiceSamplesCorrection(50);
  stats.ExpandedNoiseSamplesCorrection(200);
  stats.IncreaseCounter(k10MsSamples, kSampleRateHz);
  stats.GetNetworkStatistics(kSamplesPerPacket, &stats_output);

  EXPECT_EQ(((50u + 200u) << 14) / k10MsSamples, stats_output.expand_rate);
  EXPECT_EQ((50u << 14) / k10MsSamples, stats_output.speech_expand_rate);
}

TEST(StatisticsCalculator, RelativePacketArrivalDelay) {
  StatisticsCalculator stats;

  stats.RelativePacketArrivalDelay(50);
  NetEqLifetimeStatistics stats_output = stats.GetLifetimeStatistics();
  EXPECT_EQ(50u, stats_output.relative_packet_arrival_delay_ms);

  stats.RelativePacketArrivalDelay(20);
  stats_output = stats.GetLifetimeStatistics();
  EXPECT_EQ(70u, stats_output.relative_packet_arrival_delay_ms);
}

TEST(StatisticsCalculator, ReceivedPacket) {
  StatisticsCalculator stats;

  stats.ReceivedPacket();
  NetEqLifetimeStatistics stats_output = stats.GetLifetimeStatistics();
  EXPECT_EQ(1u, stats_output.jitter_buffer_packets_received);

  stats.ReceivedPacket();
  stats_output = stats.GetLifetimeStatistics();
  EXPECT_EQ(2u, stats_output.jitter_buffer_packets_received);
}

TEST(StatisticsCalculator, InterruptionCounter) {
  constexpr int fs_khz = 48;
  constexpr int fs_hz = fs_khz * 1000;
  StatisticsCalculator stats;
  stats.DecodedOutputPlayed();
  stats.EndExpandEvent(fs_hz);
  auto lts = stats.GetLifetimeStatistics();
  EXPECT_EQ(0, lts.interruption_count);
  EXPECT_EQ(0, lts.total_interruption_duration_ms);

  stats.ExpandedVoiceSamples(10 * fs_khz, false);
  stats.ExpandedNoiseSamples(139 * fs_khz, false);
  stats.EndExpandEvent(fs_hz);
  lts = stats.GetLifetimeStatistics();
  EXPECT_EQ(0, lts.interruption_count);

  stats.ExpandedVoiceSamples(140 * fs_khz, false);
  stats.ExpandedNoiseSamples(11 * fs_khz, false);
  stats.EndExpandEvent(fs_hz);
  lts = stats.GetLifetimeStatistics();
  EXPECT_EQ(1, lts.interruption_count);
  EXPECT_EQ(151, lts.total_interruption_duration_ms);

  stats.ExpandedVoiceSamples(100 * fs_khz, false);
  stats.ExpandedNoiseSamples(5000 * fs_khz, false);
  stats.EndExpandEvent(fs_hz);
  lts = stats.GetLifetimeStatistics();
  EXPECT_EQ(2, lts.interruption_count);
  EXPECT_EQ(5100 + 151, lts.total_interruption_duration_ms);
}

TEST(StatisticsCalculator, InterruptionCounterDoNotLogBeforeDecoding) {
  constexpr int fs_khz = 48;
  constexpr int fs_hz = fs_khz * 1000;
  StatisticsCalculator stats;

  stats.ExpandedVoiceSamples(151 * fs_khz, false);
  stats.EndExpandEvent(fs_hz);
  auto lts = stats.GetLifetimeStatistics();
  EXPECT_EQ(0, lts.interruption_count);

  stats.DecodedOutputPlayed();

  stats.ExpandedVoiceSamples(151 * fs_khz, false);
  stats.EndExpandEvent(fs_hz);
  lts = stats.GetLifetimeStatistics();
  EXPECT_EQ(1, lts.interruption_count);
}

TEST(StatisticsCalculator, DiscardedPackets) {
  StatisticsCalculator statistics_calculator;
  EXPECT_EQ(0u,
            statistics_calculator.GetLifetimeStatistics().packets_discarded);

  statistics_calculator.PacketsDiscarded(1);
  EXPECT_EQ(1u,
            statistics_calculator.GetLifetimeStatistics().packets_discarded);

  statistics_calculator.PacketsDiscarded(10);
  EXPECT_EQ(11u,
            statistics_calculator.GetLifetimeStatistics().packets_discarded);

  statistics_calculator.SecondaryPacketsDiscarded(1);
  EXPECT_EQ(11u,
            statistics_calculator.GetLifetimeStatistics().packets_discarded);

  statistics_calculator.FlushedPacketBuffer();
  EXPECT_EQ(11u,
            statistics_calculator.GetLifetimeStatistics().packets_discarded);
}

}
