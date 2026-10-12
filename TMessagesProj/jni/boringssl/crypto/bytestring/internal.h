// Copyright 2014 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_BYTESTRING_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_BYTESTRING_INTERNAL_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT int CBS_asn1_ber_to_der(CBS *in, CBS *out,
                                       uint8_t **out_storage);

OPENSSL_EXPORT int CBS_get_asn1_implicit_string(CBS *in, CBS *out,
                                                uint8_t **out_storage,
                                                CBS_ASN1_TAG outer_tag,
                                                CBS_ASN1_TAG inner_tag);

int CBB_finish_i2d(CBB *cbb, uint8_t **outp);

#if defined(__cplusplus)
}
#endif

#endif
