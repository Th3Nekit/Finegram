// Copyright 2017 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OPENSSL_HEADER_CRYPTO_PKCS7_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_PKCS7_INTERNAL_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

int pkcs7_parse_header(uint8_t **der_bytes, CBS *out, CBS *cbs);

int pkcs7_add_signed_data(CBB *out,
                          int (*digest_algos_cb)(CBB *out, const void *arg),
                          int (*cert_crl_cb)(CBB *out, const void *arg),
                          int (*signer_infos_cb)(CBB *out, const void *arg),
                          const void *arg);

#if defined(__cplusplus)
}
#endif

#endif
