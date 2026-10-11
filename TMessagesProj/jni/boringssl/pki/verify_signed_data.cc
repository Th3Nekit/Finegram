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

#include "verify_signed_data.h"

#include <openssl/bytestring.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pki/signature_verify_cache.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

#include "cert_errors.h"
#include "input.h"
#include "parse_values.h"
#include "parser.h"
#include "signature_algorithm.h"

BSSL_NAMESPACE_BEGIN

namespace {

bool SHA256UpdateWithLengthPrefixedData(SHA256_CTX *s_ctx, const uint8_t *data,
                                        uint64_t length) {
  return (SHA256_Update(s_ctx, reinterpret_cast<uint8_t *>(&length),
                        sizeof(length)) &&
          SHA256_Update(s_ctx, data, length));
}

constexpr uint32_t VerifyCacheKeyVersion = 1;

std::string SignatureVerifyCacheKey(std::string_view algorithm_name,
                                    der::Input signed_data,
                                    der::Input signature_value_bytes,
                                    EVP_PKEY *public_key) {
  SHA256_CTX s_ctx;
  bssl::ScopedCBB public_key_cbb;
  uint8_t digest[SHA256_DIGEST_LENGTH];
  uint32_t version = VerifyCacheKeyVersion;
  if (CBB_init(public_key_cbb.get(), 128) &&
      EVP_marshal_public_key(public_key_cbb.get(), public_key) &&
      SHA256_Init(&s_ctx) &&
      SHA256_Update(&s_ctx, reinterpret_cast<uint8_t *>(&version),
                    sizeof(version)) &&
      SHA256UpdateWithLengthPrefixedData(
          &s_ctx, reinterpret_cast<const uint8_t *>(algorithm_name.data()),
          algorithm_name.length()) &&
      SHA256UpdateWithLengthPrefixedData(&s_ctx, CBB_data(public_key_cbb.get()),
                                         CBB_len(public_key_cbb.get())) &&
      SHA256UpdateWithLengthPrefixedData(&s_ctx, signature_value_bytes.data(),
                                         signature_value_bytes.size()) &&
      SHA256UpdateWithLengthPrefixedData(&s_ctx, signed_data.data(),
                                         signed_data.size()) &&
      SHA256_Final(digest, &s_ctx)) {
    return std::string(reinterpret_cast<char *>(digest), sizeof(digest));
  }
  return std::string();
}

class OpenSSLErrStackTracer {
 public:
  ~OpenSSLErrStackTracer() { ERR_clear_error(); }
};

}

bool ParsePublicKey(der::Input public_key_spki,
                    bssl::UniquePtr<EVP_PKEY> *public_key) {

  OpenSSLErrStackTracer err_tracer;

  CBS cbs;
  CBS_init(&cbs, public_key_spki.data(), public_key_spki.size());
  public_key->reset(EVP_parse_public_key(&cbs));
  if (!*public_key || CBS_len(&cbs) != 0) {
    public_key->reset();
    return false;
  }
  return true;
}

bool VerifySignedData(SignatureAlgorithm algorithm, der::Input signed_data,
                      const der::BitString &signature_value,
                      EVP_PKEY *public_key, SignatureVerifyCache *cache) {
  int expected_pkey_id = 1;
  const EVP_MD *digest = nullptr;
  bool is_rsa_pss = false;
  std::string_view cache_algorithm_name;
  switch (algorithm) {
    case SignatureAlgorithm::kRsaPkcs1Sha1:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha1();
      cache_algorithm_name = "RsaPkcs1Sha1";
      break;
    case SignatureAlgorithm::kRsaPkcs1Sha256:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha256();
      cache_algorithm_name = "RsaPkcs1Sha256";
      break;
    case SignatureAlgorithm::kRsaPkcs1Sha384:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha384();
      cache_algorithm_name = "RsaPkcs1Sha384";
      break;
    case SignatureAlgorithm::kRsaPkcs1Sha512:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha512();
      cache_algorithm_name = "RsaPkcs1Sha512";
      break;

    case SignatureAlgorithm::kEcdsaSha1:
      expected_pkey_id = EVP_PKEY_EC;
      digest = EVP_sha1();
      cache_algorithm_name = "EcdsaSha1";
      break;
    case SignatureAlgorithm::kEcdsaSha256:
      expected_pkey_id = EVP_PKEY_EC;
      digest = EVP_sha256();
      cache_algorithm_name = "EcdsaSha256";
      break;
    case SignatureAlgorithm::kEcdsaSha384:
      expected_pkey_id = EVP_PKEY_EC;
      digest = EVP_sha384();
      cache_algorithm_name = "EcdsaSha384";
      break;
    case SignatureAlgorithm::kEcdsaSha512:
      expected_pkey_id = EVP_PKEY_EC;
      digest = EVP_sha512();
      cache_algorithm_name = "EcdsaSha512";
      break;

    case SignatureAlgorithm::kRsaPssSha256:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha256();
      cache_algorithm_name = "RsaPssSha256";
      is_rsa_pss = true;
      break;
    case SignatureAlgorithm::kRsaPssSha384:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha384();
      cache_algorithm_name = "RsaPssSha384";
      is_rsa_pss = true;
      break;
    case SignatureAlgorithm::kRsaPssSha512:
      expected_pkey_id = EVP_PKEY_RSA;
      digest = EVP_sha512();
      cache_algorithm_name = "RsaPssSha512";
      is_rsa_pss = true;
      break;
  }

  if (expected_pkey_id != EVP_PKEY_id(public_key)) {
    return false;
  }

  if (signature_value.unused_bits() != 0) {
    return false;
  }
  der::Input signature_value_bytes = signature_value.bytes();

  std::string cache_key;
  if (cache) {
    cache_key = SignatureVerifyCacheKey(cache_algorithm_name, signed_data,
                                        signature_value_bytes, public_key);
    if (!cache_key.empty()) {
      switch (cache->Check(cache_key)) {
        case SignatureVerifyCache::Value::kValid:
          return true;
        case SignatureVerifyCache::Value::kInvalid:
          return false;
        case SignatureVerifyCache::Value::kUnknown:
          break;
      }
    }
  }

  OpenSSLErrStackTracer err_tracer;

  bssl::ScopedEVP_MD_CTX ctx;
  EVP_PKEY_CTX *pctx = nullptr;

  if (!EVP_DigestVerifyInit(ctx.get(), &pctx, digest, nullptr, public_key)) {
    return false;
  }

  if (is_rsa_pss) {

    if (!EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) ||
        !EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, -1)) {
      return false;
    }
  }

  bool ret = 1 == EVP_DigestVerify(ctx.get(), signature_value_bytes.data(),
                                   signature_value_bytes.size(),
                                   signed_data.data(), signed_data.size());
  if (!cache_key.empty()) {
    cache->Store(cache_key, ret ? SignatureVerifyCache::Value::kValid
                                : SignatureVerifyCache::Value::kInvalid);
  }

  return ret;
}

bool VerifySignedData(SignatureAlgorithm algorithm, der::Input signed_data,
                      const der::BitString &signature_value,
                      der::Input public_key_spki, SignatureVerifyCache *cache) {
  bssl::UniquePtr<EVP_PKEY> public_key;
  if (!ParsePublicKey(public_key_spki, &public_key)) {
    return false;
  }
  return VerifySignedData(algorithm, signed_data, signature_value,
                          public_key.get(), cache);
}

BSSL_NAMESPACE_END
