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

#include "name_constraints.h"

#include <limits.h>

#include <memory>
#include <optional>

#include <openssl/base.h>
#include <openssl/bytestring.h>

#include "cert_errors.h"
#include "common_cert_errors.h"
#include "general_names.h"
#include "input.h"
#include "ip_util.h"
#include "parser.h"
#include "string_util.h"
#include "verify_name_match.h"

BSSL_NAMESPACE_BEGIN

namespace {

const int kSupportedNameTypes =
    GENERAL_NAME_RFC822_NAME | GENERAL_NAME_DNS_NAME |
    GENERAL_NAME_DIRECTORY_NAME | GENERAL_NAME_IP_ADDRESS;

enum WildcardMatchType { WILDCARD_PARTIAL_MATCH, WILDCARD_FULL_MATCH };

bool DNSNameMatches(std::string_view name, std::string_view dns_constraint,
                    WildcardMatchType wildcard_matching) {

  if (dns_constraint.empty()) {
    return true;
  }

  if (!name.empty() && *name.rbegin() == '.') {
    name.remove_suffix(1);
  }
  if (!dns_constraint.empty() && *dns_constraint.rbegin() == '.') {
    dns_constraint.remove_suffix(1);
  }

  if (wildcard_matching == WILDCARD_PARTIAL_MATCH && name.size() > 2 &&
      name[0] == '*' && name[1] == '.') {
    size_t dns_constraint_dot_pos = dns_constraint.find('.');
    if (dns_constraint_dot_pos != std::string::npos) {
      std::string_view dns_constraint_domain =
          dns_constraint.substr(dns_constraint_dot_pos + 1);
      std::string_view wildcard_domain = name.substr(2);
      if (bssl::string_util::IsEqualNoCase(wildcard_domain,
                                           dns_constraint_domain)) {
        return true;
      }
    }
  }

  if (!bssl::string_util::EndsWithNoCase(name, dns_constraint)) {
    return false;
  }

  if (name.size() == dns_constraint.size()) {
    return true;
  }

  if (!dns_constraint.empty() && dns_constraint[0] == '.') {
    dns_constraint.remove_prefix(1);
  }

  if (name.size() > dns_constraint.size() &&
      name[name.size() - dns_constraint.size() - 1] == '.') {
    return true;
  }

  return false;
}

[[nodiscard]] bool ParseGeneralSubtrees(der::Input value,
                                        GeneralNames *subtrees,
                                        CertErrors *errors) {
  BSSL_CHECK(errors);

  der::Parser sequence_parser(value);

  if (!sequence_parser.HasMore()) {
    return false;
  }
  while (sequence_parser.HasMore()) {
    der::Parser subtree_sequence;
    if (!sequence_parser.ReadSequence(&subtree_sequence)) {
      return false;
    }

    der::Input raw_general_name;
    if (!subtree_sequence.ReadRawTLV(&raw_general_name)) {
      return false;
    }

    if (!ParseGeneralName(raw_general_name,
                          GeneralNames::IP_ADDRESS_AND_NETMASK, subtrees,
                          errors)) {
      errors->AddError(kFailedParsingGeneralName);
      return false;
    }

    if (subtree_sequence.HasMore()) {
      return false;
    }
  }
  return true;
}

bool IsAlphaDigit(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z');
}

bool IsAllowedRfc822LocalPart(std::string_view local_part) {
  if (local_part.empty()) {
    return false;
  }
  for (char c : local_part) {
    if (!(IsAlphaDigit(c) || c == '!' || c == '#' || c == '$' || c == '%' ||
          c == '&' || c == '\'' || c == '*' || c == '+' || c == '-' ||
          c == '/' || c == '=' || c == '?' || c == '^' || c == '_' ||
          c == '`' || c == '{' || c == '|' || c == '}' || c == '~' ||
          c == '.')) {
      return false;
    }
  }
  return true;
}

bool IsAllowedRfc822Domain(std::string_view domain) {
  if (domain.empty()) {
    return false;
  }
  for (char c : domain) {
    if (!(IsAlphaDigit(c) || c == '-' || c == '.')) {
      return false;
    }
  }
  return true;
}

enum class Rfc822NameMatchType { kPermitted, kExcluded };
bool Rfc822NameMatches(std::string_view local_part, std::string_view domain,
                       std::string_view rfc822_constraint,
                       Rfc822NameMatchType match_type,
                       bool case_insensitive_local_part) {

  const bool error_value =
      match_type == Rfc822NameMatchType::kPermitted ? false : true;

  std::vector<std::string_view> constraint_components =
      bssl::string_util::SplitString(rfc822_constraint, '@');
  std::string_view constraint_local_part;
  std::string_view constraint_domain;
  if (constraint_components.size() == 1) {
    constraint_domain = constraint_components[0];
  } else if (constraint_components.size() == 2) {
    constraint_local_part = constraint_components[0];
    if (!IsAllowedRfc822LocalPart(constraint_local_part)) {
      return error_value;
    }
    constraint_domain = constraint_components[1];
  } else {

    return error_value;
  }
  if (!IsAllowedRfc822Domain(constraint_domain)) {
    return error_value;
  }

  if (!constraint_local_part.empty()) {
    return (case_insensitive_local_part
                ? string_util::IsEqualNoCase(local_part, constraint_local_part)
                : local_part == constraint_local_part) &&
           string_util::IsEqualNoCase(domain, constraint_domain);
  }

  if (!constraint_domain.empty() && constraint_domain[0] == '.') {
    return string_util::EndsWithNoCase(domain, constraint_domain);
  }

  return string_util::IsEqualNoCase(domain, constraint_domain);
}

}

NameConstraints::~NameConstraints() = default;

std::unique_ptr<NameConstraints> NameConstraints::Create(
    der::Input extension_value, bool is_critical, CertErrors *errors) {
  BSSL_CHECK(errors);

  auto name_constraints = std::make_unique<NameConstraints>();
  if (!name_constraints->Parse(extension_value, is_critical, errors)) {
    return nullptr;
  }
  return name_constraints;
}

std::unique_ptr<NameConstraints> NameConstraints::CreateFromPermittedSubtrees(
    GeneralNames permitted_subtrees) {
  auto name_constraints = std::make_unique<NameConstraints>();

  name_constraints->constrained_name_types_ =
      permitted_subtrees.present_name_types;
  name_constraints->permitted_subtrees_ = std::move(permitted_subtrees);

  return name_constraints;
}

bool NameConstraints::Parse(der::Input extension_value, bool is_critical,
                            CertErrors *errors) {
  BSSL_CHECK(errors);

  der::Parser extension_parser(extension_value);
  der::Parser sequence_parser;

  if (!extension_parser.ReadSequence(&sequence_parser)) {
    return false;
  }
  if (extension_parser.HasMore()) {
    return false;
  }

  std::optional<der::Input> permitted_subtrees_value;
  if (!sequence_parser.ReadOptionalTag(
          CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0,
          &permitted_subtrees_value)) {
    return false;
  }
  if (permitted_subtrees_value &&
      !ParseGeneralSubtrees(permitted_subtrees_value.value(),
                            &permitted_subtrees_, errors)) {
    return false;
  }
  constrained_name_types_ |=
      permitted_subtrees_.present_name_types &
      (is_critical ? GENERAL_NAME_ALL_TYPES : kSupportedNameTypes);

  std::optional<der::Input> excluded_subtrees_value;
  if (!sequence_parser.ReadOptionalTag(
          CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 1,
          &excluded_subtrees_value)) {
    return false;
  }
  if (excluded_subtrees_value &&
      !ParseGeneralSubtrees(excluded_subtrees_value.value(),
                            &excluded_subtrees_, errors)) {
    return false;
  }
  constrained_name_types_ |=
      excluded_subtrees_.present_name_types &
      (is_critical ? GENERAL_NAME_ALL_TYPES : kSupportedNameTypes);

  if (!permitted_subtrees_value && !excluded_subtrees_value) {
    return false;
  }

  if (sequence_parser.HasMore()) {
    return false;
  }

  return true;
}

void NameConstraints::IsPermittedCert(der::Input subject_rdn_sequence,
                                      const GeneralNames *subject_alt_names,
                                      CertErrors *errors) const {

  const size_t kMaxChecks = 1048576;

  size_t name_count = 0;

  size_t constraint_count = 0;
  if (subject_alt_names) {
    name_count = subject_alt_names->rfc822_names.size() +
                 subject_alt_names->dns_names.size() +
                 subject_alt_names->directory_names.size() +
                 subject_alt_names->ip_addresses.size();
    constraint_count = excluded_subtrees_.rfc822_names.size() +
                       permitted_subtrees_.rfc822_names.size() +
                       excluded_subtrees_.dns_names.size() +
                       permitted_subtrees_.dns_names.size() +
                       excluded_subtrees_.directory_names.size() +
                       permitted_subtrees_.directory_names.size() +
                       excluded_subtrees_.ip_address_ranges.size() +
                       permitted_subtrees_.ip_address_ranges.size();
  } else {
    constraint_count += excluded_subtrees_.directory_names.size() +
                        permitted_subtrees_.directory_names.size();
    name_count = subject_rdn_sequence.size();
  }

  size_t check_count = constraint_count * name_count;
  if ((constraint_count > 0 && check_count / constraint_count != name_count) ||
      check_count > kMaxChecks) {
    errors->AddError(cert_errors::kTooManyNameConstraintChecks);
    return;
  }

  std::vector<std::string> subject_email_addresses_to_check;
  if (!subject_alt_names &&
      (constrained_name_types() & GENERAL_NAME_RFC822_NAME)) {
    if (!FindEmailAddressesInName(subject_rdn_sequence,
                                  &subject_email_addresses_to_check)) {

      errors->AddError(cert_errors::kNotPermittedByNameConstraints);
      return;
    }
  }

  if (subject_alt_names) {

    if (constrained_name_types() & subject_alt_names->present_name_types &
        ~kSupportedNameTypes) {
      errors->AddError(cert_errors::kNotPermittedByNameConstraints);
      return;
    }

    if (constrained_name_types() & GENERAL_NAME_RFC822_NAME) {
      for (const auto &rfc822_name : subject_alt_names->rfc822_names) {
        if (!IsPermittedRfc822Name(
                rfc822_name,                                        false)) {
          errors->AddError(cert_errors::kNotPermittedByNameConstraints);
          return;
        }
      }
    }

    for (const auto &dns_name : subject_alt_names->dns_names) {
      if (!IsPermittedDNSName(dns_name)) {
        errors->AddError(cert_errors::kNotPermittedByNameConstraints);
        return;
      }
    }

    for (const auto &directory_name : subject_alt_names->directory_names) {
      if (!IsPermittedDirectoryName(directory_name)) {
        errors->AddError(cert_errors::kNotPermittedByNameConstraints);
        return;
      }
    }

    for (const auto &ip_address : subject_alt_names->ip_addresses) {
      if (!IsPermittedIP(ip_address)) {
        errors->AddError(cert_errors::kNotPermittedByNameConstraints);
        return;
      }
    }
  }

  for (const auto &rfc822_name : subject_email_addresses_to_check) {

    if (!IsPermittedRfc822Name(rfc822_name,
                                                                      true)) {
      errors->AddError(cert_errors::kNotPermittedByNameConstraints);
      return;
    }
  }

  if (subject_alt_names && subject_rdn_sequence.empty()) {
    return;
  }

  if (!IsPermittedDirectoryName(subject_rdn_sequence)) {
    errors->AddError(cert_errors::kNotPermittedByNameConstraints);
    return;
  }
}

bool NameConstraints::IsPermittedRfc822Name(
    std::string_view name, bool case_insensitive_exclude_localpart) const {

  std::vector<std::string_view> name_components =
      bssl::string_util::SplitString(name, '@');
  if (name_components.size() != 2) {

    return false;
  }
  if (!IsAllowedRfc822LocalPart(name_components[0]) ||
      !IsAllowedRfc822Domain(name_components[1])) {
    return false;
  }

  for (const auto &excluded_name : excluded_subtrees_.rfc822_names) {
    if (Rfc822NameMatches(name_components[0], name_components[1], excluded_name,
                          Rfc822NameMatchType::kExcluded,
                          case_insensitive_exclude_localpart)) {
      return false;
    }
  }

  if (!(permitted_subtrees_.present_name_types & GENERAL_NAME_RFC822_NAME)) {
    return true;
  }

  for (const auto &permitted_name : permitted_subtrees_.rfc822_names) {
    if (Rfc822NameMatches(name_components[0], name_components[1],
                          permitted_name, Rfc822NameMatchType::kPermitted,
                                                         false)) {
      return true;
    }
  }

  return false;
}

bool NameConstraints::IsPermittedDNSName(std::string_view name) const {
  for (const auto &excluded_name : excluded_subtrees_.dns_names) {

    if (DNSNameMatches(name, excluded_name, WILDCARD_PARTIAL_MATCH)) {
      return false;
    }
  }

  if (!(permitted_subtrees_.present_name_types & GENERAL_NAME_DNS_NAME)) {
    return true;
  }

  for (const auto &permitted_name : permitted_subtrees_.dns_names) {

    if (DNSNameMatches(name, permitted_name, WILDCARD_FULL_MATCH)) {
      return true;
    }
  }

  return false;
}

bool NameConstraints::IsPermittedDirectoryName(
    der::Input name_rdn_sequence) const {
  for (const auto &excluded_name : excluded_subtrees_.directory_names) {
    if (VerifyNameInSubtree(name_rdn_sequence, excluded_name)) {
      return false;
    }
  }

  if (!(permitted_subtrees_.present_name_types & GENERAL_NAME_DIRECTORY_NAME)) {
    return true;
  }

  for (const auto &permitted_name : permitted_subtrees_.directory_names) {
    if (VerifyNameInSubtree(name_rdn_sequence, permitted_name)) {
      return true;
    }
  }

  return false;
}

bool NameConstraints::IsPermittedIP(der::Input ip) const {
  for (const auto &excluded_ip : excluded_subtrees_.ip_address_ranges) {
    if (IPAddressMatchesWithNetmask(ip, excluded_ip.first,
                                    excluded_ip.second)) {
      return false;
    }
  }

  if (!(permitted_subtrees_.present_name_types & GENERAL_NAME_IP_ADDRESS)) {
    return true;
  }

  for (const auto &permitted_ip : permitted_subtrees_.ip_address_ranges) {
    if (IPAddressMatchesWithNetmask(ip, permitted_ip.first,
                                    permitted_ip.second)) {
      return true;
    }
  }

  return false;
}

BSSL_NAMESPACE_END
