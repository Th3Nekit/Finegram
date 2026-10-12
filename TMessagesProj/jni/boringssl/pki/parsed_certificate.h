// Copyright 2016 The Chromium Authors
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

#ifndef BSSL_PKI_PARSED_CERTIFICATE_H_
#define BSSL_PKI_PARSED_CERTIFICATE_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <openssl/base.h>

#include "certificate_policies.h"
#include "input.h"
#include "parse_certificate.h"
#include "signature_algorithm.h"

BSSL_NAMESPACE_BEGIN

struct GeneralNames;
class NameConstraints;
class ParsedCertificate;
class CertErrors;

using ParsedCertificateList =
    std::vector<std::shared_ptr<const ParsedCertificate>>;

class OPENSSL_EXPORT ParsedCertificate {
 private:

  class PrivateConstructor {
   private:
    friend ParsedCertificate;
    PrivateConstructor() = default;
  };

 public:
  ~ParsedCertificate();

  using ExtensionsMap = std::map<der::Input, ParsedExtension>;

  static std::shared_ptr<const ParsedCertificate> Create(
      bssl::UniquePtr<CRYPTO_BUFFER> cert_data,
      const ParseCertificateOptions &options, CertErrors *errors);

  static bool CreateAndAddToVector(
      bssl::UniquePtr<CRYPTO_BUFFER> cert_data,
      const ParseCertificateOptions &options,
      std::vector<std::shared_ptr<const bssl::ParsedCertificate>> *chain,
      CertErrors *errors);

  explicit ParsedCertificate(PrivateConstructor);

  ParsedCertificate(const ParsedCertificate &) = delete;
  ParsedCertificate &operator=(const ParsedCertificate &) = delete;

  der::Input der_cert() const { return cert_; }

  CRYPTO_BUFFER *cert_buffer() const { return cert_data_.get(); }

  der::Input tbs_certificate_tlv() const { return tbs_certificate_tlv_; }

  der::Input signature_algorithm_tlv() const {
    return signature_algorithm_tlv_;
  }

  const der::BitString &signature_value() const { return signature_value_; }

  const ParsedTbsCertificate &tbs() const { return tbs_; }

  std::optional<SignatureAlgorithm> signature_algorithm() const {
    return signature_algorithm_;
  }

  der::Input subject_tlv() const { return tbs_.subject_tlv; }

  der::Input normalized_subject() const {
    return der::Input(normalized_subject_);
  }

  der::Input issuer_tlv() const { return tbs_.issuer_tlv; }

  der::Input normalized_issuer() const {
    return der::Input(normalized_issuer_);
  }

  bool has_basic_constraints() const { return has_basic_constraints_; }

  const ParsedBasicConstraints &basic_constraints() const {
    BSSL_CHECK(has_basic_constraints_);
    return basic_constraints_;
  }

  bool has_key_usage() const { return has_key_usage_; }

  const der::BitString &key_usage() const {
    BSSL_CHECK(has_key_usage_);
    return key_usage_;
  }

  bool has_extended_key_usage() const { return has_extended_key_usage_; }

  const std::vector<der::Input> &extended_key_usage() const {
    BSSL_CHECK(has_extended_key_usage_);
    return extended_key_usage_;
  }

  bool has_subject_alt_names() const { return subject_alt_names_ != nullptr; }

  const ParsedExtension &subject_alt_names_extension() const {
    return subject_alt_names_extension_;
  }

  const GeneralNames *subject_alt_names() const {
    return subject_alt_names_.get();
  }

  bool has_name_constraints() const { return name_constraints_ != nullptr; }

  const NameConstraints &name_constraints() const {
    BSSL_CHECK(name_constraints_);
    return *name_constraints_;
  }

  bool has_authority_info_access() const { return has_authority_info_access_; }

  const ParsedExtension &authority_info_access_extension() const {
    return authority_info_access_extension_;
  }

  const std::vector<std::string_view> &ca_issuers_uris() const {
    return ca_issuers_uris_;
  }

  const std::vector<std::string_view> &ocsp_uris() const { return ocsp_uris_; }

  bool has_policy_oids() const { return has_policy_oids_; }

  const std::vector<der::Input> &policy_oids() const {
    BSSL_CHECK(has_policy_oids());
    return policy_oids_;
  }

  bool has_policy_constraints() const { return has_policy_constraints_; }

  const ParsedPolicyConstraints &policy_constraints() const {
    BSSL_CHECK(has_policy_constraints_);
    return policy_constraints_;
  }

  bool has_policy_mappings() const { return has_policy_mappings_; }

  const std::vector<ParsedPolicyMapping> &policy_mappings() const {
    BSSL_CHECK(has_policy_mappings_);
    return policy_mappings_;
  }

  const std::optional<uint8_t> &inhibit_any_policy() const {
    return inhibit_any_policy_;
  }

  const std::optional<ParsedAuthorityKeyIdentifier> &authority_key_identifier()
      const {
    return authority_key_identifier_;
  }

  const std::optional<der::Input> &subject_key_identifier() const {
    return subject_key_identifier_;
  }

  const ExtensionsMap &extensions() const { return extensions_; }

  bool GetExtension(der::Input extension_oid,
                    ParsedExtension *parsed_extension) const;

 private:

  bssl::UniquePtr<CRYPTO_BUFFER> cert_data_;

  der::Input cert_;

  der::Input tbs_certificate_tlv_;
  der::Input signature_algorithm_tlv_;
  der::BitString signature_value_;
  ParsedTbsCertificate tbs_;

  std::optional<SignatureAlgorithm> signature_algorithm_;

  std::string normalized_subject_;

  std::string normalized_issuer_;

  bool has_basic_constraints_ = false;
  ParsedBasicConstraints basic_constraints_;

  bool has_key_usage_ = false;
  der::BitString key_usage_;

  bool has_extended_key_usage_ = false;
  std::vector<der::Input> extended_key_usage_;

  ParsedExtension subject_alt_names_extension_;

  std::unique_ptr<GeneralNames> subject_alt_names_;

  std::unique_ptr<NameConstraints> name_constraints_;

  bool has_authority_info_access_ = false;
  ParsedExtension authority_info_access_extension_;

  std::vector<std::string_view> ca_issuers_uris_;
  std::vector<std::string_view> ocsp_uris_;

  bool has_policy_oids_ = false;
  std::vector<der::Input> policy_oids_;

  bool has_policy_constraints_ = false;
  ParsedPolicyConstraints policy_constraints_;

  bool has_policy_mappings_ = false;
  std::vector<ParsedPolicyMapping> policy_mappings_;

  std::optional<uint8_t> inhibit_any_policy_;

  std::optional<ParsedAuthorityKeyIdentifier> authority_key_identifier_;

  std::optional<der::Input> subject_key_identifier_;

  ExtensionsMap extensions_;
};

BSSL_NAMESPACE_END

#endif
