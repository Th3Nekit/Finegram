/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

public class TimestampAligner {

  public static long getRtcTimeNanos() {
    return nativeRtcTimeNanos();
  }

  private volatile long nativeTimestampAligner = nativeCreateTimestampAligner();

  public long translateTimestamp(long cameraTimeNs) {
    checkNativeAlignerExists();
    return nativeTranslateTimestamp(nativeTimestampAligner, cameraTimeNs);
  }

  public void dispose() {
    checkNativeAlignerExists();
    nativeReleaseTimestampAligner(nativeTimestampAligner);
    nativeTimestampAligner = 0;
  }

  private void checkNativeAlignerExists() {
    if (nativeTimestampAligner == 0) {
      throw new IllegalStateException("TimestampAligner has been disposed.");
    }
  }

  private static native long nativeRtcTimeNanos();
  private static native long nativeCreateTimestampAligner();
  private static native void nativeReleaseTimestampAligner(long timestampAligner);
  private static native long nativeTranslateTimestamp(long timestampAligner, long cameraTimeNs);
}
