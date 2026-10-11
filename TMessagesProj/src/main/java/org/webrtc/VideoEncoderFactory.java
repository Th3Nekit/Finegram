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

import androidx.annotation.Nullable;

public interface VideoEncoderFactory {
  public interface VideoEncoderSelector {

    @CalledByNative("VideoEncoderSelector") void onCurrentEncoder(VideoCodecInfo info);

    @Nullable @CalledByNative("VideoEncoderSelector") VideoCodecInfo onAvailableBitrate(int kbps);

    @Nullable
    @CalledByNative("VideoEncoderSelector")
    default VideoCodecInfo onResolutionChange(int widht, int height) {
      return null;
    }

    @Nullable @CalledByNative("VideoEncoderSelector") VideoCodecInfo onEncoderBroken();
  }

  @Nullable @CalledByNative VideoEncoder createEncoder(VideoCodecInfo info);

  @CalledByNative VideoCodecInfo[] getSupportedCodecs();

  @CalledByNative
  default VideoCodecInfo[] getImplementations() {
    return getSupportedCodecs();
  }

  @CalledByNative
  default VideoEncoderSelector getEncoderSelector() {
    return null;
  }
}
