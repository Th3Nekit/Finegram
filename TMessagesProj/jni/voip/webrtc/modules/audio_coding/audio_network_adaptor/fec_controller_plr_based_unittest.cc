/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/audio_network_adaptor/fec_controller_plr_based.h"

#include <utility>

#include "common_audio/mocks/mock_smoothing_filter.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

constexpr int kDisablingBandwidthLow = 15000;
constexpr float kDisablingPacketLossAtLowBw = 0.08f;
constexpr int kDisablingBandwidthHigh = 64000;
constexpr float kDisablingPacketLossAtHighBw = 0.01f;
constexpr int kEnablingBandwidthLow = 17000;
constexpr float kEnablingPacketLossAtLowBw = 0.1f;
constexpr int kEnablingBandwidthHigh = 64000;
constexpr float kEnablingPacketLossAtHighBw = 0.05f;

constexpr float kEpsilon = 1e-5f;

struct FecControllerPlrBasedTestStates {
  std::unique_ptr<FecControllerPlrBased> controller;
  MockSmoothingFilter* packet_loss_smoother;
};

FecControllerPlrBasedTestStates CreateFecControllerPlrBased(
    bool initial_fec_enabled,
    const ThresholdCurve& enabling_curve,
    const ThresholdCurve& disabling_curve) {
  FecControllerPlrBasedTestStates states;
  std::unique_ptr<MockSmoothingFilter> mock_smoothing_filter(
      new NiceMock<MockSmoothingFilter>());
  states.packet_loss_smoother = mock_smoothing_filter.get();
  states.controller.reset(new FecControllerPlrBased(
      FecControllerPlrBased::Config(initial_fec_enabled, enabling_curve,
                                    disabling_curve, 0),
      std::move(mock_smoothing_filter)));
  return states;
}

FecControllerPlrBasedTestStates CreateFecControllerPlrBased(
    bool initial_fec_enabled) {
  return CreateFecControllerPlrBased(
      initial_fec_enabled,
      ThresholdCurve(kEnablingBandwidthLow, kEnablingPacketLossAtLowBw,
                     kEnablingBandwidthHigh, kEnablingPacketLossAtHighBw),
      ThresholdCurve(kDisablingBandwidthLow, kDisablingPacketLossAtLowBw,
                     kDisablingBandwidthHigh, kDisablingPacketLossAtHighBw));
}

void UpdateNetworkMetrics(FecControllerPlrBasedTestStates* states,
                          const absl::optional<int>& uplink_bandwidth_bps,
                          const absl::optional<float>& uplink_packet_loss) {

  if (uplink_bandwidth_bps) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.uplink_bandwidth_bps = uplink_bandwidth_bps;
    states->controller->UpdateNetworkMetrics(network_metrics);
  }
  if (uplink_packet_loss) {
    Controller::NetworkMetrics network_metrics;
    network_metrics.uplink_packet_loss_fraction = uplink_packet_loss;
    EXPECT_CALL(*states->packet_loss_smoother, AddSample(*uplink_packet_loss));
    states->controller->UpdateNetworkMetrics(network_metrics);

    EXPECT_CALL(*states->packet_loss_smoother, GetAverage())
        .WillOnce(Return(*uplink_packet_loss));
  }
}

void CheckDecision(FecControllerPlrBasedTestStates* states,
                   bool expected_enable_fec,
                   float expected_uplink_packet_loss_fraction) {
  AudioEncoderRuntimeConfig config;
  states->controller->MakeDecision(&config);
  EXPECT_EQ(expected_enable_fec, config.enable_fec);
  EXPECT_EQ(expected_uplink_packet_loss_fraction,
            config.uplink_packet_loss_fraction);
}

}

TEST(FecControllerPlrBasedTest, OutputInitValueBeforeAnyInputsAreReceived) {
  for (bool initial_fec_enabled : {false, true}) {
    auto states = CreateFecControllerPlrBased(initial_fec_enabled);
    CheckDecision(&states, initial_fec_enabled, 0);
  }
}

TEST(FecControllerPlrBasedTest, OutputInitValueWhenUplinkBandwidthUnknown) {

  for (bool initial_fec_enabled : {false, true}) {
    for (float packet_loss :
         {kDisablingPacketLossAtLowBw - kEpsilon, kDisablingPacketLossAtLowBw,
          kDisablingPacketLossAtLowBw + kEpsilon,
          kEnablingPacketLossAtLowBw - kEpsilon, kEnablingPacketLossAtLowBw,
          kEnablingPacketLossAtLowBw + kEpsilon}) {
      auto states = CreateFecControllerPlrBased(initial_fec_enabled);
      UpdateNetworkMetrics(&states, absl::nullopt, packet_loss);
      CheckDecision(&states, initial_fec_enabled, packet_loss);
    }
  }
}

TEST(FecControllerPlrBasedTest,
     OutputInitValueWhenUplinkPacketLossFractionUnknown) {

  for (bool initial_fec_enabled : {false, true}) {
    for (int bandwidth : {kDisablingBandwidthLow - 1, kDisablingBandwidthLow,
                          kDisablingBandwidthLow + 1, kEnablingBandwidthLow - 1,
                          kEnablingBandwidthLow, kEnablingBandwidthLow + 1}) {
      auto states = CreateFecControllerPlrBased(initial_fec_enabled);
      UpdateNetworkMetrics(&states, bandwidth, absl::nullopt);
      CheckDecision(&states, initial_fec_enabled, 0.0);
    }
  }
}

TEST(FecControllerPlrBasedTest, EnableFecForHighBandwidth) {
  auto states = CreateFecControllerPlrBased(false);
  UpdateNetworkMetrics(&states, kEnablingBandwidthHigh,
                       kEnablingPacketLossAtHighBw);
  CheckDecision(&states, true, kEnablingPacketLossAtHighBw);
}

TEST(FecControllerPlrBasedTest, UpdateMultipleNetworkMetricsAtOnce) {

  auto states = CreateFecControllerPlrBased(false);
  Controller::NetworkMetrics network_metrics;
  network_metrics.uplink_bandwidth_bps = kEnablingBandwidthHigh;
  network_metrics.uplink_packet_loss_fraction = kEnablingPacketLossAtHighBw;
  EXPECT_CALL(*states.packet_loss_smoother, GetAverage())
      .WillOnce(Return(kEnablingPacketLossAtHighBw));
  states.controller->UpdateNetworkMetrics(network_metrics);
  CheckDecision(&states, true, kEnablingPacketLossAtHighBw);
}

TEST(FecControllerPlrBasedTest, MaintainFecOffForHighBandwidth) {
  auto states = CreateFecControllerPlrBased(false);
  constexpr float kPacketLoss = kEnablingPacketLossAtHighBw * 0.99f;
  UpdateNetworkMetrics(&states, kEnablingBandwidthHigh, kPacketLoss);
  CheckDecision(&states, false, kPacketLoss);
}

TEST(FecControllerPlrBasedTest, EnableFecForMediumBandwidth) {
  auto states = CreateFecControllerPlrBased(false);
  constexpr float kPacketLoss =
      (kEnablingPacketLossAtLowBw + kEnablingPacketLossAtHighBw) / 2.0;
  UpdateNetworkMetrics(&states,
                       (kEnablingBandwidthHigh + kEnablingBandwidthLow) / 2,
                       kPacketLoss);
  CheckDecision(&states, true, kPacketLoss);
}

TEST(FecControllerPlrBasedTest, MaintainFecOffForMediumBandwidth) {
  auto states = CreateFecControllerPlrBased(false);
  constexpr float kPacketLoss =
      kEnablingPacketLossAtLowBw * 0.49f + kEnablingPacketLossAtHighBw * 0.51f;
  UpdateNetworkMetrics(&states,
                       (kEnablingBandwidthHigh + kEnablingBandwidthLow) / 2,
                       kPacketLoss);
  CheckDecision(&states, false, kPacketLoss);
}

TEST(FecControllerPlrBasedTest, EnableFecForLowBandwidth) {
  auto states = CreateFecControllerPlrBased(false);
  UpdateNetworkMetrics(&states, kEnablingBandwidthLow,
                       kEnablingPacketLossAtLowBw);
  CheckDecision(&states, true, kEnablingPacketLossAtLowBw);
}

TEST(FecControllerPlrBasedTest, MaintainFecOffForLowBandwidth) {
  auto states = CreateFecControllerPlrBased(false);
  constexpr float kPacketLoss = kEnablingPacketLossAtLowBw * 0.99f;
  UpdateNetworkMetrics(&states, kEnablingBandwidthLow, kPacketLoss);
  CheckDecision(&states, false, kPacketLoss);
}

TEST(FecControllerPlrBasedTest, MaintainFecOffForVeryLowBandwidth) {
  auto states = CreateFecControllerPlrBased(false);

  UpdateNetworkMetrics(&states, kEnablingBandwidthLow - 1, 1.0);
  CheckDecision(&states, false, 1.0);
}

TEST(FecControllerPlrBasedTest, DisableFecForHighBandwidth) {
  auto states = CreateFecControllerPlrBased(true);
  constexpr float kPacketLoss = kDisablingPacketLossAtHighBw - kEpsilon;
  UpdateNetworkMetrics(&states, kDisablingBandwidthHigh, kPacketLoss);
  CheckDecision(&states, false, kPacketLoss);
}

TEST(FecControllerPlrBasedTest, MaintainFecOnForHighBandwidth) {

  auto states = CreateFecControllerPlrBased(true);
  UpdateNetworkMetrics(&states, kDisablingBandwidthHigh,
                       kDisablingPacketLossAtHighBw);
  CheckDecision(&states, true, kDisablingPacketLossAtHighBw);
}

TEST(FecControllerPlrBasedTest, DisableFecOnMediumBandwidth) {
  auto states = CreateFecControllerPlrBased(true);
  constexpr float kPacketLoss =
      (kDisablingPacketLossAtLowBw + kDisablingPacketLossAtHighBw) / 2.0f -
      kEpsilon;
  UpdateNetworkMetrics(&states,
                       (kDisablingBandwidthHigh + kDisablingBandwidthLow) / 2,
                       kPacketLoss);
  CheckDecision(&states, false, kPacketLoss);
}

TEST(FecControllerPlrBasedTest, MaintainFecOnForMediumBandwidth) {
  auto states = CreateFecControllerPlrBased(true);
  constexpr float kPacketLoss = kDisablingPacketLossAtLowBw * 0.51f +
                                kDisablingPacketLossAtHighBw * 0.49f - kEpsilon;
  UpdateNetworkMetrics(&states,
                       (kEnablingBandwidthHigh + kDisablingBandwidthLow) / 2,
                       kPacketLoss);
  CheckDecision(&states, true, kPacketLoss);
}

TEST(FecControllerPlrBasedTest, DisableFecForLowBandwidth) {
  auto states = CreateFecControllerPlrBased(true);
  constexpr float kPacketLoss = kDisablingPacketLossAtLowBw - kEpsilon;
  UpdateNetworkMetrics(&states, kDisablingBandwidthLow, kPacketLoss);
  CheckDecision(&states, false, kPacketLoss);
}

TEST(FecControllerPlrBasedTest, DisableFecForVeryLowBandwidth) {
  auto states = CreateFecControllerPlrBased(true);

  UpdateNetworkMetrics(&states, kDisablingBandwidthLow - 1, 1.0);
  CheckDecision(&states, false, 1.0);
}

TEST(FecControllerPlrBasedTest, CheckBehaviorOnChangingNetworkMetrics) {

  auto states = CreateFecControllerPlrBased(true);
  UpdateNetworkMetrics(&states, kDisablingBandwidthLow - 1, 1.0);
  CheckDecision(&states, false, 1.0);

  UpdateNetworkMetrics(&states, kEnablingBandwidthLow,
                       kEnablingPacketLossAtLowBw * 0.99f);
  CheckDecision(&states, false, kEnablingPacketLossAtLowBw * 0.99f);

  UpdateNetworkMetrics(&states, kEnablingBandwidthHigh,
                       kEnablingPacketLossAtHighBw);
  CheckDecision(&states, true, kEnablingPacketLossAtHighBw);

  UpdateNetworkMetrics(&states, kDisablingBandwidthHigh,
                       kDisablingPacketLossAtHighBw);
  CheckDecision(&states, true, kDisablingPacketLossAtHighBw);

  UpdateNetworkMetrics(&states, kDisablingBandwidthHigh + 1, 0.0);
  CheckDecision(&states, false, 0.0);
}

TEST(FecControllerPlrBasedTest, CheckBehaviorOnSpecialCurves) {

  constexpr int kEnablingBandwidthHigh = kEnablingBandwidthLow;
  constexpr float kDisablingPacketLossAtLowBw = kDisablingPacketLossAtHighBw;
  FecControllerPlrBasedTestStates states;
  std::unique_ptr<MockSmoothingFilter> mock_smoothing_filter(
      new NiceMock<MockSmoothingFilter>());
  states.packet_loss_smoother = mock_smoothing_filter.get();
  states.controller.reset(new FecControllerPlrBased(
      FecControllerPlrBased::Config(
          true,
          ThresholdCurve(kEnablingBandwidthLow, kEnablingPacketLossAtLowBw,
                         kEnablingBandwidthHigh, kEnablingPacketLossAtHighBw),
          ThresholdCurve(kDisablingBandwidthLow, kDisablingPacketLossAtLowBw,
                         kDisablingBandwidthHigh, kDisablingPacketLossAtHighBw),
          0),
      std::move(mock_smoothing_filter)));

  UpdateNetworkMetrics(&states, kDisablingBandwidthLow - 1, 1.0);
  CheckDecision(&states, false, 1.0);

  UpdateNetworkMetrics(&states, kEnablingBandwidthLow,
                       kEnablingPacketLossAtHighBw * 0.99f);
  CheckDecision(&states, false, kEnablingPacketLossAtHighBw * 0.99f);

  UpdateNetworkMetrics(&states, kEnablingBandwidthHigh,
                       kEnablingPacketLossAtHighBw);
  CheckDecision(&states, true, kEnablingPacketLossAtHighBw);

  UpdateNetworkMetrics(&states, kDisablingBandwidthHigh,
                       kDisablingPacketLossAtHighBw);
  CheckDecision(&states, true, kDisablingPacketLossAtHighBw);

  UpdateNetworkMetrics(&states, kDisablingBandwidthHigh + 1, 0.0);
  CheckDecision(&states, false, 0.0);
}

TEST(FecControllerPlrBasedTest, SingleThresholdCurveForEnablingAndDisabling) {

  constexpr int kBandwidthLow = 10000;
  constexpr float kPacketLossAtLowBw = 0.25f;
  constexpr int kBandwidthHigh = 20000;
  constexpr float kPacketLossAtHighBw = 0.125f;
  auto curve = ThresholdCurve(kBandwidthLow, kPacketLossAtLowBw, kBandwidthHigh,
                              kPacketLossAtHighBw);

  struct NetworkState {
    int bandwidth;
    float packet_loss;
  };

  std::vector<NetworkState> below{
      {kBandwidthLow - 1, kPacketLossAtLowBw + 0.1f},
      {(kBandwidthLow + kBandwidthHigh) / 2,
       (kPacketLossAtLowBw + kPacketLossAtHighBw) / 2 - kEpsilon},
      {kBandwidthHigh + 1, kPacketLossAtHighBw - kEpsilon}
  };

  std::vector<NetworkState> on{
      {kBandwidthLow, kPacketLossAtLowBw + 0.1f},
      {kBandwidthLow, kPacketLossAtLowBw},
      {(kBandwidthLow + kBandwidthHigh) / 2,
       (kPacketLossAtLowBw + kPacketLossAtHighBw) / 2},
      {kBandwidthHigh, kPacketLossAtHighBw},
      {kBandwidthHigh + 1, kPacketLossAtHighBw},
  };

  std::vector<NetworkState> above{
      {(kBandwidthLow + kBandwidthHigh) / 2,
       (kPacketLossAtLowBw + kPacketLossAtHighBw) / 2 + kEpsilon},
      {kBandwidthHigh + 1, kPacketLossAtHighBw + kEpsilon},
  };

  for (NetworkState net_state : below) {
    for (bool initial_fec_enabled : {false, true}) {
      auto states =
          CreateFecControllerPlrBased(initial_fec_enabled, curve, curve);
      UpdateNetworkMetrics(&states, net_state.bandwidth, net_state.packet_loss);
      CheckDecision(&states, false, net_state.packet_loss);
    }
  }

  for (const std::vector<NetworkState>& states_list : {on, above}) {
    for (NetworkState net_state : states_list) {
      for (bool initial_fec_enabled : {false, true}) {
        auto states =
            CreateFecControllerPlrBased(initial_fec_enabled, curve, curve);
        UpdateNetworkMetrics(&states, net_state.bandwidth,
                             net_state.packet_loss);
        CheckDecision(&states, true, net_state.packet_loss);
      }
    }
  }
}

TEST(FecControllerPlrBasedTest, FecAlwaysOff) {
  ThresholdCurve always_off_curve(0, 1.0f + kEpsilon, 0, 1.0f + kEpsilon);
  for (bool initial_fec_enabled : {false, true}) {
    for (int bandwidth : {0, 10000}) {
      for (float packet_loss : {0.0f, 0.5f, 1.0f}) {
        auto states = CreateFecControllerPlrBased(
            initial_fec_enabled, always_off_curve, always_off_curve);
        UpdateNetworkMetrics(&states, bandwidth, packet_loss);
        CheckDecision(&states, false, packet_loss);
      }
    }
  }
}

TEST(FecControllerPlrBasedTest, FecAlwaysOn) {
  ThresholdCurve always_on_curve(0, 0.0f, 0, 0.0f);
  for (bool initial_fec_enabled : {false, true}) {
    for (int bandwidth : {0, 10000}) {
      for (float packet_loss : {0.0f, 0.5f, 1.0f}) {
        auto states = CreateFecControllerPlrBased(
            initial_fec_enabled, always_on_curve, always_on_curve);
        UpdateNetworkMetrics(&states, bandwidth, packet_loss);
        CheckDecision(&states, true, packet_loss);
      }
    }
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(FecControllerPlrBasedDeathTest, InvalidConfig) {
  FecControllerPlrBasedTestStates states;
  std::unique_ptr<MockSmoothingFilter> mock_smoothing_filter(
      new NiceMock<MockSmoothingFilter>());
  states.packet_loss_smoother = mock_smoothing_filter.get();
  EXPECT_DEATH(
      states.controller.reset(new FecControllerPlrBased(
          FecControllerPlrBased::Config(
              true,
              ThresholdCurve(kDisablingBandwidthLow - 1,
                             kEnablingPacketLossAtLowBw, kEnablingBandwidthHigh,
                             kEnablingPacketLossAtHighBw),
              ThresholdCurve(
                  kDisablingBandwidthLow, kDisablingPacketLossAtLowBw,
                  kDisablingBandwidthHigh, kDisablingPacketLossAtHighBw),
              0),
          std::move(mock_smoothing_filter))),
      "Check failed");
}
#endif

}
