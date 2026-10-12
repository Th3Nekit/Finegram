/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.audio;

import android.media.AudioTrack;
import android.os.Build;
import org.webrtc.Logging;

class LowLatencyAudioBufferManager {
  private static final String TAG = "LowLatencyAudioBufferManager";

  private int prevUnderrunCount;

  private int ticksUntilNextDecrease;

  private boolean keepLoweringBufferSize;

  private int bufferIncreaseCounter;

  public LowLatencyAudioBufferManager() {
    this.prevUnderrunCount = 0;
    this.ticksUntilNextDecrease = 10;
    this.keepLoweringBufferSize = true;
    this.bufferIncreaseCounter = 0;
  }

  public void maybeAdjustBufferSize(AudioTrack audioTrack) {
    if (Build.VERSION.SDK_INT >= 26) {
      final int underrunCount = audioTrack.getUnderrunCount();
      if (underrunCount > prevUnderrunCount) {

        if (bufferIncreaseCounter < 5) {

          final int currentBufferSize = audioTrack.getBufferSizeInFrames();
          final int newBufferSize = currentBufferSize + audioTrack.getPlaybackRate() / 100;
          Logging.d(TAG,
              "Underrun detected! Increasing AudioTrack buffer size from " + currentBufferSize
                  + " to " + newBufferSize);
          audioTrack.setBufferSizeInFrames(newBufferSize);
          bufferIncreaseCounter++;
        }

        keepLoweringBufferSize = false;
        prevUnderrunCount = underrunCount;
        ticksUntilNextDecrease = 10;
      } else if (keepLoweringBufferSize) {
        ticksUntilNextDecrease--;
        if (ticksUntilNextDecrease <= 0) {

          final int bufferSize10ms = audioTrack.getPlaybackRate() / 100;

          final int currentBufferSize = audioTrack.getBufferSizeInFrames();
          final int newBufferSize = Math.max(bufferSize10ms, currentBufferSize - bufferSize10ms);
          if (newBufferSize != currentBufferSize) {
            Logging.d(TAG,
                "Lowering AudioTrack buffer size from " + currentBufferSize + " to "
                    + newBufferSize);
            audioTrack.setBufferSizeInFrames(newBufferSize);
          }
          ticksUntilNextDecrease = 10;
        }
      }
    }
  }
}
