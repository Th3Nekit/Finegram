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

#include "parse_certificate.h"

#include <gtest/gtest.h>
#include <openssl/pool.h>
#include "cert_errors.h"
#include "general_names.h"
#include "input.h"
#include "parsed_certificate.h"
#include "test_helpers.h"

BSSL_NAMESPACE_BEGIN

namespace {

std::string ToString(const der::GeneralizedTime &time) {
  std::ostringstream pretty_time;
  pretty_time << "year=" << int{time.year} << ", month=" << int{time.month}
              << ", day=" << int{time.day} << ", hours=" << int{time.hours}
              << ", minutes=" << int{time.minutes}
              << ", seconds=" << int{time.seconds};
  return pretty_time.str();
}

std::string GetFilePath(const std::string &file_name) {
  return std::string("testdata/parse_certificate_unittest/") + file_name;
}

void RunCertificateTest(const std::string &file_name) {
  std::string data;
  std::string expected_errors;
  std::string expected_tbs_certificate;
  std::string expected_signature_algorithm;
  std::string expected_signature;

  const PemBlockMapping mappings[] = {
      {"CERTIFICATE", &data},
      {"ERRORS", &expected_errors, true             },
      {"SIGNATURE", &expected_signature, true             },
      {"SIGNATURE ALGORITHM", &expected_signature_algorithm, true             },
      {"TBS CERTIFICATE", &expected_tbs_certificate, true             },
  };
  std::string test_file_path = GetFilePath(file_name);
  ASSERT_TRUE(ReadTestDataFromPemFile(test_file_path, mappings));

  bool expected_result = !expected_tbs_certificate.empty();

  der::Input tbs_certificate_tlv;
  der::Input signature_algorithm_tlv;
  der::BitString signature_value;
  CertErrors errors;
  bool actual_result =
      ParseCertificate(der::Input(data), &tbs_certificate_tlv,
                       &signature_algorithm_tlv, &signature_value, &errors);

  EXPECT_EQ(expected_result, actual_result);
  VerifyCertErrors(expected_errors, errors, test_file_path);

  if (expected_result && actual_result) {
    EXPECT_EQ(0, signature_value.unused_bits());
    EXPECT_EQ(der::Input(expected_signature), signature_value.bytes());
    EXPECT_EQ(der::Input(expected_signature_algorithm),
              signature_algorithm_tlv);
    EXPECT_EQ(der::Input(expected_tbs_certificate), tbs_certificate_tlv);
  }
}

TEST(ParseCertificateTest, Version3) {
  RunCertificateTest("cert_version3.pem");
}

TEST(ParseCertificateTest, Skeleton) {
  RunCertificateTest("cert_skeleton.pem");
}

TEST(ParseCertificateTest, NotSequence) {
  RunCertificateTest("cert_not_sequence.pem");
}

TEST(ParseCertificateTest, DataAfterSignature) {
  RunCertificateTest("cert_data_after_signature.pem");
}

TEST(ParseCertificateTest, MissingSignature) {
  RunCertificateTest("cert_missing_signature.pem");
}

TEST(ParseCertificateTest, SignatureNotBitString) {
  RunCertificateTest("cert_signature_not_bit_string.pem");
}

TEST(ParseCertificateTest, EmptySequence) {
  RunCertificateTest("cert_empty_sequence.pem");
}

TEST(ParseCertificateTest, AlgorithmNotSequence) {
  RunCertificateTest("cert_algorithm_not_sequence.pem");
}

void RunTbsCertificateTestGivenVersion(const std::string &file_name,
                                       CertificateVersion expected_version) {
  std::string data;
  std::string expected_serial_number;
  std::string expected_signature_algorithm;
  std::string expected_issuer;
  std::string expected_validity_not_before;
  std::string expected_validity_not_after;
  std::string expected_subject;
  std::string expected_spki;
  std::string expected_issuer_unique_id;
  std::string expected_subject_unique_id;
  std::string expected_extensions;
  std::string expected_errors;

  const PemBlockMapping mappings[] = {
      {"TBS CERTIFICATE", &data},
      {"SIGNATURE ALGORITHM", &expected_signature_algorithm, true},
      {"SERIAL NUMBER", &expected_serial_number, true},
      {"ISSUER", &expected_issuer, true},
      {"VALIDITY NOTBEFORE", &expected_validity_not_before, true},
      {"VALIDITY NOTAFTER", &expected_validity_not_after, true},
      {"SUBJECT", &expected_subject, true},
      {"SPKI", &expected_spki, true},
      {"ISSUER UNIQUE ID", &expected_issuer_unique_id, true},
      {"SUBJECT UNIQUE ID", &expected_subject_unique_id, true},
      {"EXTENSIONS", &expected_extensions, true},
      {"ERRORS", &expected_errors, true},
  };
  std::string test_file_path = GetFilePath(file_name);
  ASSERT_TRUE(ReadTestDataFromPemFile(test_file_path, mappings));

  bool expected_result = !expected_spki.empty();

  ParsedTbsCertificate parsed;
  CertErrors errors;
  bool actual_result =
      ParseTbsCertificate(der::Input(data), {}, &parsed, &errors);

  EXPECT_EQ(expected_result, actual_result);
  VerifyCertErrors(expected_errors, errors, test_file_path);

  if (!expected_result || !actual_result) {
    return;
  }

  EXPECT_EQ(expected_version, parsed.version);

  EXPECT_EQ(der::Input(expected_serial_number), parsed.serial_number);
  EXPECT_EQ(der::Input(expected_signature_algorithm),
            parsed.signature_algorithm_tlv);

  EXPECT_EQ(der::Input(expected_issuer), parsed.issuer_tlv);

  EXPECT_EQ(expected_validity_not_before, ToString(parsed.validity_not_before));
  EXPECT_EQ(expected_validity_not_after, ToString(parsed.validity_not_after));

  EXPECT_EQ(der::Input(expected_subject), parsed.subject_tlv);
  EXPECT_EQ(der::Input(expected_spki), parsed.spki_tlv);

  EXPECT_EQ(!expected_issuer_unique_id.empty(),
            parsed.issuer_unique_id.has_value());
  if (parsed.issuer_unique_id.has_value()) {
    EXPECT_EQ(der::Input(expected_issuer_unique_id),
              parsed.issuer_unique_id->bytes());
  }
  EXPECT_EQ(!expected_subject_unique_id.empty(),
            parsed.subject_unique_id.has_value());
  if (parsed.subject_unique_id.has_value()) {
    EXPECT_EQ(der::Input(expected_subject_unique_id),
              parsed.subject_unique_id->bytes());
  }

  EXPECT_EQ(!expected_extensions.empty(), parsed.extensions_tlv.has_value());
  if (parsed.extensions_tlv) {
    EXPECT_EQ(der::Input(expected_extensions), parsed.extensions_tlv.value());
  }
}

void RunTbsCertificateTest(const std::string &file_name) {
  RunTbsCertificateTestGivenVersion(file_name, CertificateVersion::V3);
}

TEST(ParseTbsCertificateTest, Version3NoOptionals) {
  RunTbsCertificateTest("tbs_v3_no_optionals.pem");
}

TEST(ParseTbsCertificateTest, Version3WithExtensions) {
  RunTbsCertificateTest("tbs_v3_extensions.pem");
}

TEST(ParseTbsCertificateTest, Version1) {
  RunTbsCertificateTestGivenVersion("tbs_v1.pem", CertificateVersion::V1);
}

TEST(ParseTbsCertificateTest, ExplicitVersion1) {
  RunTbsCertificateTest("tbs_explicit_v1.pem");
}

TEST(ParseTbsCertificateTest, Version1WithExtensions) {
  RunTbsCertificateTest("tbs_v1_extensions.pem");
}

TEST(ParseTbsCertificateTest, Version2WithExtensions) {
  RunTbsCertificateTest("tbs_v2_extensions.pem");
}

TEST(ParseTbsCertificateTest, Version2NoOptionals) {
  RunTbsCertificateTestGivenVersion("tbs_v2_no_optionals.pem",
                                    CertificateVersion::V2);
}

TEST(ParseTbsCertificateTest, Version2IssuerUniqueId) {
  RunTbsCertificateTestGivenVersion("tbs_v2_issuer_unique_id.pem",
                                    CertificateVersion::V2);
}

TEST(ParseTbsCertificateTest, Version2IssuerAndSubjectUniqueId) {
  RunTbsCertificateTestGivenVersion("tbs_v2_issuer_and_subject_unique_id.pem",
                                    CertificateVersion::V2);
}

TEST(ParseTbsCertificateTest, Version3AllOptionals) {
  RunTbsCertificateTest("tbs_v3_all_optionals.pem");
}

TEST(ParseTbsCertificateTest, Version4) { RunTbsCertificateTest("tbs_v4.pem"); }

TEST(ParseTbsCertificateTest, Version3DataAfterExtensions) {
  RunTbsCertificateTest("tbs_v3_data_after_extensions.pem");
}

TEST(ParseTbsCertificateTest, Version3Real) {
  RunTbsCertificateTest("tbs_v3_real.pem");
}

TEST(ParseTbsCertificateTest, ValidityBothUtcTime) {
  RunTbsCertificateTest("tbs_validity_both_utc_time.pem");
}

TEST(ParseTbsCertificateTest, ValidityBothGeneralizedTime) {
  RunTbsCertificateTest("tbs_validity_both_generalized_time.pem");
}

TEST(ParseTbsCertificateTest, ValidityUTCTimeAndGeneralizedTime) {
  RunTbsCertificateTest("tbs_validity_utc_time_and_generalized_time.pem");
}

TEST(ParseTbsCertificateTest, ValidityGeneralizedTimeAndUTCTime) {
  RunTbsCertificateTest("tbs_validity_generalized_time_and_utc_time.pem");
}

TEST(ParseTbsCertificateTest, ValidityRelaxed) {
  RunTbsCertificateTest("tbs_validity_relaxed.pem");
}

TEST(ParseKeyUsageTest, OneBitAllZeros) {
  const uint8_t der[] = {
      0x03, 0x02,
      0x07,
      0x00,
  };

  der::BitString key_usage;
  ASSERT_FALSE(ParseKeyUsage(der::Input(der), &key_usage));
}

TEST(ParseKeyUsageTest, 32BitsAllZeros) {
  const uint8_t der[] = {
      0x03, 0x05,
      0x00,
      0x00, 0x00, 0x00, 0x00,
  };

  der::BitString key_usage;
  ASSERT_FALSE(ParseKeyUsage(der::Input(der), &key_usage));
}

TEST(ParseKeyUsageTest, 32BitsOneSet) {
  const uint8_t der[] = {
      0x03, 0x05,
      0x00,
      0x00, 0x00, 0x00, 0x02,
  };

  der::BitString key_usage;
  ASSERT_TRUE(ParseKeyUsage(der::Input(der), &key_usage));

  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_DIGITAL_SIGNATURE));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_NON_REPUDIATION));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_ENCIPHERMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_DATA_ENCIPHERMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_AGREEMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_CERT_SIGN));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_CRL_SIGN));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_ENCIPHER_ONLY));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_DECIPHER_ONLY));
}

TEST(ParseKeyUsageTest, ThreeBits) {
  const uint8_t der[] = {
      0x03, 0x02,
      0x05,
      0xA0,
  };

  der::BitString key_usage;
  ASSERT_TRUE(ParseKeyUsage(der::Input(der), &key_usage));

  EXPECT_TRUE(key_usage.AssertsBit(KEY_USAGE_BIT_DIGITAL_SIGNATURE));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_NON_REPUDIATION));
  EXPECT_TRUE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_ENCIPHERMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_DATA_ENCIPHERMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_AGREEMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_CERT_SIGN));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_CRL_SIGN));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_ENCIPHER_ONLY));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_DECIPHER_ONLY));
}

TEST(ParseKeyUsageTest, DecipherOnly) {
  const uint8_t der[] = {
      0x03, 0x03,
      0x07,
      0x00, 0x80,
  };

  der::BitString key_usage;
  ASSERT_TRUE(ParseKeyUsage(der::Input(der), &key_usage));

  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_DIGITAL_SIGNATURE));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_NON_REPUDIATION));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_ENCIPHERMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_DATA_ENCIPHERMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_AGREEMENT));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_KEY_CERT_SIGN));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_CRL_SIGN));
  EXPECT_FALSE(key_usage.AssertsBit(KEY_USAGE_BIT_ENCIPHER_ONLY));
  EXPECT_TRUE(key_usage.AssertsBit(KEY_USAGE_BIT_DECIPHER_ONLY));
}

TEST(ParseKeyUsageTest, Empty) {
  const uint8_t der[] = {
      0x03, 0x01,
      0x00,
  };

  der::BitString key_usage;
  ASSERT_FALSE(ParseKeyUsage(der::Input(der), &key_usage));
}

TEST(ParseAuthorityInfoAccess, BasicTests) {

  const uint8_t der[] = {
      0x30, 0x81, 0xc3, 0x30, 0x1d, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05,
      0x07, 0x30, 0x01, 0xa4, 0x11, 0x30, 0x0f, 0x31, 0x0d, 0x30, 0x0b, 0x06,
      0x03, 0x55, 0x04, 0x03, 0x13, 0x04, 0x6f, 0x63, 0x73, 0x70, 0x30, 0x22,
      0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x02, 0xa4, 0x16,
      0x30, 0x14, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13,
      0x09, 0x63, 0x61, 0x20, 0x69, 0x73, 0x73, 0x75, 0x65, 0x72, 0x30, 0x2a,
      0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x03, 0x86, 0x1e,
      0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x6e, 0x6f, 0x6e, 0x73, 0x74,
      0x61, 0x6e, 0x64, 0x61, 0x72, 0x64, 0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70,
      0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x30, 0x23, 0x06, 0x08, 0x2b, 0x06,
      0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x86, 0x17, 0x68, 0x74, 0x74, 0x70,
      0x3a, 0x2f, 0x2f, 0x6f, 0x63, 0x73, 0x70, 0x2e, 0x65, 0x78, 0x61, 0x6d,
      0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x30, 0x2d, 0x06, 0x08, 0x2b,
      0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x02, 0x86, 0x21, 0x68, 0x74, 0x74,
      0x70, 0x3a, 0x2f, 0x2f, 0x77, 0x77, 0x77, 0x2e, 0x65, 0x78, 0x61, 0x6d,
      0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x69, 0x73, 0x73, 0x75,
      0x65, 0x72, 0x2e, 0x63, 0x72, 0x74};

  std::vector<AuthorityInfoAccessDescription> access_descriptions;
  ASSERT_TRUE(ParseAuthorityInfoAccess(der::Input(der), &access_descriptions));
  ASSERT_EQ(5u, access_descriptions.size());
  {
    const auto &desc = access_descriptions[0];
    EXPECT_EQ(der::Input(kAdOcspOid), desc.access_method_oid);
    const uint8_t location_der[] = {0xa4, 0x11, 0x30, 0x0f, 0x31, 0x0d, 0x30,
                                    0x0b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13,
                                    0x04, 0x6f, 0x63, 0x73, 0x70};
    EXPECT_EQ(der::Input(location_der), desc.access_location);
  }
  {
    const auto &desc = access_descriptions[1];
    EXPECT_EQ(der::Input(kAdCaIssuersOid), desc.access_method_oid);
    const uint8_t location_der[] = {
        0xa4, 0x16, 0x30, 0x14, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04,
        0x03, 0x13, 0x09, 0x63, 0x61, 0x20, 0x69, 0x73, 0x73, 0x75, 0x65, 0x72};
    EXPECT_EQ(der::Input(location_der), desc.access_location);
  }
  {
    const auto &desc = access_descriptions[2];
    const uint8_t method_oid[] = {0x2b, 0x06, 0x01, 0x05,
                                  0x05, 0x07, 0x30, 0x03};
    EXPECT_EQ(der::Input(method_oid), desc.access_method_oid);
    const uint8_t location_der[] = {
        0x86, 0x1e, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x6e, 0x6f,
        0x6e, 0x73, 0x74, 0x61, 0x6e, 0x64, 0x61, 0x72, 0x64, 0x2e, 0x65,
        0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d};
    EXPECT_EQ(der::Input(location_der), desc.access_location);
  }
  {
    const auto &desc = access_descriptions[3];
    EXPECT_EQ(der::Input(kAdOcspOid), desc.access_method_oid);
    const uint8_t location_der[] = {0x86, 0x17, 0x68, 0x74, 0x74, 0x70, 0x3a,
                                    0x2f, 0x2f, 0x6f, 0x63, 0x73, 0x70, 0x2e,
                                    0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65,
                                    0x2e, 0x63, 0x6f, 0x6d};
    EXPECT_EQ(der::Input(location_der), desc.access_location);
  }
  {
    const auto &desc = access_descriptions[4];
    EXPECT_EQ(der::Input(kAdCaIssuersOid), desc.access_method_oid);
    const uint8_t location_der[] = {
        0x86, 0x21, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x77, 0x77, 0x77,
        0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d,
        0x2f, 0x69, 0x73, 0x73, 0x75, 0x65, 0x72, 0x2e, 0x63, 0x72, 0x74};
    EXPECT_EQ(der::Input(location_der), desc.access_location);
  }

  std::vector<std::string_view> ca_issuers_uris, ocsp_uris;
  ASSERT_TRUE(ParseAuthorityInfoAccessURIs(der::Input(der), &ca_issuers_uris,
                                           &ocsp_uris));
  ASSERT_EQ(1u, ca_issuers_uris.size());
  EXPECT_EQ("http://www.example.com/issuer.crt", ca_issuers_uris.front());
  ASSERT_EQ(1u, ocsp_uris.size());
  EXPECT_EQ("http://ocsp.example.com", ocsp_uris.front());
}

TEST(ParseAuthorityInfoAccess, NoOcspOrCaIssuersURIs) {

  const uint8_t der[] = {0x30, 0x18, 0x30, 0x16, 0x06, 0x02, 0x2a, 0x03, 0xa4,
                         0x10, 0x30, 0x0e, 0x31, 0x0c, 0x30, 0x0a, 0x06, 0x03,
                         0x55, 0x04, 0x03, 0x13, 0x03, 0x66, 0x6f, 0x6f};

  std::vector<AuthorityInfoAccessDescription> access_descriptions;
  ASSERT_TRUE(ParseAuthorityInfoAccess(der::Input(der), &access_descriptions));
  ASSERT_EQ(1u, access_descriptions.size());
  const auto &desc = access_descriptions[0];
  const uint8_t method_oid[] = {0x2a, 0x03};
  EXPECT_EQ(der::Input(method_oid), desc.access_method_oid);
  const uint8_t location_der[] = {0xa4, 0x10, 0x30, 0x0e, 0x31, 0x0c,
                                  0x30, 0x0a, 0x06, 0x03, 0x55, 0x04,
                                  0x03, 0x13, 0x03, 0x66, 0x6f, 0x6f};
  EXPECT_EQ(der::Input(location_der), desc.access_location);

  std::vector<std::string_view> ca_issuers_uris, ocsp_uris;

  ASSERT_TRUE(ParseAuthorityInfoAccessURIs(der::Input(der), &ca_issuers_uris,
                                           &ocsp_uris));
  EXPECT_EQ(0u, ca_issuers_uris.size());
  EXPECT_EQ(0u, ocsp_uris.size());
}

TEST(ParseAuthorityInfoAccess, IncompleteAccessDescription) {

  const uint8_t der[] = {0x30, 0x31, 0x30, 0x23, 0x06, 0x08, 0x2b, 0x06, 0x01,
                         0x05, 0x05, 0x07, 0x30, 0x01, 0x86, 0x17, 0x68, 0x74,
                         0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x6f, 0x63, 0x73, 0x70,
                         0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e,
                         0x63, 0x6f, 0x6d, 0x30, 0x0a, 0x06, 0x08, 0x2b, 0x06,
                         0x01, 0x05, 0x05, 0x07, 0x30, 0x02};

  std::vector<AuthorityInfoAccessDescription> access_descriptions;
  EXPECT_FALSE(ParseAuthorityInfoAccess(der::Input(der), &access_descriptions));

  std::vector<std::string_view> ca_issuers_uris, ocsp_uris;
  EXPECT_FALSE(ParseAuthorityInfoAccessURIs(der::Input(der), &ca_issuers_uris,
                                            &ocsp_uris));
}

TEST(ParseAuthorityInfoAccess, ExtraDataInAccessDescription) {

  const uint8_t der[] = {
      0x30, 0x2c, 0x30, 0x2a, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07,
      0x30, 0x01, 0x86, 0x17, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x6f,
      0x63, 0x73, 0x70, 0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e,
      0x63, 0x6f, 0x6d, 0x13, 0x05, 0x68, 0x65, 0x6e, 0x6c, 0x6f};

  std::vector<AuthorityInfoAccessDescription> access_descriptions;
  EXPECT_FALSE(ParseAuthorityInfoAccess(der::Input(der), &access_descriptions));

  std::vector<std::string_view> ca_issuers_uris, ocsp_uris;
  EXPECT_FALSE(ParseAuthorityInfoAccessURIs(der::Input(der), &ca_issuers_uris,
                                            &ocsp_uris));
}

TEST(ParseAuthorityInfoAccess, EmptySequence) {

  const uint8_t der[] = {0x30, 0x00};

  std::vector<AuthorityInfoAccessDescription> access_descriptions;
  EXPECT_FALSE(ParseAuthorityInfoAccess(der::Input(der), &access_descriptions));

  std::vector<std::string_view> ca_issuers_uris, ocsp_uris;
  EXPECT_FALSE(ParseAuthorityInfoAccessURIs(der::Input(der), &ca_issuers_uris,
                                            &ocsp_uris));
}

class ParseCrlDistributionPointsTest : public ::testing::Test {
 public:
 protected:
  bool GetCrlDps(const char *file_name,
                 std::vector<ParsedDistributionPoint> *dps) {
    std::string cert_bytes;

    const PemBlockMapping mappings[] = {
        {"CERTIFICATE", &cert_bytes},
    };
    std::string test_file_path = GetFilePath(file_name);
    EXPECT_TRUE(ReadTestDataFromPemFile(test_file_path, mappings));

    CertErrors errors;
    std::shared_ptr<const ParsedCertificate> cert = ParsedCertificate::Create(
        bssl::UniquePtr<CRYPTO_BUFFER>(CRYPTO_BUFFER_new(
            reinterpret_cast<const uint8_t *>(cert_bytes.data()),
            cert_bytes.size(), nullptr)),
        {}, &errors);

    if (!cert) {
      return false;
    }

    auto it = cert->extensions().find(der::Input(kCrlDistributionPointsOid));
    if (it == cert->extensions().end()) {
      return false;
    }

    der::Input crl_dp_tlv = it->second.value;

    keep_alive_certs_.push_back(cert);

    return ParseCrlDistributionPoints(crl_dp_tlv, dps);
  }

 private:
  ParsedCertificateList keep_alive_certs_;
};

TEST_F(ParseCrlDistributionPointsTest, OneUriNoIssuer) {
  std::vector<ParsedDistributionPoint> dps;
  ASSERT_TRUE(GetCrlDps("crldp_1uri_noissuer.pem", &dps));

  ASSERT_EQ(1u, dps.size());
  const ParsedDistributionPoint &dp1 = dps.front();

  ASSERT_TRUE(dp1.distribution_point_fullname);
  const GeneralNames &fullname = *dp1.distribution_point_fullname;
  EXPECT_EQ(GENERAL_NAME_UNIFORM_RESOURCE_IDENTIFIER,
            fullname.present_name_types);
  ASSERT_EQ(1u, fullname.uniform_resource_identifiers.size());
  EXPECT_EQ(fullname.uniform_resource_identifiers.front(),
            std::string("http://www.example.com/foo.crl"));

  EXPECT_FALSE(dp1.distribution_point_name_relative_to_crl_issuer);
  EXPECT_FALSE(dp1.reasons);
  EXPECT_FALSE(dp1.crl_issuer);
}

TEST_F(ParseCrlDistributionPointsTest, ThreeUrisNoIssuer) {
  std::vector<ParsedDistributionPoint> dps;
  ASSERT_TRUE(GetCrlDps("crldp_3uri_noissuer.pem", &dps));

  ASSERT_EQ(1u, dps.size());
  const ParsedDistributionPoint &dp1 = dps.front();

  ASSERT_TRUE(dp1.distribution_point_fullname);
  const GeneralNames &fullname = *dp1.distribution_point_fullname;
  EXPECT_EQ(GENERAL_NAME_UNIFORM_RESOURCE_IDENTIFIER,
            fullname.present_name_types);
  ASSERT_EQ(3u, fullname.uniform_resource_identifiers.size());
  EXPECT_EQ(fullname.uniform_resource_identifiers[0],
            std::string("http://www.example.com/foo1.crl"));
  EXPECT_EQ(fullname.uniform_resource_identifiers[1],
            std::string("http://www.example.com/blah.crl"));
  EXPECT_EQ(fullname.uniform_resource_identifiers[2],
            std::string("not-even-a-url"));

  EXPECT_FALSE(dp1.distribution_point_name_relative_to_crl_issuer);
  EXPECT_FALSE(dp1.reasons);
  EXPECT_FALSE(dp1.crl_issuer);
}

TEST_F(ParseCrlDistributionPointsTest, CrlIssuerAsDirname) {
  std::vector<ParsedDistributionPoint> dps;
  ASSERT_TRUE(GetCrlDps("crldp_issuer_as_dirname.pem", &dps));

  ASSERT_EQ(1u, dps.size());
  const ParsedDistributionPoint &dp1 = dps.front();
  ASSERT_TRUE(dp1.distribution_point_fullname);
  const GeneralNames &fullname = *dp1.distribution_point_fullname;
  EXPECT_EQ(GENERAL_NAME_DIRECTORY_NAME, fullname.present_name_types);

  const uint8_t kExpectedName[] = {
      0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55,
      0x53, 0x31, 0x1f, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x13, 0x16,
      0x54, 0x65, 0x73, 0x74, 0x20, 0x43, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69,
      0x63, 0x61, 0x74, 0x65, 0x73, 0x20, 0x32, 0x30, 0x31, 0x31, 0x31, 0x22,
      0x30, 0x20, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x19, 0x69, 0x6e, 0x64,
      0x69, 0x72, 0x65, 0x63, 0x74, 0x43, 0x52, 0x4c, 0x20, 0x43, 0x41, 0x33,
      0x20, 0x63, 0x52, 0x4c, 0x49, 0x73, 0x73, 0x75, 0x65, 0x72, 0x31, 0x29,
      0x30, 0x27, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x20, 0x69, 0x6e, 0x64,
      0x69, 0x72, 0x65, 0x63, 0x74, 0x20, 0x43, 0x52, 0x4c, 0x20, 0x66, 0x6f,
      0x72, 0x20, 0x69, 0x6e, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x43, 0x52,
      0x4c, 0x20, 0x43, 0x41, 0x33};
  ASSERT_EQ(1u, fullname.directory_names.size());
  EXPECT_EQ(der::Input(kExpectedName), fullname.directory_names[0]);

  EXPECT_FALSE(dp1.distribution_point_name_relative_to_crl_issuer);
  EXPECT_FALSE(dp1.reasons);

  ASSERT_TRUE(dp1.crl_issuer);

  const uint8_t kExpectedCrlIssuer[] = {
      0xa4, 0x54, 0x30, 0x52, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55,
      0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x1f, 0x30, 0x1d, 0x06,
      0x03, 0x55, 0x04, 0x0a, 0x13, 0x16, 0x54, 0x65, 0x73, 0x74, 0x20,
      0x43, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69, 0x63, 0x61, 0x74, 0x65,
      0x73, 0x20, 0x32, 0x30, 0x31, 0x31, 0x31, 0x22, 0x30, 0x20, 0x06,
      0x03, 0x55, 0x04, 0x0b, 0x13, 0x19, 0x69, 0x6e, 0x64, 0x69, 0x72,
      0x65, 0x63, 0x74, 0x43, 0x52, 0x4c, 0x20, 0x43, 0x41, 0x33, 0x20,
      0x63, 0x52, 0x4c, 0x49, 0x73, 0x73, 0x75, 0x65, 0x72};
  EXPECT_EQ(der::Input(kExpectedCrlIssuer), dp1.crl_issuer);
}

TEST_F(ParseCrlDistributionPointsTest, FullnameAsDirname) {
  std::vector<ParsedDistributionPoint> dps;
  ASSERT_TRUE(GetCrlDps("crldp_full_name_as_dirname.pem", &dps));

  ASSERT_EQ(1u, dps.size());
  const ParsedDistributionPoint &dp1 = dps.front();

  ASSERT_TRUE(dp1.distribution_point_fullname);
  const GeneralNames &fullname = *dp1.distribution_point_fullname;
  EXPECT_EQ(GENERAL_NAME_DIRECTORY_NAME, fullname.present_name_types);

  const uint8_t kExpectedName[] = {
      0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55,
      0x53, 0x31, 0x1f, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x13, 0x16,
      0x54, 0x65, 0x73, 0x74, 0x20, 0x43, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69,
      0x63, 0x61, 0x74, 0x65, 0x73, 0x20, 0x32, 0x30, 0x31, 0x31, 0x31, 0x45,
      0x30, 0x43, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x3c, 0x53, 0x65, 0x6c,
      0x66, 0x2d, 0x49, 0x73, 0x73, 0x75, 0x65, 0x64, 0x20, 0x43, 0x65, 0x72,
      0x74, 0x20, 0x44, 0x50, 0x20, 0x66, 0x6f, 0x72, 0x20, 0x42, 0x61, 0x73,
      0x69, 0x63, 0x20, 0x53, 0x65, 0x6c, 0x66, 0x2d, 0x49, 0x73, 0x73, 0x75,
      0x65, 0x64, 0x20, 0x43, 0x52, 0x4c, 0x20, 0x53, 0x69, 0x67, 0x6e, 0x69,
      0x6e, 0x67, 0x20, 0x4b, 0x65, 0x79, 0x20, 0x43, 0x41};
  ASSERT_EQ(1u, fullname.directory_names.size());
  EXPECT_EQ(der::Input(kExpectedName), fullname.directory_names[0]);

  EXPECT_FALSE(dp1.distribution_point_name_relative_to_crl_issuer);
  EXPECT_FALSE(dp1.reasons);
  EXPECT_FALSE(dp1.crl_issuer);
}

TEST_F(ParseCrlDistributionPointsTest, RelativeNameAndReasonsAndMultipleDPs) {

  const uint8_t kInputDer[] = {
      0x30, 0x37, 0x30, 0x17, 0xa0, 0x11, 0xa1, 0x0f, 0x31, 0x0d, 0x30, 0x0b,
      0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x04, 0x43, 0x52, 0x4c, 0x31, 0x81,
      0x02, 0x05, 0x60, 0x30, 0x1c, 0xa0, 0x15, 0xa0, 0x13, 0xa4, 0x11, 0x30,
      0x0f, 0x31, 0x0d, 0x30, 0x0b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x04,
      0x43, 0x52, 0x4c, 0x32, 0x81, 0x03, 0x07, 0x9f, 0x80};

  std::vector<ParsedDistributionPoint> dps;
  ASSERT_TRUE(ParseCrlDistributionPoints(der::Input(kInputDer), &dps));
  ASSERT_EQ(2u, dps.size());
  {
    const ParsedDistributionPoint &dp = dps[0];
    EXPECT_FALSE(dp.distribution_point_fullname);

    ASSERT_TRUE(dp.distribution_point_name_relative_to_crl_issuer);

    const uint8_t kExpectedRDN[] = {0x31, 0x0d, 0x30, 0x0b, 0x06,
                                    0x03, 0x55, 0x04, 0x03, 0x13,
                                    0x04, 0x43, 0x52, 0x4c, 0x31};
    EXPECT_EQ(der::Input(kExpectedRDN),
              *dp.distribution_point_name_relative_to_crl_issuer);

    ASSERT_TRUE(dp.reasons);
    const uint8_t kExpectedReasons[] = {0x05, 0x60};
    EXPECT_EQ(der::Input(kExpectedReasons), *dp.reasons);

    EXPECT_FALSE(dp.crl_issuer);
  }
  {
    const ParsedDistributionPoint &dp = dps[1];
    ASSERT_TRUE(dp.distribution_point_fullname);
    const GeneralNames &fullname = *dp.distribution_point_fullname;
    EXPECT_EQ(GENERAL_NAME_DIRECTORY_NAME, fullname.present_name_types);

    const uint8_t kExpectedName[] = {0x31, 0x0d, 0x30, 0x0b, 0x06,
                                     0x03, 0x55, 0x04, 0x03, 0x13,
                                     0x04, 0x43, 0x52, 0x4c, 0x32};
    ASSERT_EQ(1u, fullname.directory_names.size());
    EXPECT_EQ(der::Input(kExpectedName), fullname.directory_names[0]);

    EXPECT_FALSE(dp.distribution_point_name_relative_to_crl_issuer);

    ASSERT_TRUE(dp.reasons);
    const uint8_t kExpectedReasons[] = {0x07, 0x9f, 0x80};
    EXPECT_EQ(der::Input(kExpectedReasons), *dp.reasons);

    EXPECT_FALSE(dp.crl_issuer);
  }
}

TEST_F(ParseCrlDistributionPointsTest, NoDistributionPointName) {

  const uint8_t kInputDer[] = {0x30, 0x1d, 0x30, 0x1b, 0xa2, 0x19, 0xa4, 0x17,
                               0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03,
                               0x55, 0x04, 0x0b, 0x13, 0x0a, 0x63, 0x72, 0x6c,
                               0x20, 0x69, 0x73, 0x73, 0x75, 0x65, 0x72};

  std::vector<ParsedDistributionPoint> dps;
  ASSERT_TRUE(ParseCrlDistributionPoints(der::Input(kInputDer), &dps));
  ASSERT_EQ(1u, dps.size());
  const ParsedDistributionPoint &dp = dps[0];
  EXPECT_FALSE(dp.distribution_point_fullname);

  EXPECT_FALSE(dp.distribution_point_name_relative_to_crl_issuer);

  EXPECT_FALSE(dp.reasons);

  ASSERT_TRUE(dp.crl_issuer);
  const uint8_t kExpectedDer[] = {0xa4, 0x17, 0x30, 0x15, 0x31, 0x13, 0x30,
                                  0x11, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13,
                                  0x0a, 0x63, 0x72, 0x6c, 0x20, 0x69, 0x73,
                                  0x73, 0x75, 0x65, 0x72};
  EXPECT_EQ(der::Input(kExpectedDer), *dp.crl_issuer);
}

TEST_F(ParseCrlDistributionPointsTest, OnlyReasons) {

  const uint8_t kInputDer[] = {0x30, 0x06, 0x30, 0x04, 0x81, 0x02, 0x05, 0x60};

  std::vector<ParsedDistributionPoint> dps;
  EXPECT_FALSE(ParseCrlDistributionPoints(der::Input(kInputDer), &dps));
}

TEST_F(ParseCrlDistributionPointsTest, EmptyDistributionPoint) {

  const uint8_t kInputDer[] = {0x30, 0x02, 0x30, 0x00};

  std::vector<ParsedDistributionPoint> dps;
  EXPECT_FALSE(ParseCrlDistributionPoints(der::Input(kInputDer), &dps));
}

TEST_F(ParseCrlDistributionPointsTest, EmptyDistributionPoints) {

  const uint8_t kInputDer[] = {0x30, 0x00};

  std::vector<ParsedDistributionPoint> dps;
  EXPECT_FALSE(ParseCrlDistributionPoints(der::Input(kInputDer), &dps));
}

bool ParseAuthorityKeyIdentifierTestData(
    const char *file_name, std::string *backing_bytes,
    ParsedAuthorityKeyIdentifier *authority_key_identifier) {

  const PemBlockMapping mappings[] = {
      {"AUTHORITY_KEY_IDENTIFIER", backing_bytes},
  };
  std::string test_file_path =
      std::string(
          "testdata/parse_certificate_unittest/authority_key_identifier/") +
      file_name;
  EXPECT_TRUE(ReadTestDataFromPemFile(test_file_path, mappings));

  return ParseAuthorityKeyIdentifier(der::Input(*backing_bytes),
                                     authority_key_identifier);
}

TEST(ParseAuthorityKeyIdentifierTest, EmptyInput) {
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(
      ParseAuthorityKeyIdentifier(der::Input(), &authority_key_identifier));
}

TEST(ParseAuthorityKeyIdentifierTest, EmptySequence) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;

  ASSERT_TRUE(ParseAuthorityKeyIdentifierTestData(
      "empty_sequence.pem", &backing_bytes, &authority_key_identifier));

  EXPECT_FALSE(authority_key_identifier.key_identifier);
  EXPECT_FALSE(authority_key_identifier.authority_cert_issuer);
  EXPECT_FALSE(authority_key_identifier.authority_cert_serial_number);
}

TEST(ParseAuthorityKeyIdentifierTest, KeyIdentifier) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  ASSERT_TRUE(ParseAuthorityKeyIdentifierTestData(
      "key_identifier.pem", &backing_bytes, &authority_key_identifier));

  ASSERT_TRUE(authority_key_identifier.key_identifier);
  const uint8_t kExpectedValue[] = {0xDE, 0xAD, 0xB0, 0x0F};
  EXPECT_EQ(der::Input(kExpectedValue),
            authority_key_identifier.key_identifier);
}

TEST(ParseAuthorityKeyIdentifierTest, IssuerAndSerial) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  ASSERT_TRUE(ParseAuthorityKeyIdentifierTestData(
      "issuer_and_serial.pem", &backing_bytes, &authority_key_identifier));

  EXPECT_FALSE(authority_key_identifier.key_identifier);

  ASSERT_TRUE(authority_key_identifier.authority_cert_issuer);
  const uint8_t kExpectedIssuer[] = {0xa4, 0x11, 0x30, 0x0f, 0x31, 0x0d, 0x30,
                                     0x0b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
                                     0x04, 0x52, 0x6f, 0x6f, 0x74};
  EXPECT_EQ(der::Input(kExpectedIssuer),
            authority_key_identifier.authority_cert_issuer);

  ASSERT_TRUE(authority_key_identifier.authority_cert_serial_number);
  const uint8_t kExpectedSerial[] = {0x27, 0x4F};
  EXPECT_EQ(der::Input(kExpectedSerial),
            authority_key_identifier.authority_cert_serial_number);
}

TEST(ParseAuthorityKeyIdentifierTest, KeyIdentifierAndIssuerAndSerial) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  ASSERT_TRUE(ParseAuthorityKeyIdentifierTestData(
      "key_identifier_and_issuer_and_serial.pem", &backing_bytes,
      &authority_key_identifier));

  ASSERT_TRUE(authority_key_identifier.key_identifier);
  const uint8_t kExpectedValue[] = {0xDE, 0xAD, 0xB0, 0x0F};
  EXPECT_EQ(der::Input(kExpectedValue),
            authority_key_identifier.key_identifier);

  ASSERT_TRUE(authority_key_identifier.authority_cert_issuer);
  const uint8_t kExpectedIssuer[] = {0xa4, 0x11, 0x30, 0x0f, 0x31, 0x0d, 0x30,
                                     0x0b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
                                     0x04, 0x52, 0x6f, 0x6f, 0x74};
  EXPECT_EQ(der::Input(kExpectedIssuer),
            authority_key_identifier.authority_cert_issuer);

  ASSERT_TRUE(authority_key_identifier.authority_cert_serial_number);
  const uint8_t kExpectedSerial[] = {0x27, 0x4F};
  EXPECT_EQ(der::Input(kExpectedSerial),
            authority_key_identifier.authority_cert_serial_number);
}

TEST(ParseAuthorityKeyIdentifierTest, IssuerOnly) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(ParseAuthorityKeyIdentifierTestData(
      "issuer_only.pem", &backing_bytes, &authority_key_identifier));
}

TEST(ParseAuthorityKeyIdentifierTest, SerialOnly) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(ParseAuthorityKeyIdentifierTestData(
      "serial_only.pem", &backing_bytes, &authority_key_identifier));
}

TEST(ParseAuthorityKeyIdentifierTest, InvalidContents) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(ParseAuthorityKeyIdentifierTestData(
      "invalid_contents.pem", &backing_bytes, &authority_key_identifier));
}

TEST(ParseAuthorityKeyIdentifierTest, InvalidKeyIdentifier) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(ParseAuthorityKeyIdentifierTestData(
      "invalid_key_identifier.pem", &backing_bytes, &authority_key_identifier));
}

TEST(ParseAuthorityKeyIdentifierTest, InvalidIssuer) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(ParseAuthorityKeyIdentifierTestData(
      "invalid_issuer.pem", &backing_bytes, &authority_key_identifier));
}

TEST(ParseAuthorityKeyIdentifierTest, InvalidSerial) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(ParseAuthorityKeyIdentifierTestData(
      "invalid_serial.pem", &backing_bytes, &authority_key_identifier));
}

TEST(ParseAuthorityKeyIdentifierTest, ExtraContentsAfterIssuerAndSerial) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(ParseAuthorityKeyIdentifierTestData(
      "extra_contents_after_issuer_and_serial.pem", &backing_bytes,
      &authority_key_identifier));
}

TEST(ParseAuthorityKeyIdentifierTest, ExtraContentsAfterExtensionSequence) {
  std::string backing_bytes;
  ParsedAuthorityKeyIdentifier authority_key_identifier;
  EXPECT_FALSE(ParseAuthorityKeyIdentifierTestData(
      "extra_contents_after_extension_sequence.pem", &backing_bytes,
      &authority_key_identifier));
}

TEST(ParseSubjectKeyIdentifierTest, EmptyInput) {
  der::Input subject_key_identifier;
  EXPECT_FALSE(
      ParseSubjectKeyIdentifier(der::Input(), &subject_key_identifier));
}

TEST(ParseSubjectKeyIdentifierTest, Valid) {

  const uint8_t kInput[] = {0x04, 0x02, 0xab, 0xcd};
  const uint8_t kExpected[] = {0xab, 0xcd};
  der::Input subject_key_identifier;
  EXPECT_TRUE(
      ParseSubjectKeyIdentifier(der::Input(kInput), &subject_key_identifier));
  EXPECT_EQ(der::Input(kExpected), subject_key_identifier);
}

TEST(ParseSubjectKeyIdentifierTest, ExtraData) {

  const uint8_t kInput[] = {0x04, 0x02, 0xab, 0xcd, 0x05};
  der::Input subject_key_identifier;
  EXPECT_FALSE(
      ParseSubjectKeyIdentifier(der::Input(kInput), &subject_key_identifier));
}

}

BSSL_NAMESPACE_END
