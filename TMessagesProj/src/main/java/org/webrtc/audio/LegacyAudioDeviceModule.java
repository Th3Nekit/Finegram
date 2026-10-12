/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.audio;

import org.webrtc.voiceengine.WebRtcAudioRecord;
import org.webrtc.voiceengine.WebRtcAudioTrack;

@Deprecated
public class LegacyAudioDeviceModule implements AudioDeviceModule {
  @Override
  public long getNativeAudioDeviceModulePointer() {

    return 0;
  }

  @Override
  public void release() {

  }

  @Override
  public void setSpeakerMute(boolean mute) {
    WebRtcAudioTrack.setSpeakerMute(mute);
  }

  @Override
  public void setMicrophoneMute(boolean mute) {
    WebRtcAudioRecord.setMicrophoneMute(mute);
  }
}
