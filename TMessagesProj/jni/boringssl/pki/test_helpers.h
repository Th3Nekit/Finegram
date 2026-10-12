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

#ifndef BSSL_PKI_TEST_HELPERS_H_
#define BSSL_PKI_TEST_HELPERS_H_

#include <stddef.h>

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include "input.h"
#include "parsed_certificate.h"
#include "simple_path_builder_delegate.h"
#include "trust_store.h"
#include "verify_certificate_chain.h"

BSSL_NAMESPACE_BEGIN

namespace der {

void PrintTo(Input data, ::std::ostream *os);

}

der::Input SequenceValueFromString(std::string_view s);

struct PemBlockMapping {

  const char *block_name;

  std::string *value;

  bool optional = false;
};

::testing::AssertionResult ReadTestDataFromPemFile(
    const std::string &file_path_ascii, const PemBlockMapping *mappings,
    size_t mappings_length);

template <size_t N>
::testing::AssertionResult ReadTestDataFromPemFile(
    const std::string &file_path_ascii, const PemBlockMapping (&mappings)[N]) {
  return ReadTestDataFromPemFile(file_path_ascii, mappings, N);
}

struct VerifyCertChainTest {
  VerifyCertChainTest();
  ~VerifyCertChainTest();

  ParsedCertificateList chain;

  CertificateTrust last_cert_trust;

  der::GeneralizedTime time;

  KeyPurpose key_purpose = KeyPurpose::ANY_EKU;

  InitialExplicitPolicy initial_explicit_policy = InitialExplicitPolicy::kFalse;

  std::set<der::Input> user_initial_policy_set;

  InitialPolicyMappingInhibit initial_policy_mapping_inhibit =
      InitialPolicyMappingInhibit::kFalse;

  InitialAnyPolicyInhibit initial_any_policy_inhibit =
      InitialAnyPolicyInhibit::kFalse;

  std::string expected_errors;

  std::set<std::string> expected_user_constrained_policy_set;

  SimplePathBuilderDelegate::DigestPolicy digest_policy =
      SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1;

  bool HasHighSeverityErrors() const;
};

bool ReadVerifyCertChainTestFromFile(const std::string &file_path_ascii,
                                     VerifyCertChainTest *test);

bool ReadCertChainFromFile(const std::string &file_path_ascii,
                           ParsedCertificateList *chain);

std::shared_ptr<const ParsedCertificate> ReadCertFromFile(
    const std::string &file_path_ascii);

std::string ReadTestFileToString(const std::string &file_path_ascii);

void VerifyCertPathErrors(const std::string &expected_errors_str,
                          const CertPathErrors &actual_errors,
                          const ParsedCertificateList &chain,
                          const std::string &errors_file_path);

void VerifyCertErrors(const std::string &expected_errors_str,
                      const CertErrors &actual_errors,
                      const std::string &errors_file_path);

void VerifyUserConstrainedPolicySet(
    const std::set<std::string> &expected_user_constrained_policy_str_set,
    const std::set<der::Input> &actual_user_constrained_policy_set,
    const std::string &errors_file_path);

BSSL_NAMESPACE_END

#endif
