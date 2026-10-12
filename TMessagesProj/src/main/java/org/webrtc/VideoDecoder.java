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

public interface VideoDecoder {

  public class Settings {
    public final int numberOfCores;
    public final int width;
    public final int height;

    @CalledByNative("Settings")
    public Settings(int numberOfCores, int width, int height) {
      this.numberOfCores = numberOfCores;
      this.width = width;
      this.height = height;
    }
  }

  public class DecodeInfo {
    public final boolean isMissingFrames;
    public final long renderTimeMs;

    public DecodeInfo(boolean isMissingFrames, long renderTimeMs) {
      this.isMissingFrames = isMissingFrames;
      this.renderTimeMs = renderTimeMs;
    }
  }

  public interface Callback {

    void onDecodedFrame(VideoFrame frame, Integer decodeTimeMs, Integer qp);
  }

  @CalledByNative
  default long createNative(long webrtcEnvRef) {
    return 0;
  }

  @CalledByNative VideoCodecStatus initDecode(Settings settings, Callback decodeCallback);

  @CalledByNative VideoCodecStatus release();

  @CalledByNative VideoCodecStatus decode(EncodedImage frame, DecodeInfo info);

  @CalledByNative String getImplementationName();
}
