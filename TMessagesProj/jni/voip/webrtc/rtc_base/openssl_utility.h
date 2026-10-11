/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_OPENSSL_UTILITY_H_
#define RTC_BASE_OPENSSL_UTILITY_H_

#include <openssl/ossl_typ.h>

#include <string>

#include "absl/strings/string_view.h"

namespace rtc {

namespace openssl {

#ifdef OPENSSL_IS_BORINGSSL

bool ParseCertificate(CRYPTO_BUFFER* cert_buffer,
                      CBS* signature_algorithm_oid,
                      int64_t* expiration_time);
#endif

bool VerifyPeerCertMatchesHost(SSL* ssl, absl::string_view host);

void LogSSLErrors(absl::string_view prefix);

#ifndef WEBRTC_EXCLUDE_BUILT_IN_SSL_ROOT_CERTS

bool LoadBuiltinSSLRootCertificates(SSL_CTX* ssl_ctx);
#endif

#ifdef OPENSSL_IS_BORINGSSL
CRYPTO_BUFFER_POOL* GetBufferPool();
#endif

}
}

#endif
