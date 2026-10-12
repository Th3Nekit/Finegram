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

public final class CryptoOptions {

  public final class Srtp {

    private final boolean enableGcmCryptoSuites;

    private final boolean enableAes128Sha1_32CryptoCipher;

    private final boolean enableEncryptedRtpHeaderExtensions;

    private Srtp(boolean enableGcmCryptoSuites, boolean enableAes128Sha1_32CryptoCipher,
        boolean enableEncryptedRtpHeaderExtensions) {
      this.enableGcmCryptoSuites = enableGcmCryptoSuites;
      this.enableAes128Sha1_32CryptoCipher = enableAes128Sha1_32CryptoCipher;
      this.enableEncryptedRtpHeaderExtensions = enableEncryptedRtpHeaderExtensions;
    }

    @CalledByNative("Srtp")
    public boolean getEnableGcmCryptoSuites() {
      return enableGcmCryptoSuites;
    }

    @CalledByNative("Srtp")
    public boolean getEnableAes128Sha1_32CryptoCipher() {
      return enableAes128Sha1_32CryptoCipher;
    }

    @CalledByNative("Srtp")
    public boolean getEnableEncryptedRtpHeaderExtensions() {
      return enableEncryptedRtpHeaderExtensions;
    }
  }

  public final class SFrame {

    private final boolean requireFrameEncryption;

    private SFrame(boolean requireFrameEncryption) {
      this.requireFrameEncryption = requireFrameEncryption;
    }

    @CalledByNative("SFrame")
    public boolean getRequireFrameEncryption() {
      return requireFrameEncryption;
    }
  }

  private final Srtp srtp;
  private final SFrame sframe;

  private CryptoOptions(boolean enableGcmCryptoSuites, boolean enableAes128Sha1_32CryptoCipher,
      boolean enableEncryptedRtpHeaderExtensions, boolean requireFrameEncryption) {
    this.srtp = new Srtp(
        enableGcmCryptoSuites, enableAes128Sha1_32CryptoCipher, enableEncryptedRtpHeaderExtensions);
    this.sframe = new SFrame(requireFrameEncryption);
  }

  public static Builder builder() {
    return new Builder();
  }

  @CalledByNative
  public Srtp getSrtp() {
    return srtp;
  }

  @CalledByNative
  public SFrame getSFrame() {
    return sframe;
  }

  public static class Builder {
    private boolean enableGcmCryptoSuites;
    private boolean enableAes128Sha1_32CryptoCipher;
    private boolean enableEncryptedRtpHeaderExtensions;
    private boolean requireFrameEncryption;

    private Builder() {}

    public Builder setEnableGcmCryptoSuites(boolean enableGcmCryptoSuites) {
      this.enableGcmCryptoSuites = enableGcmCryptoSuites;
      return this;
    }

    public Builder setEnableAes128Sha1_32CryptoCipher(boolean enableAes128Sha1_32CryptoCipher) {
      this.enableAes128Sha1_32CryptoCipher = enableAes128Sha1_32CryptoCipher;
      return this;
    }

    public Builder setEnableEncryptedRtpHeaderExtensions(
        boolean enableEncryptedRtpHeaderExtensions) {
      this.enableEncryptedRtpHeaderExtensions = enableEncryptedRtpHeaderExtensions;
      return this;
    }

    public Builder setRequireFrameEncryption(boolean requireFrameEncryption) {
      this.requireFrameEncryption = requireFrameEncryption;
      return this;
    }

    public CryptoOptions createCryptoOptions() {
      return new CryptoOptions(enableGcmCryptoSuites, enableAes128Sha1_32CryptoCipher,
          enableEncryptedRtpHeaderExtensions, requireFrameEncryption);
    }
  }
}
