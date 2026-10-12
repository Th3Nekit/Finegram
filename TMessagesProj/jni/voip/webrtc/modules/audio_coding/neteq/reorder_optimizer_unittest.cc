/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/reorder_optimizer.h"

#include "test/gtest.h"

namespace webrtc {

namespace {

constexpr int kForgetFactor = 32745;
constexpr int kMsPerLossPercent = 20;
constexpr int kStartForgetWeight = 1;

}

TEST(ReorderOptimizerTest, OnlyIncreaseDelayForReorderedPackets) {
  ReorderOptimizer reorder_optimizer(kForgetFactor, kMsPerLossPercent,
                                     kStartForgetWeight);
  EXPECT_FALSE(reorder_optimizer.GetOptimalDelayMs());

  reorder_optimizer.Update(60,               false, 0);
  EXPECT_EQ(reorder_optimizer.GetOptimalDelayMs(), 20);

  reorder_optimizer.Update(100,               false, 0);
  EXPECT_EQ(reorder_optimizer.GetOptimalDelayMs(), 20);

  reorder_optimizer.Update(80,               true, 0);
  EXPECT_EQ(reorder_optimizer.GetOptimalDelayMs(), 100);
}

TEST(ReorderOptimizerTest, AvoidIncreasingDelayWhenProbabilityIsLow) {
  ReorderOptimizer reorder_optimizer(kForgetFactor, kMsPerLossPercent,
                                     kStartForgetWeight);

  reorder_optimizer.Update(40,               true, 0);
  reorder_optimizer.Update(40,               true, 0);
  reorder_optimizer.Update(40,               true, 0);
  EXPECT_EQ(reorder_optimizer.GetOptimalDelayMs(), 60);

  reorder_optimizer.Update(600,               true, 0);
  EXPECT_EQ(reorder_optimizer.GetOptimalDelayMs(), 60);
}

TEST(ReorderOptimizerTest, BaseDelayIsSubtractedFromCost) {
  constexpr int kBaseDelayMs = 200;
  ReorderOptimizer reorder_optimizer(kForgetFactor, kMsPerLossPercent,
                                     kStartForgetWeight);

  reorder_optimizer.Update(40,               true, kBaseDelayMs);
  reorder_optimizer.Update(40,               true, kBaseDelayMs);
  reorder_optimizer.Update(40,               true, kBaseDelayMs);
  EXPECT_EQ(reorder_optimizer.GetOptimalDelayMs(), 60);

  reorder_optimizer.Update(600,               true, kBaseDelayMs);
  EXPECT_EQ(reorder_optimizer.GetOptimalDelayMs(), 620);
}

}
