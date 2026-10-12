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

#include <memory>
#include <set>

#include <gtest/gtest.h>
#include <optional>
#include "cert_errors.h"
#include "input.h"
#include "mock_signature_verify_cache.h"
#include "parse_values.h"
#include "parser.h"
#include "signature_algorithm.h"
#include "test_helpers.h"

BSSL_NAMESPACE_BEGIN

namespace {

enum VerifyResult {
  SUCCESS,
  FAILURE,
};

void RunTestCase(VerifyResult expected_result, const char *file_name,
                 SignatureVerifyCache *cache) {
  std::string path =
      std::string("testdata/verify_signed_data_unittest/") + file_name;

  std::string public_key;
  std::string algorithm;
  std::string signed_data;
  std::string signature_value;

  const PemBlockMapping mappings[] = {
      {"PUBLIC KEY", &public_key},
      {"ALGORITHM", &algorithm},
      {"DATA", &signed_data},
      {"SIGNATURE", &signature_value},
  };

  ASSERT_TRUE(ReadTestDataFromPemFile(path, mappings));

  std::optional<SignatureAlgorithm> signature_algorithm =
      ParseSignatureAlgorithm(der::Input(algorithm));
  ASSERT_TRUE(signature_algorithm);

  der::Parser signature_value_parser((der::Input(signature_value)));
  std::optional<der::BitString> signature_value_bit_string =
      signature_value_parser.ReadBitString();
  ASSERT_TRUE(signature_value_bit_string.has_value())
      << "The signature value is not a valid BIT STRING";

  bool expected_result_bool = expected_result == SUCCESS;

  bool result = VerifySignedData(*signature_algorithm, der::Input(signed_data),
                                 signature_value_bit_string.value(),
                                 der::Input(public_key), cache);

  EXPECT_EQ(expected_result_bool, result);
}

void RunTestCase(VerifyResult expected_result, const char *file_name) {
  RunTestCase(expected_result, file_name,           nullptr);
}

TEST(VerifySignedDataTest, RsaPkcs1Sha1) {
  RunTestCase(SUCCESS, "rsa-pkcs1-sha1.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha256) {
  RunTestCase(SUCCESS, "rsa-pkcs1-sha256.pem");
}

TEST(VerifySignedDataTest, Rsa2048Pkcs1Sha512) {
  RunTestCase(SUCCESS, "rsa2048-pkcs1-sha512.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha256KeyEncodedBer) {
  RunTestCase(FAILURE, "rsa-pkcs1-sha256-key-encoded-ber.pem");
}

TEST(VerifySignedDataTest, EcdsaSecp384r1Sha256) {
  RunTestCase(SUCCESS, "ecdsa-secp384r1-sha256.pem");
}

TEST(VerifySignedDataTest, EcdsaPrime256v1Sha512) {
  RunTestCase(SUCCESS, "ecdsa-prime256v1-sha512.pem");
}

TEST(VerifySignedDataTest, RsaPssSha256) {
  RunTestCase(SUCCESS, "rsa-pss-sha256.pem");
}

TEST(VerifySignedDataTest, RsaPssSha256WrongSalt) {
  RunTestCase(FAILURE, "rsa-pss-sha256-wrong-salt.pem");
}

TEST(VerifySignedDataTest, EcdsaSecp384r1Sha256CorruptedData) {
  RunTestCase(FAILURE, "ecdsa-secp384r1-sha256-corrupted-data.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha1WrongAlgorithm) {
  RunTestCase(FAILURE, "rsa-pkcs1-sha1-wrong-algorithm.pem");
}

TEST(VerifySignedDataTest, EcdsaPrime256v1Sha512WrongSignatureFormat) {
  RunTestCase(FAILURE, "ecdsa-prime256v1-sha512-wrong-signature-format.pem");
}

TEST(VerifySignedDataTest, EcdsaUsingRsaKey) {
  RunTestCase(FAILURE, "ecdsa-using-rsa-key.pem");
}

TEST(VerifySignedDataTest, RsaUsingEcKey) {
  RunTestCase(FAILURE, "rsa-using-ec-key.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha1BadKeyDerNull) {
  RunTestCase(FAILURE, "rsa-pkcs1-sha1-bad-key-der-null.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha1BadKeyDerLength) {
  RunTestCase(FAILURE, "rsa-pkcs1-sha1-bad-key-der-length.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha256UsingEcdsaAlgorithm) {
  RunTestCase(FAILURE, "rsa-pkcs1-sha256-using-ecdsa-algorithm.pem");
}

TEST(VerifySignedDataTest, EcdsaPrime256v1Sha512UsingRsaAlgorithm) {
  RunTestCase(FAILURE, "ecdsa-prime256v1-sha512-using-rsa-algorithm.pem");
}

TEST(VerifySignedDataTest, EcdsaPrime256v1Sha512UsingEcdhKey) {
  RunTestCase(FAILURE, "ecdsa-prime256v1-sha512-using-ecdh-key.pem");
}

TEST(VerifySignedDataTest, EcdsaPrime256v1Sha512UsingEcmqvKey) {
  RunTestCase(FAILURE, "ecdsa-prime256v1-sha512-using-ecmqv-key.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha1KeyParamsAbsent) {
  RunTestCase(FAILURE, "rsa-pkcs1-sha1-key-params-absent.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha1UsingPssKeyNoParams) {
  RunTestCase(FAILURE, "rsa-pkcs1-sha1-using-pss-key-no-params.pem");
}

TEST(VerifySignedDataTest, RsaPssSha256UsingPssKeyWithParams) {

  RunTestCase(FAILURE, "rsa-pss-sha256-using-pss-key-with-params.pem");
}

TEST(VerifySignedDataTest, EcdsaPrime256v1Sha512SpkiParamsNull) {
  RunTestCase(FAILURE, "ecdsa-prime256v1-sha512-spki-params-null.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha256UsingIdEaRsa) {
  RunTestCase(FAILURE, "rsa-pkcs1-sha256-using-id-ea-rsa.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha256SpkiNonNullParams) {
  RunTestCase(FAILURE, "rsa-pkcs1-sha256-spki-non-null-params.pem");
}

TEST(VerifySignedDataTest, EcdsaPrime256v1Sha512UnusedBitsSignature) {
  RunTestCase(FAILURE, "ecdsa-prime256v1-sha512-unused-bits-signature.pem");
}

TEST(VerifySignedDataTest, Ecdsa384) {

  RunTestCase(SUCCESS, "ecdsa-secp384r1-sha256.pem");
  RunTestCase(SUCCESS, "ecdsa-prime256v1-sha512.pem");
}

TEST(VerifySignedDataTestWithCache, TestVerifyCache) {
  MockSignatureVerifyCache verify_cache;

  RunTestCase(SUCCESS, "rsa-pss-sha256.pem",           nullptr);
  EXPECT_EQ(verify_cache.CacheHits(), 0U);
  EXPECT_EQ(verify_cache.CacheMisses(), 0U);
  EXPECT_EQ(verify_cache.CacheStores(), 0U);

  RunTestCase(SUCCESS, "rsa-pss-sha256.pem", &verify_cache);
  EXPECT_EQ(verify_cache.CacheHits(), 0U);
  EXPECT_EQ(verify_cache.CacheMisses(), 1U);
  EXPECT_EQ(verify_cache.CacheStores(), 1U);

  RunTestCase(SUCCESS, "rsa-pss-sha256.pem", &verify_cache);
  RunTestCase(SUCCESS, "rsa-pss-sha256.pem", &verify_cache);
  RunTestCase(SUCCESS, "rsa-pss-sha256.pem", &verify_cache);
  EXPECT_EQ(verify_cache.CacheHits(), 3U);
  EXPECT_EQ(verify_cache.CacheMisses(), 1U);
  EXPECT_EQ(verify_cache.CacheStores(), 1U);

  RunTestCase(FAILURE, "ecdsa-prime256v1-sha512-using-ecdh-key.pem",
              &verify_cache);
  EXPECT_EQ(verify_cache.CacheHits(), 3U);
  EXPECT_EQ(verify_cache.CacheMisses(), 1U);
  EXPECT_EQ(verify_cache.CacheStores(), 1U);

  RunTestCase(FAILURE, "ecdsa-secp384r1-sha256-corrupted-data.pem",
              &verify_cache);
  EXPECT_EQ(verify_cache.CacheHits(), 3U);
  EXPECT_EQ(verify_cache.CacheMisses(), 2U);
  EXPECT_EQ(verify_cache.CacheStores(), 2U);

  RunTestCase(FAILURE, "ecdsa-secp384r1-sha256-corrupted-data.pem",
              &verify_cache);
  RunTestCase(FAILURE, "ecdsa-secp384r1-sha256-corrupted-data.pem",
              &verify_cache);
  RunTestCase(FAILURE, "ecdsa-secp384r1-sha256-corrupted-data.pem",
              &verify_cache);
  EXPECT_EQ(verify_cache.CacheHits(), 6U);
  EXPECT_EQ(verify_cache.CacheMisses(), 2U);
  EXPECT_EQ(verify_cache.CacheStores(), 2U);
}

}

BSSL_NAMESPACE_END
