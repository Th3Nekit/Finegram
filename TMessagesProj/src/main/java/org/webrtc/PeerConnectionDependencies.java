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

import androidx.annotation.Nullable;

public final class PeerConnectionDependencies {

  private final PeerConnection.Observer observer;

  private final SSLCertificateVerifier sslCertificateVerifier;

  public static class Builder {
    private PeerConnection.Observer observer;
    private SSLCertificateVerifier sslCertificateVerifier;

    private Builder(PeerConnection.Observer observer) {
      this.observer = observer;
    }

    public Builder setSSLCertificateVerifier(SSLCertificateVerifier sslCertificateVerifier) {
      this.sslCertificateVerifier = sslCertificateVerifier;
      return this;
    }

    public PeerConnectionDependencies createPeerConnectionDependencies() {
      return new PeerConnectionDependencies(observer, sslCertificateVerifier);
    }
  }

  public static Builder builder(PeerConnection.Observer observer) {
    return new Builder(observer);
  }

  PeerConnection.Observer getObserver() {
    return observer;
  }

  @Nullable
  SSLCertificateVerifier getSSLCertificateVerifier() {
    return sslCertificateVerifier;
  }

  private PeerConnectionDependencies(
      PeerConnection.Observer observer, SSLCertificateVerifier sslCertificateVerifier) {
    this.observer = observer;
    this.sslCertificateVerifier = sslCertificateVerifier;
  }
}
