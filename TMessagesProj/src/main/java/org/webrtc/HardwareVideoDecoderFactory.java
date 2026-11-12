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

import android.media.MediaCodecInfo;
import androidx.annotation.Nullable;
import java.util.Arrays;

public class HardwareVideoDecoderFactory extends MediaCodecVideoDecoderFactory {
  private final static Predicate<MediaCodecInfo> defaultAllowedPredicate =
      new Predicate<MediaCodecInfo>() {
        @Override
        public boolean test(MediaCodecInfo arg) {
          return MediaCodecUtils.isHardwareAccelerated(arg);
        }
      };

  @Deprecated
  public HardwareVideoDecoderFactory() {
    this(null);
  }

  public HardwareVideoDecoderFactory(@Nullable EglBase.Context sharedContext) {
    this(sharedContext,                              null);
  }

  public HardwareVideoDecoderFactory(@Nullable EglBase.Context sharedContext,
      @Nullable Predicate<MediaCodecInfo> codecAllowedPredicate) {
    super(sharedContext,
        (codecAllowedPredicate == null ? defaultAllowedPredicate
                                       : codecAllowedPredicate.and(defaultAllowedPredicate)));
  }
}
