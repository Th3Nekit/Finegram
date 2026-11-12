/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.util.HashMap;
import java.util.Map;

public class Metrics {
  private static final String TAG = "Metrics";

  public final Map<String, HistogramInfo> map =
      new HashMap<String, HistogramInfo>();

  @CalledByNative
  Metrics() {}

  public static class HistogramInfo {
    public final int min;
    public final int max;
    public final int bucketCount;
    public final Map<Integer, Integer> samples =
        new HashMap<Integer, Integer>();

    @CalledByNative("HistogramInfo")
    public HistogramInfo(int min, int max, int bucketCount) {
      this.min = min;
      this.max = max;
      this.bucketCount = bucketCount;
    }

    @CalledByNative("HistogramInfo")
    public void addSample(int value, int numEvents) {
      samples.put(value, numEvents);
    }
  }

  @CalledByNative
  private void add(String name, HistogramInfo info) {
    map.put(name, info);
  }

  public static void enable() {
    nativeEnable();
  }

  public static Metrics getAndReset() {
    return nativeGetAndReset();
  }

  private static native void nativeEnable();
  private static native Metrics nativeGetAndReset();
}
