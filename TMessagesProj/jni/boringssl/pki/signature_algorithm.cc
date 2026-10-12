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

#include "signature_algorithm.h"

#include <openssl/bytestring.h>
#include <openssl/digest.h>

#include "input.h"
#include "parse_values.h"
#include "parser.h"

BSSL_NAMESPACE_BEGIN

namespace {

const uint8_t kOidSha1WithRsaEncryption[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                             0x0d, 0x01, 0x01, 0x05};

const uint8_t kOidSha1WithRsaSignature[] = {0x2b, 0x0e, 0x03, 0x02, 0x1d};

const uint8_t kOidSha256WithRsaEncryption[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                               0x0d, 0x01, 0x01, 0x0b};

const uint8_t kOidSha384WithRsaEncryption[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                               0x0d, 0x01, 0x01, 0x0c};

const uint8_t kOidSha512WithRsaEncryption[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                               0x0d, 0x01, 0x01, 0x0d};

const uint8_t kOidEcdsaWithSha1[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x01};

const uint8_t kOidEcdsaWithSha256[] = {0x2a, 0x86, 0x48, 0xce,
                                       0x3d, 0x04, 0x03, 0x02};

const uint8_t kOidEcdsaWithSha384[] = {0x2a, 0x86, 0x48, 0xce,
                                       0x3d, 0x04, 0x03, 0x03};

const uint8_t kOidEcdsaWithSha512[] = {0x2a, 0x86, 0x48, 0xce,
                                       0x3d, 0x04, 0x03, 0x04};

const uint8_t kOidRsaSsaPss[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                 0x0d, 0x01, 0x01, 0x0a};

const uint8_t kOidMgf1[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                            0x0d, 0x01, 0x01, 0x08};

[[nodiscard]] bool IsNull(der::Input input) {
  der::Parser parser(input);
  der::Input null_value;
  if (!parser.ReadTag(CBS_ASN1_NULL, &null_value)) {
    return false;
  }

  if (!null_value.empty()) {
    return false;
  }

  return !parser.HasMore();
}

[[nodiscard]] bool IsNullOrEmpty(der::Input input) {
  return IsNull(input) || input.empty();
}

[[nodiscard]] bool ParseMaskGenAlgorithm(const der::Input input,
                                         DigestAlgorithm *mgf1_hash) {
  der::Input oid;
  der::Input params;
  if (!ParseAlgorithmIdentifier(input, &oid, &params)) {
    return false;
  }

  if (oid != der::Input(kOidMgf1)) {
    return false;
  }

  return ParseHashAlgorithm(params, mgf1_hash);
}

std::optional<SignatureAlgorithm> ParseRsaPss(der::Input params) {
  der::Parser parser(params);
  der::Parser params_parser;
  if (!parser.ReadSequence(&params_parser)) {
    return std::nullopt;
  }

  if (parser.HasMore()) {
    return std::nullopt;
  }

  der::Input field;
  DigestAlgorithm hash, mgf1_hash;
  der::Parser salt_length_parser;
  uint64_t salt_length;
  if (!params_parser.ReadTag(
          CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0, &field) ||
      !ParseHashAlgorithm(field, &hash) ||
      !params_parser.ReadTag(
          CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 1, &field) ||
      !ParseMaskGenAlgorithm(field, &mgf1_hash) ||
      !params_parser.ReadConstructed(
          CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 2,
          &salt_length_parser) ||
      !salt_length_parser.ReadUint64(&salt_length) ||
      salt_length_parser.HasMore() || params_parser.HasMore()) {
    return std::nullopt;
  }

  if (hash != mgf1_hash) {
    return std::nullopt;
  }
  if (hash == DigestAlgorithm::Sha256 && salt_length == 32) {
    return SignatureAlgorithm::kRsaPssSha256;
  }
  if (hash == DigestAlgorithm::Sha384 && salt_length == 48) {
    return SignatureAlgorithm::kRsaPssSha384;
  }
  if (hash == DigestAlgorithm::Sha512 && salt_length == 64) {
    return SignatureAlgorithm::kRsaPssSha512;
  }

  return std::nullopt;
}

}

[[nodiscard]] bool ParseAlgorithmIdentifier(der::Input input,
                                            der::Input *algorithm,
                                            der::Input *parameters) {
  der::Parser parser(input);

  der::Parser algorithm_identifier_parser;
  if (!parser.ReadSequence(&algorithm_identifier_parser)) {
    return false;
  }

  if (parser.HasMore()) {
    return false;
  }

  if (!algorithm_identifier_parser.ReadTag(CBS_ASN1_OBJECT, algorithm)) {
    return false;
  }

  *parameters = der::Input();
  if (algorithm_identifier_parser.HasMore() &&
      !algorithm_identifier_parser.ReadRawTLV(parameters)) {
    return false;
  }
  return !algorithm_identifier_parser.HasMore();
}

[[nodiscard]] bool ParseHashAlgorithm(der::Input input, DigestAlgorithm *out) {
  CBS cbs;
  CBS_init(&cbs, input.data(), input.size());
  const EVP_MD *md = EVP_parse_digest_algorithm(&cbs);

  if (md == EVP_sha1()) {
    *out = DigestAlgorithm::Sha1;
  } else if (md == EVP_sha256()) {
    *out = DigestAlgorithm::Sha256;
  } else if (md == EVP_sha384()) {
    *out = DigestAlgorithm::Sha384;
  } else if (md == EVP_sha512()) {
    *out = DigestAlgorithm::Sha512;
  } else {

    return false;
  }

  return true;
}

std::optional<SignatureAlgorithm> ParseSignatureAlgorithm(
    der::Input algorithm_identifier) {
  der::Input oid;
  der::Input params;
  if (!ParseAlgorithmIdentifier(algorithm_identifier, &oid, &params)) {
    return std::nullopt;
  }

  if (oid == der::Input(kOidSha1WithRsaEncryption) && IsNullOrEmpty(params)) {
    return SignatureAlgorithm::kRsaPkcs1Sha1;
  }
  if (oid == der::Input(kOidSha256WithRsaEncryption) && IsNullOrEmpty(params)) {
    return SignatureAlgorithm::kRsaPkcs1Sha256;
  }
  if (oid == der::Input(kOidSha384WithRsaEncryption) && IsNullOrEmpty(params)) {
    return SignatureAlgorithm::kRsaPkcs1Sha384;
  }
  if (oid == der::Input(kOidSha512WithRsaEncryption) && IsNullOrEmpty(params)) {
    return SignatureAlgorithm::kRsaPkcs1Sha512;
  }
  if (oid == der::Input(kOidSha1WithRsaSignature) && IsNullOrEmpty(params)) {
    return SignatureAlgorithm::kRsaPkcs1Sha1;
  }

  if (oid == der::Input(kOidEcdsaWithSha1) && params.empty()) {
    return SignatureAlgorithm::kEcdsaSha1;
  }
  if (oid == der::Input(kOidEcdsaWithSha256) && params.empty()) {
    return SignatureAlgorithm::kEcdsaSha256;
  }
  if (oid == der::Input(kOidEcdsaWithSha384) && params.empty()) {
    return SignatureAlgorithm::kEcdsaSha384;
  }
  if (oid == der::Input(kOidEcdsaWithSha512) && params.empty()) {
    return SignatureAlgorithm::kEcdsaSha512;
  }

  if (oid == der::Input(kOidRsaSsaPss)) {
    return ParseRsaPss(params);
  }

  return std::nullopt;
}

std::optional<DigestAlgorithm> GetTlsServerEndpointDigestAlgorithm(
    SignatureAlgorithm alg) {

  switch (alg) {

    case SignatureAlgorithm::kRsaPkcs1Sha1:
    case SignatureAlgorithm::kEcdsaSha1:
      return DigestAlgorithm::Sha256;

    case SignatureAlgorithm::kRsaPkcs1Sha256:
    case SignatureAlgorithm::kEcdsaSha256:
      return DigestAlgorithm::Sha256;

    case SignatureAlgorithm::kRsaPkcs1Sha384:
    case SignatureAlgorithm::kEcdsaSha384:
      return DigestAlgorithm::Sha384;

    case SignatureAlgorithm::kRsaPkcs1Sha512:
    case SignatureAlgorithm::kEcdsaSha512:
      return DigestAlgorithm::Sha512;

    case SignatureAlgorithm::kRsaPssSha256:
      return DigestAlgorithm::Sha256;
    case SignatureAlgorithm::kRsaPssSha384:
      return DigestAlgorithm::Sha384;
    case SignatureAlgorithm::kRsaPssSha512:
      return DigestAlgorithm::Sha512;
  }
  return std::nullopt;
}

BSSL_NAMESPACE_END
