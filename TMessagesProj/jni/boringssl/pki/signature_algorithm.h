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

#ifndef BSSL_PKI_SIGNATURE_ALGORITHM_H_
#define BSSL_PKI_SIGNATURE_ALGORITHM_H_

#include <stdint.h>

#include <optional>

#include <openssl/base.h>
#include <openssl/evp.h>

BSSL_NAMESPACE_BEGIN

namespace der {
class Input;
}

enum class DigestAlgorithm {
  Md2,
  Md4,
  Md5,
  Sha1,
  Sha256,
  Sha384,
  Sha512,
};

enum class SignatureAlgorithm {
  kRsaPkcs1Sha1,
  kRsaPkcs1Sha256,
  kRsaPkcs1Sha384,
  kRsaPkcs1Sha512,
  kEcdsaSha1,
  kEcdsaSha256,
  kEcdsaSha384,
  kEcdsaSha512,

  kRsaPssSha256,
  kRsaPssSha384,
  kRsaPssSha512,
  kMaxValue = kRsaPssSha512,
};

[[nodiscard]] OPENSSL_EXPORT bool ParseAlgorithmIdentifier(
    der::Input input, der::Input *algorithm, der::Input *parameters);

[[nodiscard]] bool ParseHashAlgorithm(der::Input input, DigestAlgorithm *out);

OPENSSL_EXPORT std::optional<SignatureAlgorithm> ParseSignatureAlgorithm(
    der::Input algorithm_identifier);

OPENSSL_EXPORT std::optional<DigestAlgorithm>
GetTlsServerEndpointDigestAlgorithm(SignatureAlgorithm alg);

BSSL_NAMESPACE_END

#endif
