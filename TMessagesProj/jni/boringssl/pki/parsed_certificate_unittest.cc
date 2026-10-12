// Copyright 2017 The Chromium Authors
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

#include "parsed_certificate.h"

#include <gtest/gtest.h>
#include <openssl/pool.h>
#include "cert_errors.h"
#include "input.h"
#include "parse_certificate.h"
#include "test_helpers.h"

BSSL_NAMESPACE_BEGIN

namespace {

std::string GetFilePath(const std::string &file_name) {
  return std::string("testdata/parse_certificate_unittest/") + file_name;
}

std::shared_ptr<const ParsedCertificate> ParseCertificateFromFile(
    const std::string &file_name, const ParseCertificateOptions &options) {
  std::string data;
  std::string expected_errors;

  const PemBlockMapping mappings[] = {
      {"CERTIFICATE", &data},
      {"ERRORS", &expected_errors, true             },
  };
  std::string test_file_path = GetFilePath(file_name);
  EXPECT_TRUE(ReadTestDataFromPemFile(test_file_path, mappings));

  CertErrors errors;
  std::shared_ptr<const ParsedCertificate> cert = ParsedCertificate::Create(
      bssl::UniquePtr<CRYPTO_BUFFER>(
          CRYPTO_BUFFER_new(reinterpret_cast<const uint8_t *>(data.data()),
                            data.size(), nullptr)),
      options, &errors);

  if (!options.allow_invalid_serial_numbers) {
    VerifyCertErrors(expected_errors, errors, test_file_path);
  }

  if (!cert) {
    EXPECT_FALSE(errors.ToDebugString().empty());
  }

  return cert;
}

der::Input DavidBenOid() {

  static const uint8_t kOid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x12,
                                 0x04, 0x01, 0x84, 0xb7, 0x09, 0x00};
  return der::Input(kOid);
}

TEST(ParsedCertificateTest, ExtensionCritical) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("extension_critical.pem", {});
  ASSERT_TRUE(cert);

  const uint8_t kExpectedValue[] = {0x30, 0x00};

  ParsedExtension extension;
  ASSERT_TRUE(cert->GetExtension(DavidBenOid(), &extension));

  EXPECT_TRUE(extension.critical);
  EXPECT_EQ(DavidBenOid(), extension.oid);
  EXPECT_EQ(der::Input(kExpectedValue), extension.value);
}

TEST(ParsedCertificateTest, ExtensionNotCritical) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("extension_not_critical.pem", {});
  ASSERT_TRUE(cert);

  const uint8_t kExpectedValue[] = {0x30, 0x00};

  ParsedExtension extension;
  ASSERT_TRUE(cert->GetExtension(DavidBenOid(), &extension));

  EXPECT_FALSE(extension.critical);
  EXPECT_EQ(DavidBenOid(), extension.oid);
  EXPECT_EQ(der::Input(kExpectedValue), extension.value);
}

TEST(ParsedCertificateTest, ExtensionCritical0) {
  ASSERT_FALSE(ParseCertificateFromFile("extension_critical_0.pem", {}));
}

TEST(ParsedCertificateTest, ExtensionCritical3) {
  ASSERT_FALSE(ParseCertificateFromFile("extension_critical_3.pem", {}));
}

TEST(ParsedCertificateTest, ExtensionsEmptySequence) {
  ASSERT_FALSE(ParseCertificateFromFile("extensions_empty_sequence.pem", {}));
}

TEST(ParsedCertificateTest, ExtensionsNotSequence) {
  ASSERT_FALSE(ParseCertificateFromFile("extensions_not_sequence.pem", {}));
}

TEST(ParsedCertificateTest, ExtensionsDataAfterSequence) {
  ASSERT_FALSE(
      ParseCertificateFromFile("extensions_data_after_sequence.pem", {}));
}

TEST(ParsedCertificateTest, ExtensionsDuplicateKeyUsage) {
  ASSERT_FALSE(
      ParseCertificateFromFile("extensions_duplicate_key_usage.pem", {}));
}

TEST(ParsedCertificateTest, BadKeyUsage) {
  ASSERT_FALSE(ParseCertificateFromFile("bad_key_usage.pem", {}));
}

TEST(ParsedCertificateTest, BadPolicyQualifiers) {
  ASSERT_FALSE(ParseCertificateFromFile("bad_policy_qualifiers.pem", {}));
}

TEST(ParsedCertificateTest, BadSignatureAlgorithmOid) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("bad_signature_algorithm_oid.pem", {});
  ASSERT_TRUE(cert);
  ASSERT_FALSE(cert->signature_algorithm());
}

TEST(ParsedCertificateTest, BadValidity) {
  ASSERT_FALSE(ParseCertificateFromFile("bad_validity.pem", {}));
}

TEST(ParsedCertificateTest, FailedSignatureAlgorithm) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("failed_signature_algorithm.pem", {});
  ASSERT_TRUE(cert);
  ASSERT_FALSE(cert->signature_algorithm());
}

TEST(ParsedCertificateTest, IssuerBadPrintableString) {
  ASSERT_FALSE(ParseCertificateFromFile("issuer_bad_printable_string.pem", {}));
}

TEST(ParsedCertificateTest, NameConstraintsBadIp) {
  ASSERT_FALSE(ParseCertificateFromFile("name_constraints_bad_ip.pem", {}));
}

TEST(ParsedCertificateTest, PolicyQualifiersEmptySequence) {
  ASSERT_FALSE(
      ParseCertificateFromFile("policy_qualifiers_empty_sequence.pem", {}));
}

TEST(ParsedCertificateTest, SubjectBlankSubjectAltNameNotCritical) {
  ASSERT_FALSE(ParseCertificateFromFile(
      "subject_blank_subjectaltname_not_critical.pem", {}));
}

TEST(ParsedCertificateTest, SubjectNotAscii) {
  ASSERT_FALSE(ParseCertificateFromFile("subject_not_ascii.pem", {}));
}

TEST(ParsedCertificateTest, SubjectNotPrintableString) {
  ASSERT_FALSE(
      ParseCertificateFromFile("subject_not_printable_string.pem", {}));
}

TEST(ParsedCertificateTest, SubjectAltNameBadIp) {
  ASSERT_FALSE(ParseCertificateFromFile("subjectaltname_bad_ip.pem", {}));
}

TEST(ParsedCertificateTest, SubjectAltNameDnsNotAscii) {
  ASSERT_FALSE(
      ParseCertificateFromFile("subjectaltname_dns_not_ascii.pem", {}));
}

TEST(ParsedCertificateTest, SubjectAltNameGeneralNamesEmptySequence) {
  ASSERT_FALSE(ParseCertificateFromFile(
      "subjectaltname_general_names_empty_sequence.pem", {}));
}

TEST(ParsedCertificateTest, SubjectAltNameTrailingData) {
  ASSERT_FALSE(
      ParseCertificateFromFile("subjectaltname_trailing_data.pem", {}));
}

TEST(ParsedCertificateTest, V1ExplicitVersion) {
  ASSERT_FALSE(ParseCertificateFromFile("v1_explicit_version.pem", {}));
}

TEST(ParsedCertificateTest, ExtendedKeyUsage) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("extended_key_usage.pem", {});
  ASSERT_TRUE(cert);

  ASSERT_EQ(4u, cert->extensions().size());

  ParsedExtension extension;
  ASSERT_TRUE(cert->GetExtension(der::Input(kExtKeyUsageOid), &extension));

  EXPECT_FALSE(extension.critical);
  EXPECT_EQ(45u, extension.value.size());

  EXPECT_TRUE(cert->has_extended_key_usage());
  EXPECT_EQ(4u, cert->extended_key_usage().size());
}

TEST(ParsedCertificateTest, KeyUsage) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("key_usage.pem", {});
  ASSERT_TRUE(cert);

  ASSERT_TRUE(cert->has_key_usage());

  EXPECT_EQ(5u, cert->key_usage().unused_bits());
  const uint8_t kExpectedBytes[] = {0xA0};
  EXPECT_EQ(der::Input(kExpectedBytes), cert->key_usage().bytes());

  EXPECT_TRUE(cert->key_usage().AssertsBit(0));
  EXPECT_FALSE(cert->key_usage().AssertsBit(1));
  EXPECT_TRUE(cert->key_usage().AssertsBit(2));
}

TEST(ParsedCertificateTest, Policies) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("policies.pem", {});
  ASSERT_TRUE(cert);

  ASSERT_EQ(4u, cert->extensions().size());

  ParsedExtension extension;
  ASSERT_TRUE(
      cert->GetExtension(der::Input(kCertificatePoliciesOid), &extension));

  EXPECT_FALSE(extension.critical);
  EXPECT_EQ(95u, extension.value.size());

  EXPECT_TRUE(cert->has_policy_oids());
  EXPECT_EQ(2u, cert->policy_oids().size());
}

TEST(ParsedCertificateTest, SubjectAltName) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("subject_alt_name.pem", {});
  ASSERT_TRUE(cert);

  ASSERT_TRUE(cert->has_subject_alt_names());
}

TEST(ParsedCertificateTest, ExtensionsReal) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("extensions_real.pem", {});
  ASSERT_TRUE(cert);

  ASSERT_EQ(7u, cert->extensions().size());

  EXPECT_TRUE(cert->has_key_usage());
  EXPECT_TRUE(cert->has_basic_constraints());
  EXPECT_TRUE(cert->has_authority_info_access());
  EXPECT_TRUE(cert->has_policy_oids());

  ASSERT_TRUE(cert->authority_key_identifier());
  ASSERT_TRUE(cert->authority_key_identifier()->key_identifier);
  EXPECT_FALSE(cert->authority_key_identifier()->authority_cert_issuer);
  EXPECT_FALSE(cert->authority_key_identifier()->authority_cert_serial_number);
  const uint8_t expected_authority_key_identifier[] = {
      0xc0, 0x7a, 0x98, 0x68, 0x8d, 0x89, 0xfb, 0xab, 0x05, 0x64,
      0x0c, 0x11, 0x7d, 0xaa, 0x7d, 0x65, 0xb8, 0xca, 0xcc, 0x4e,
  };
  EXPECT_EQ(der::Input(expected_authority_key_identifier),
            cert->authority_key_identifier()->key_identifier);

  ASSERT_TRUE(cert->subject_key_identifier());
  const uint8_t expected_subject_key_identifier[] = {
      0x4a, 0xdd, 0x06, 0x16, 0x1b, 0xbc, 0xf6, 0x68, 0xb5, 0x76,
      0xf5, 0x81, 0xb6, 0xbb, 0x62, 0x1a, 0xba, 0x5a, 0x81, 0x2f};
  EXPECT_EQ(der::Input(expected_subject_key_identifier),
            cert->subject_key_identifier());

  ParsedExtension extension;
  ASSERT_TRUE(
      cert->GetExtension(der::Input(kCertificatePoliciesOid), &extension));

  EXPECT_FALSE(extension.critical);
  EXPECT_EQ(16u, extension.value.size());

}

TEST(ParsedCertificateTest, BasicConstraintsNotCa) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("basic_constraints_not_ca.pem", {});
  ASSERT_TRUE(cert);

  EXPECT_TRUE(cert->has_basic_constraints());
  EXPECT_FALSE(cert->basic_constraints().is_ca);
  EXPECT_FALSE(cert->basic_constraints().has_path_len);
}

TEST(ParsedCertificateTest, BasicConstraintsCaNoPath) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("basic_constraints_ca_no_path.pem", {});
  ASSERT_TRUE(cert);

  EXPECT_TRUE(cert->has_basic_constraints());
  EXPECT_TRUE(cert->basic_constraints().is_ca);
  EXPECT_FALSE(cert->basic_constraints().has_path_len);
}

TEST(ParsedCertificateTest, BasicConstraintsCaPath9) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("basic_constraints_ca_path_9.pem", {});
  ASSERT_TRUE(cert);

  EXPECT_TRUE(cert->has_basic_constraints());
  EXPECT_TRUE(cert->basic_constraints().is_ca);
  EXPECT_TRUE(cert->basic_constraints().has_path_len);
  EXPECT_EQ(9u, cert->basic_constraints().path_len);
}

TEST(ParsedCertificateTest, BasicConstraintsPathlen255) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("basic_constraints_pathlen_255.pem", {});
  ASSERT_TRUE(cert);

  EXPECT_TRUE(cert->has_basic_constraints());
  EXPECT_TRUE(cert->basic_constraints().is_ca);
  EXPECT_TRUE(cert->basic_constraints().has_path_len);
  EXPECT_EQ(255, cert->basic_constraints().path_len);
}

TEST(ParsedCertificateTest, BasicConstraintsPathlen256) {
  ASSERT_FALSE(
      ParseCertificateFromFile("basic_constraints_pathlen_256.pem", {}));
}

TEST(ParsedCertificateTest, BasicConstraintsNegativePath) {
  ASSERT_FALSE(
      ParseCertificateFromFile("basic_constraints_negative_path.pem", {}));
}

TEST(ParsedCertificateTest, BasicConstraintsPathTooLarge) {
  ASSERT_FALSE(
      ParseCertificateFromFile("basic_constraints_path_too_large.pem", {}));
}

TEST(ParsedCertificateTest, BasicConstraintsCaFalse) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("basic_constraints_ca_false.pem", {});
  ASSERT_TRUE(cert);

  EXPECT_TRUE(cert->has_basic_constraints());
  EXPECT_FALSE(cert->basic_constraints().is_ca);
  EXPECT_FALSE(cert->basic_constraints().has_path_len);
}

TEST(ParsedCertificateTest, BasicConstraintsUnconsumedData) {
  ASSERT_FALSE(
      ParseCertificateFromFile("basic_constraints_unconsumed_data.pem", {}));
}

TEST(ParsedCertificateTest, BasicConstraintsPathLenButNotCa) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("basic_constraints_pathlen_not_ca.pem", {});
  ASSERT_TRUE(cert);

  EXPECT_TRUE(cert->has_basic_constraints());
  EXPECT_FALSE(cert->basic_constraints().is_ca);
  EXPECT_TRUE(cert->basic_constraints().has_path_len);
  EXPECT_EQ(1u, cert->basic_constraints().path_len);
}

TEST(ParsedCertificateTest, PolicyConstraintsRequire) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("policy_constraints_require.pem", {});
  ASSERT_TRUE(cert);

  EXPECT_TRUE(cert->has_policy_constraints());
  EXPECT_TRUE(cert->policy_constraints().require_explicit_policy.has_value());
  EXPECT_EQ(3, cert->policy_constraints().require_explicit_policy.value());
  EXPECT_FALSE(cert->policy_constraints().inhibit_policy_mapping.has_value());
}

TEST(ParsedCertificateTest, PolicyConstraintsInhibit) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("policy_constraints_inhibit.pem", {});
  ASSERT_TRUE(cert);

  EXPECT_TRUE(cert->has_policy_constraints());
  EXPECT_FALSE(cert->policy_constraints().require_explicit_policy.has_value());
  EXPECT_TRUE(cert->policy_constraints().inhibit_policy_mapping.has_value());
  EXPECT_EQ(1, cert->policy_constraints().inhibit_policy_mapping.value());
}

TEST(ParsedCertificateTest, PolicyConstraintsInhibitRequire) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("policy_constraints_inhibit_require.pem", {});
  ASSERT_TRUE(cert);

  EXPECT_TRUE(cert->has_policy_constraints());
  EXPECT_TRUE(cert->policy_constraints().require_explicit_policy.has_value());
  EXPECT_EQ(5, cert->policy_constraints().require_explicit_policy.value());
  EXPECT_TRUE(cert->policy_constraints().inhibit_policy_mapping.has_value());
  EXPECT_EQ(2, cert->policy_constraints().inhibit_policy_mapping.value());
}

TEST(ParsedCertificateTest, PolicyConstraintsEmpty) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("policy_constraints_empty.pem", {});
  ASSERT_FALSE(cert);
}

TEST(ParsedCertificateTest, SerialNumberZeroPadded) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("serial_zero_padded.pem", {});
  ASSERT_TRUE(cert);

  static const uint8_t expected_serial[3] = {0x00, 0x80, 0x01};
  EXPECT_EQ(der::Input(expected_serial), cert->tbs().serial_number);
}

TEST(ParsedCertificateTest, SerialNumberZeroPadded21BytesLong) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("serial_zero_padded_21_bytes.pem", {});
  ASSERT_FALSE(cert);

  ParseCertificateOptions options;
  options.allow_invalid_serial_numbers = true;
  cert = ParseCertificateFromFile("serial_zero_padded_21_bytes.pem", options);
  ASSERT_TRUE(cert);

  static const uint8_t expected_serial[21] = {
      0x00, 0x80, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
      0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13};
  EXPECT_EQ(der::Input(expected_serial), cert->tbs().serial_number);
}

TEST(ParsedCertificateTest, SerialNumberNegative) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("serial_negative.pem", {});
  ASSERT_TRUE(cert);

  static const uint8_t expected_serial[2] = {0x80, 0x01};
  EXPECT_EQ(der::Input(expected_serial), cert->tbs().serial_number);
}

TEST(ParsedCertificateTest, SerialNumber37BytesLong) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("serial_37_bytes.pem", {});
  ASSERT_FALSE(cert);

  ParseCertificateOptions options;
  options.allow_invalid_serial_numbers = true;
  cert = ParseCertificateFromFile("serial_37_bytes.pem", options);
  ASSERT_TRUE(cert);

  static const uint8_t expected_serial[37] = {
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
      0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
      0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e,
      0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25};
  EXPECT_EQ(der::Input(expected_serial), cert->tbs().serial_number);
}

TEST(ParsedCertificateTest, SerialNumberZero) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("serial_zero.pem", {});
  ASSERT_TRUE(cert);

  static const uint8_t expected_serial[] = {0x00};
  EXPECT_EQ(der::Input(expected_serial), cert->tbs().serial_number);
}

TEST(ParsedCertificateTest, SerialNotNumber) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("serial_not_number.pem", {});
  ASSERT_FALSE(cert);
}

TEST(ParsedCertificateTest, SerialNotMinimal) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("serial_not_minimal.pem", {});
  ASSERT_FALSE(cert);
}

TEST(ParsedCertificateTest, InhibitAnyPolicy) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("inhibit_any_policy.pem", {});
  ASSERT_TRUE(cert);

  ParsedExtension extension;
  ASSERT_TRUE(cert->GetExtension(der::Input(kInhibitAnyPolicyOid), &extension));

  std::optional<uint8_t> skip_count = ParseInhibitAnyPolicy(extension.value);
  ASSERT_TRUE(skip_count.has_value());
  EXPECT_EQ(3, skip_count.value());
}

TEST(ParsedCertificateTest, SubjectKeyIdentifierNotOctetString) {
  std::shared_ptr<const ParsedCertificate> cert = ParseCertificateFromFile(
      "subject_key_identifier_not_octet_string.pem", {});
  ASSERT_FALSE(cert);
}

TEST(ParsedCertificateTest, AuthourityKeyIdentifierNotSequence) {
  std::shared_ptr<const ParsedCertificate> cert =
      ParseCertificateFromFile("authority_key_identifier_not_sequence.pem", {});
  ASSERT_FALSE(cert);
}

}

BSSL_NAMESPACE_END
