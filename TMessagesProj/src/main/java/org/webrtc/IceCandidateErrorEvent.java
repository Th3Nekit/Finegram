/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

public final class IceCandidateErrorEvent {

  public final String address;

  public final int port;

  public final String url;

  public final int errorCode;

  public final String errorText;

  @CalledByNative
  public IceCandidateErrorEvent(
      String address, int port, String url, int errorCode, String errorText) {
    this.address = address;
    this.port = port;
    this.url = url;
    this.errorCode = errorCode;
    this.errorText = errorText;
  }
}
