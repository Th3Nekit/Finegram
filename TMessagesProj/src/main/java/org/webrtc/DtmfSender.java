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

public class DtmfSender {
  private long nativeDtmfSender;

  public DtmfSender(long nativeDtmfSender) {
    this.nativeDtmfSender = nativeDtmfSender;
  }

  public boolean canInsertDtmf() {
    checkDtmfSenderExists();
    return nativeCanInsertDtmf(nativeDtmfSender);
  }

  public boolean insertDtmf(String tones, int duration, int interToneGap) {
    checkDtmfSenderExists();
    return nativeInsertDtmf(nativeDtmfSender, tones, duration, interToneGap);
  }

  public String tones() {
    checkDtmfSenderExists();
    return nativeTones(nativeDtmfSender);
  }

  public int duration() {
    checkDtmfSenderExists();
    return nativeDuration(nativeDtmfSender);
  }

  public int interToneGap() {
    checkDtmfSenderExists();
    return nativeInterToneGap(nativeDtmfSender);
  }

  public void dispose() {
    checkDtmfSenderExists();
    JniCommon.nativeReleaseRef(nativeDtmfSender);
    nativeDtmfSender = 0;
  }

  private void checkDtmfSenderExists() {
    if (nativeDtmfSender == 0) {
      throw new IllegalStateException("DtmfSender has been disposed.");
    }
  }

  private static native boolean nativeCanInsertDtmf(long dtmfSender);
  private static native boolean nativeInsertDtmf(
      long dtmfSender, String tones, int duration, int interToneGap);
  private static native String nativeTones(long dtmfSender);
  private static native int nativeDuration(long dtmfSender);
  private static native int nativeInterToneGap(long dtmfSender);
};
