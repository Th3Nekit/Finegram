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

class Histogram {
  private final long handle;

  private Histogram(long handle) {
    this.handle = handle;
  }

  static public Histogram createCounts(String name, int min, int max, int bucketCount) {
    return new Histogram(nativeCreateCounts(name, min, max, bucketCount));
  }

  static public Histogram createEnumeration(String name, int max) {
    return new Histogram(nativeCreateEnumeration(name, max));
  }

  public void addSample(int sample) {
    nativeAddSample(handle, sample);
  }

  private static native long nativeCreateCounts(String name, int min, int max, int bucketCount);
  private static native long nativeCreateEnumeration(String name, int max);
  private static native void nativeAddSample(long handle, int sample);
}
