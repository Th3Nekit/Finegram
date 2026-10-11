/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_OPENSSL_SESSION_CACHE_H_
#define RTC_BASE_OPENSSL_SESSION_CACHE_H_

#include <openssl/ossl_typ.h>

#include <map>
#include <string>

#include "absl/strings/string_view.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/string_utils.h"

#ifndef OPENSSL_IS_BORINGSSL
typedef struct ssl_session_st SSL_SESSION;
#endif

namespace rtc {

class OpenSSLSessionCache final {
 public:

  OpenSSLSessionCache(SSLMode ssl_mode, SSL_CTX* ssl_ctx);

  ~OpenSSLSessionCache();

  OpenSSLSessionCache(const OpenSSLSessionCache&) = delete;
  OpenSSLSessionCache& operator=(const OpenSSLSessionCache&) = delete;

  SSL_SESSION* LookupSession(absl::string_view hostname) const;

  void AddSession(absl::string_view hostname, SSL_SESSION* session);

  SSL_CTX* GetSSLContext() const;

  SSLMode GetSSLMode() const;

 private:

  const SSLMode ssl_mode_;

  SSL_CTX* ssl_ctx_ = nullptr;

  std::map<std::string, SSL_SESSION*, rtc::AbslStringViewCmp> sessions_;

};

}

#endif
