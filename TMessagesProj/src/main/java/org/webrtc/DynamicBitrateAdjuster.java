/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

class DynamicBitrateAdjuster extends BaseBitrateAdjuster {

  private static final double BITRATE_ADJUSTMENT_SEC = 3.0;

  private static final double BITRATE_ADJUSTMENT_MAX_SCALE = 4;

  private static final int BITRATE_ADJUSTMENT_STEPS = 20;

  private static final double BITS_PER_BYTE = 8.0;

  private double deviationBytes;
  private double timeSinceLastAdjustmentMs;
  private int bitrateAdjustmentScaleExp;

  @Override
  public void setTargets(int targetBitrateBps, double targetFramerateFps) {
    if (this.targetBitrateBps > 0 && targetBitrateBps < this.targetBitrateBps) {

      deviationBytes = deviationBytes * targetBitrateBps / this.targetBitrateBps;
    }
    super.setTargets(targetBitrateBps, targetFramerateFps);
  }

  @Override
  public void reportEncodedFrame(int size) {
    if (targetFramerateFps == 0) {
      return;
    }

    double expectedBytesPerFrame = (targetBitrateBps / BITS_PER_BYTE) / targetFramerateFps;
    deviationBytes += (size - expectedBytesPerFrame);
    timeSinceLastAdjustmentMs += 1000.0 / targetFramerateFps;

    double deviationThresholdBytes = targetBitrateBps / BITS_PER_BYTE;

    double deviationCap = BITRATE_ADJUSTMENT_SEC * deviationThresholdBytes;
    deviationBytes = Math.min(deviationBytes, deviationCap);
    deviationBytes = Math.max(deviationBytes, -deviationCap);

    if (timeSinceLastAdjustmentMs <= 1000 * BITRATE_ADJUSTMENT_SEC) {
      return;
    }

    if (deviationBytes > deviationThresholdBytes) {

      int bitrateAdjustmentInc = (int) (deviationBytes / deviationThresholdBytes + 0.5);
      bitrateAdjustmentScaleExp -= bitrateAdjustmentInc;

      bitrateAdjustmentScaleExp = Math.max(bitrateAdjustmentScaleExp, -BITRATE_ADJUSTMENT_STEPS);
      deviationBytes = deviationThresholdBytes;
    } else if (deviationBytes < -deviationThresholdBytes) {

      int bitrateAdjustmentInc = (int) (-deviationBytes / deviationThresholdBytes + 0.5);
      bitrateAdjustmentScaleExp += bitrateAdjustmentInc;

      bitrateAdjustmentScaleExp = Math.min(bitrateAdjustmentScaleExp, BITRATE_ADJUSTMENT_STEPS);
      deviationBytes = -deviationThresholdBytes;
    }
    timeSinceLastAdjustmentMs = 0;
  }

  private double getBitrateAdjustmentScale() {
    return Math.pow(BITRATE_ADJUSTMENT_MAX_SCALE,
        (double) bitrateAdjustmentScaleExp / BITRATE_ADJUSTMENT_STEPS);
  }

  @Override
  public int getAdjustedBitrateBps() {
    return (int) (targetBitrateBps * getBitrateAdjustmentScale());
  }
}
