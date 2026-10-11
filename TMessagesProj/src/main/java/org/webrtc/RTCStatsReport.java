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

import java.util.Map;

public class RTCStatsReport {
  private final long timestampUs;
  private final Map<String, RTCStats> stats;

  public RTCStatsReport(long timestampUs, Map<String, RTCStats> stats) {
    this.timestampUs = timestampUs;
    this.stats = stats;
  }

  public double getTimestampUs() {
    return timestampUs;
  }

  public Map<String, RTCStats> getStatsMap() {
    return stats;
  }

  @Override
  public String toString() {
    StringBuilder builder = new StringBuilder();
    builder.append("{ timestampUs: ").append(timestampUs).append(", stats: [\n");
    boolean first = true;
    for (RTCStats stat : stats.values()) {
      if (!first) {
        builder.append(",\n");
      }
      builder.append(stat);
      first = false;
    }
    builder.append(" ] }");
    return builder.toString();
  }

  @SuppressWarnings("unchecked")
  @CalledByNative
  private static RTCStatsReport create(long timestampUs, Map stats) {
    return new RTCStatsReport(timestampUs, stats);
  }
}
