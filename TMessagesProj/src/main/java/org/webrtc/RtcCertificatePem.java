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

import org.webrtc.PeerConnection;

public class RtcCertificatePem {

  public final String privateKey;

  public final String certificate;

  private static final long DEFAULT_EXPIRY = 60 * 60 * 24 * 30;

  @CalledByNative
  public RtcCertificatePem(String privateKey, String certificate) {
    this.privateKey = privateKey;
    this.certificate = certificate;
  }

  @CalledByNative
  String getPrivateKey() {
    return privateKey;
  }

  @CalledByNative
  String getCertificate() {
    return certificate;
  }

  public static RtcCertificatePem generateCertificate() {
    return nativeGenerateCertificate(PeerConnection.KeyType.ECDSA, DEFAULT_EXPIRY);
  }

  public static RtcCertificatePem generateCertificate(PeerConnection.KeyType keyType) {
    return nativeGenerateCertificate(keyType, DEFAULT_EXPIRY);
  }

  public static RtcCertificatePem generateCertificate(long expires) {
    return nativeGenerateCertificate(PeerConnection.KeyType.ECDSA, expires);
  }

  public static RtcCertificatePem generateCertificate(
      PeerConnection.KeyType keyType, long expires) {
    return nativeGenerateCertificate(keyType, expires);
  }

  private static native RtcCertificatePem nativeGenerateCertificate(
      PeerConnection.KeyType keyType, long expires);
}
