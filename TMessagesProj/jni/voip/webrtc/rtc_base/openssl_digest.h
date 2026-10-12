/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_OPENSSL_DIGEST_H_
#define RTC_BASE_OPENSSL_DIGEST_H_

#include <openssl/ossl_typ.h>
#include <stddef.h>

#include <string>

#include "absl/strings/string_view.h"
#include "rtc_base/message_digest.h"

namespace rtc {

class OpenSSLDigest final : public MessageDigest {
 public:

  explicit OpenSSLDigest(absl::string_view algorithm);
  ~OpenSSLDigest() override;

  size_t Size() const override;

  void Update(const void* buf, size_t len) override;

  size_t Finish(void* buf, size_t len) override;

  static bool GetDigestEVP(absl::string_view algorithm, const EVP_MD** md);

  static bool GetDigestName(const EVP_MD* md, std::string* algorithm);

  static bool GetDigestSize(absl::string_view algorithm, size_t* len);

 private:
  EVP_MD_CTX* ctx_ = nullptr;
  const EVP_MD* md_;
};

}

#endif
