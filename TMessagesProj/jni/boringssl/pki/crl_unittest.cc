// Copyright 2019 The Chromium Authors
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

#include "crl.h"

#include <string_view>

#include <gtest/gtest.h>
#include <openssl/pool.h>
#include "cert_errors.h"
#include "parsed_certificate.h"
#include "string_util.h"
#include "test_helpers.h"

BSSL_NAMESPACE_BEGIN

namespace {

constexpr int64_t kAgeOneWeek = 7 * 24 * 60 * 60;

std::string GetFilePath(std::string_view file_name) {
  return std::string("testdata/crl_unittest/") + std::string(file_name);
}

std::shared_ptr<const ParsedCertificate> ParseCertificate(
    std::string_view data) {
  CertErrors errors;
  return ParsedCertificate::Create(
      bssl::UniquePtr<CRYPTO_BUFFER>(
          CRYPTO_BUFFER_new(reinterpret_cast<const uint8_t *>(data.data()),
                            data.size(), nullptr)),
      {}, &errors);
}

class CheckCRLTest : public ::testing::TestWithParam<const char *> {};

constexpr char const *kTestParams[] = {
    "good.pem",
    "good_issuer_name_normalization.pem",
    "good_issuer_no_keyusage.pem",
    "good_no_nextupdate.pem",
    "good_fake_extension.pem",
    "good_fake_extension_no_nextupdate.pem",
    "good_generalizedtime.pem",
    "good_no_version.pem",
    "good_no_crldp.pem",
    "good_key_rollover.pem",
    "good_idp_contains_uri.pem",
    "good_idp_onlycontainsusercerts.pem",
    "good_idp_onlycontainsusercerts_no_basic_constraints.pem",
    "good_idp_onlycontainscacerts.pem",
    "good_idp_uri_and_onlycontainsusercerts.pem",
    "good_idp_uri_and_onlycontainscacerts.pem",
    "revoked.pem",
    "revoked_no_nextupdate.pem",
    "revoked_fake_crlentryextension.pem",
    "revoked_generalized_revocationdate.pem",
    "revoked_key_rollover.pem",
    "bad_crldp_has_crlissuer.pem",
    "bad_fake_critical_extension.pem",
    "bad_fake_critical_crlentryextension.pem",
    "bad_signature.pem",
    "bad_thisupdate_in_future.pem",
    "bad_thisupdate_too_old.pem",
    "bad_nextupdate_too_old.pem",
    "bad_wrong_issuer.pem",
    "bad_key_rollover_signature.pem",
    "bad_idp_contains_wrong_uri.pem",
    "bad_idp_indirectcrl.pem",
    "bad_idp_onlycontainsusercerts.pem",
    "bad_idp_onlycontainscacerts.pem",
    "bad_idp_onlycontainscacerts_no_basic_constraints.pem",
    "bad_idp_uri_and_onlycontainsusercerts.pem",
    "bad_idp_uri_and_onlycontainscacerts.pem",
    "invalid_mismatched_signature_algorithm.pem",
    "invalid_revoked_empty_sequence.pem",
    "invalid_v1_with_extension.pem",
    "invalid_v1_with_crlentryextension.pem",
    "invalid_v1_explicit.pem",
    "invalid_v3.pem",
    "invalid_issuer_keyusage_no_crlsign.pem",
    "invalid_key_rollover_issuer_keyusage_no_crlsign.pem",
    "invalid_garbage_version.pem",
    "invalid_garbage_tbs_signature_algorithm.pem",
    "invalid_garbage_issuer_name.pem",
    "invalid_garbage_thisupdate.pem",
    "invalid_garbage_after_thisupdate.pem",
    "invalid_garbage_after_nextupdate.pem",
    "invalid_garbage_after_revokedcerts.pem",
    "invalid_garbage_after_extensions.pem",
    "invalid_garbage_tbscertlist.pem",
    "invalid_garbage_signaturealgorithm.pem",
    "invalid_garbage_signaturevalue.pem",
    "invalid_garbage_after_signaturevalue.pem",
    "invalid_garbage_revoked_serial_number.pem",
    "invalid_garbage_revocationdate.pem",
    "invalid_garbage_after_revocationdate.pem",
    "invalid_garbage_after_crlentryextensions.pem",
    "invalid_garbage_crlentry.pem",
    "invalid_idp_dpname_choice_extra_data.pem",
    "invalid_idp_empty_sequence.pem",
    "invalid_idp_onlycontains_user_and_ca_certs.pem",
    "invalid_idp_onlycontainsusercerts_v1_leaf.pem",
};

struct PrintTestName {
  std::string operator()(
      const testing::TestParamInfo<const char *> &info) const {
    std::string_view name(info.param);

    name.remove_suffix(4);
    return std::string(name);
  }
};

INSTANTIATE_TEST_SUITE_P(All, CheckCRLTest, ::testing::ValuesIn(kTestParams),
                         PrintTestName());

TEST_P(CheckCRLTest, FromFile) {
  std::string_view file_name(GetParam());

  std::string crl_data;
  std::string ca_data_2;
  std::string ca_data;
  std::string cert_data;
  const PemBlockMapping mappings[] = {
      {"CRL", &crl_data},
      {"CA CERTIFICATE 2", &ca_data_2,              true},
      {"CA CERTIFICATE", &ca_data},
      {"CERTIFICATE", &cert_data},
  };

  ASSERT_TRUE(ReadTestDataFromPemFile(GetFilePath(file_name), mappings));

  std::shared_ptr<const ParsedCertificate> cert = ParseCertificate(cert_data);
  ASSERT_TRUE(cert);
  std::shared_ptr<const ParsedCertificate> issuer_cert =
      ParseCertificate(ca_data);
  ASSERT_TRUE(issuer_cert);
  ParsedCertificateList certs = {cert, issuer_cert};
  if (!ca_data_2.empty()) {
    std::shared_ptr<const ParsedCertificate> issuer_cert_2 =
        ParseCertificate(ca_data_2);
    ASSERT_TRUE(issuer_cert_2);
    certs.push_back(issuer_cert_2);
  }

  ParsedDistributionPoint fake_cert_dp;
  ParsedDistributionPoint *cert_dp = &fake_cert_dp;
  std::vector<ParsedDistributionPoint> distribution_points;
  ParsedExtension crl_dp_extension;
  if (cert->GetExtension(der::Input(kCrlDistributionPointsOid),
                         &crl_dp_extension)) {
    ASSERT_TRUE(ParseCrlDistributionPoints(crl_dp_extension.value,
                                           &distribution_points));
    ASSERT_LE(distribution_points.size(), 1U);
    if (!distribution_points.empty()) {
      cert_dp = &distribution_points[0];
    }
  }
  ASSERT_TRUE(cert_dp);

  int64_t kVerifyTime = 1489017600;

  CRLRevocationStatus expected_revocation_status = CRLRevocationStatus::UNKNOWN;
  if (string_util::StartsWith(file_name, "good")) {
    expected_revocation_status = CRLRevocationStatus::GOOD;
  } else if (string_util::StartsWith(file_name, "revoked")) {
    expected_revocation_status = CRLRevocationStatus::REVOKED;
  }

  CRLRevocationStatus revocation_status =
      CheckCRL(crl_data, certs,                       0, *cert_dp, kVerifyTime,
               kAgeOneWeek);
  EXPECT_EQ(expected_revocation_status, revocation_status);

  ParsedCertificateList other_certs;
  ASSERT_TRUE(ReadCertChainFromFile(
      "testdata/parse_certificate_unittest/cert_version3.pem", &other_certs));
  ASSERT_FALSE(other_certs.empty());
  certs.insert(certs.begin(), other_certs[0]);
  revocation_status = CheckCRL(crl_data, certs,                       1,
                               *cert_dp, kVerifyTime, kAgeOneWeek);
  EXPECT_EQ(expected_revocation_status, revocation_status);
}

}

BSSL_NAMESPACE_END
