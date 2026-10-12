// Copyright 2015 The Chromium Authors
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

#ifndef BSSL_PKI_VERIFY_SIGNED_DATA_H_
#define BSSL_PKI_VERIFY_SIGNED_DATA_H_

#include <openssl/base.h>
#include <openssl/evp.h>
#include <openssl/pki/signature_verify_cache.h>

#include "signature_algorithm.h"

BSSL_NAMESPACE_BEGIN

namespace der {
class BitString;
class Input;
}

[[nodiscard]] OPENSSL_EXPORT bool VerifySignedData(
    SignatureAlgorithm algorithm, der::Input signed_data,
    const der::BitString &signature_value, EVP_PKEY *public_key,
    SignatureVerifyCache *cache);

[[nodiscard]] OPENSSL_EXPORT bool VerifySignedData(
    SignatureAlgorithm algorithm, der::Input signed_data,
    const der::BitString &signature_value, der::Input public_key_spki,
    SignatureVerifyCache *cache);

[[nodiscard]] OPENSSL_EXPORT bool ParsePublicKey(
    der::Input public_key_spki, bssl::UniquePtr<EVP_PKEY> *public_key);

BSSL_NAMESPACE_END

#endif
