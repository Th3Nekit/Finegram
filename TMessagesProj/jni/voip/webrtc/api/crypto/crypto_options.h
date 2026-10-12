/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_CRYPTO_CRYPTO_OPTIONS_H_
#define API_CRYPTO_CRYPTO_OPTIONS_H_

#include <vector>

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct RTC_EXPORT CryptoOptions {
  CryptoOptions();
  CryptoOptions(const CryptoOptions& other);
  ~CryptoOptions();

  static CryptoOptions NoGcm();

  std::vector<int> GetSupportedDtlsSrtpCryptoSuites() const;

  bool operator==(const CryptoOptions& other) const;
  bool operator!=(const CryptoOptions& other) const;

  struct Srtp {

    bool enable_gcm_crypto_suites = true;

    bool enable_aes128_sha1_32_crypto_cipher = false;

    bool enable_aes128_sha1_80_crypto_cipher = true;

    bool enable_encrypted_rtp_header_extensions = false;
  } srtp;

  struct SFrame {

    bool require_frame_encryption = false;
  } sframe;
};

}

#endif
