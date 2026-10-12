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

#include "verify_name_match.h"

#include <openssl/base.h>
#include <openssl/bytestring.h>

#include "cert_error_params.h"
#include "cert_errors.h"
#include "input.h"
#include "parse_name.h"
#include "parser.h"

BSSL_NAMESPACE_BEGIN

DEFINE_CERT_ERROR_ID(kFailedConvertingAttributeValue,
                     "Failed converting AttributeValue to string");
DEFINE_CERT_ERROR_ID(kFailedNormalizingString, "Failed normalizing string");

namespace {

enum CharsetEnforcement {
  NO_ENFORCEMENT,
  ENFORCE_PRINTABLE_STRING,
  ENFORCE_ASCII,
};

[[nodiscard]] bool NormalizeDirectoryString(
    CharsetEnforcement charset_enforcement, std::string *output) {

  std::string::const_iterator read_iter = output->begin();
  std::string::iterator write_iter = output->begin();

  for (; read_iter != output->end() && *read_iter == ' '; ++read_iter) {

  }

  for (; read_iter != output->end(); ++read_iter) {
    const unsigned char c = *read_iter;
    if (c == ' ') {

      std::string::const_iterator next_iter = read_iter + 1;
      if (next_iter != output->end() && *next_iter != ' ') {
        *(write_iter++) = ' ';
      }
    } else if (c >= 'A' && c <= 'Z') {

      *(write_iter++) = c + ('a' - 'A');
    } else {

      switch (charset_enforcement) {
        case ENFORCE_PRINTABLE_STRING:

          if (!((c >= 'a' && c <= 'z') || (c >= '\'' && c <= ':') || c == '=' ||
                c == '?')) {
            return false;
          }
          break;
        case ENFORCE_ASCII:
          if (c > 0x7F) {
            return false;
          }
          break;
        case NO_ENFORCEMENT:
          break;
      }
      *(write_iter++) = c;
    }
  }
  if (write_iter != output->end()) {
    output->erase(write_iter, output->end());
  }
  return true;
}

[[nodiscard]] bool NormalizeValue(X509NameAttribute attribute,
                                  std::string *output, CertErrors *errors) {
  BSSL_CHECK(errors);

  if (!attribute.ValueAsStringUnsafe(output)) {
    errors->AddError(kFailedConvertingAttributeValue,
                     CreateCertErrorParams1SizeT("tag", attribute.value_tag));
    return false;
  }

  bool success = false;
  switch (attribute.value_tag) {
    case CBS_ASN1_PRINTABLESTRING:
      success = NormalizeDirectoryString(ENFORCE_PRINTABLE_STRING, output);
      break;
    case CBS_ASN1_BMPSTRING:
    case CBS_ASN1_UNIVERSALSTRING:
    case CBS_ASN1_UTF8STRING:
      success = NormalizeDirectoryString(NO_ENFORCEMENT, output);
      break;
    case CBS_ASN1_IA5STRING:
      success = NormalizeDirectoryString(ENFORCE_ASCII, output);
      break;
    default:

      success = false;
      break;
  }

  if (!success) {
    errors->AddError(kFailedNormalizingString,
                     CreateCertErrorParams1SizeT("tag", attribute.value_tag));
  }

  return success;
}

bool IsNormalizableDirectoryString(CBS_ASN1_TAG tag) {
  switch (tag) {
    case CBS_ASN1_PRINTABLESTRING:
    case CBS_ASN1_UTF8STRING:

    case CBS_ASN1_IA5STRING:
    case CBS_ASN1_UNIVERSALSTRING:
    case CBS_ASN1_BMPSTRING:
      return true;

    default:
      return false;
  }
}

bool VerifyValueMatch(X509NameAttribute a, X509NameAttribute b) {
  if (IsNormalizableDirectoryString(a.value_tag) &&
      IsNormalizableDirectoryString(b.value_tag)) {
    std::string a_normalized, b_normalized;

    CertErrors unused_errors;
    if (!NormalizeValue(a, &a_normalized, &unused_errors) ||
        !NormalizeValue(b, &b_normalized, &unused_errors)) {
      return false;
    }
    return a_normalized == b_normalized;
  }

  if (a.value_tag != b.value_tag) {
    return false;
  }

  return a.value == b.value;
}

bool VerifyRdnMatch(der::Parser *a_parser, der::Parser *b_parser) {
  RelativeDistinguishedName a_type_and_values, b_type_and_values;
  if (!ReadRdn(a_parser, &a_type_and_values) ||
      !ReadRdn(b_parser, &b_type_and_values)) {
    return false;
  }

  if (a_type_and_values.size() != b_type_and_values.size()) {
    return false;
  }

  for (const auto &a : a_type_and_values) {
    auto b_iter = b_type_and_values.begin();
    for (; b_iter != b_type_and_values.end(); ++b_iter) {
      const auto &b = *b_iter;
      if (a.type == b.type && VerifyValueMatch(a, b)) {
        break;
      }
    }
    if (b_iter == b_type_and_values.end()) {
      return false;
    }

    b_type_and_values.erase(b_iter);
  }

  return true;
}

enum NameMatchType {
  EXACT_MATCH,
  SUBTREE_MATCH,
};

bool VerifyNameMatchInternal(der::Input a, der::Input b,
                             NameMatchType match_type) {

  der::Parser a_rdn_sequence_counter(a);
  der::Parser b_rdn_sequence_counter(b);
  while (a_rdn_sequence_counter.HasMore() && b_rdn_sequence_counter.HasMore()) {
    if (!a_rdn_sequence_counter.SkipTag(CBS_ASN1_SET) ||
        !b_rdn_sequence_counter.SkipTag(CBS_ASN1_SET)) {
      return false;
    }
  }

  if (b_rdn_sequence_counter.HasMore()) {
    return false;
  }
  if (match_type == EXACT_MATCH && a_rdn_sequence_counter.HasMore()) {
    return false;
  }

  der::Parser a_rdn_sequence(a);
  der::Parser b_rdn_sequence(b);
  while (a_rdn_sequence.HasMore() && b_rdn_sequence.HasMore()) {
    der::Parser a_rdn, b_rdn;
    if (!a_rdn_sequence.ReadConstructed(CBS_ASN1_SET, &a_rdn) ||
        !b_rdn_sequence.ReadConstructed(CBS_ASN1_SET, &b_rdn)) {
      return false;
    }
    if (!VerifyRdnMatch(&a_rdn, &b_rdn)) {
      return false;
    }
  }

  return true;
}

}

bool NormalizeName(der::Input name_rdn_sequence,
                   std::string *normalized_rdn_sequence, CertErrors *errors) {
  BSSL_CHECK(errors);

  der::Parser rdn_sequence_parser(name_rdn_sequence);

  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 0)) {
    return false;
  }

  while (rdn_sequence_parser.HasMore()) {

    der::Parser rdn_parser;
    if (!rdn_sequence_parser.ReadConstructed(CBS_ASN1_SET, &rdn_parser)) {
      return false;
    }
    RelativeDistinguishedName type_and_values;
    if (!ReadRdn(&rdn_parser, &type_and_values)) {
      return false;
    }

    CBB rdn_cbb;
    if (!CBB_add_asn1(cbb.get(), &rdn_cbb, CBS_ASN1_SET)) {
      return false;
    }

    for (const auto &type_and_value : type_and_values) {

      CBB attribute_type_and_value_cbb, type_cbb, value_cbb;
      if (!CBB_add_asn1(&rdn_cbb, &attribute_type_and_value_cbb,
                        CBS_ASN1_SEQUENCE)) {
        return false;
      }

      if (!CBB_add_asn1(&attribute_type_and_value_cbb, &type_cbb,
                        CBS_ASN1_OBJECT) ||
          !CBB_add_bytes(&type_cbb, type_and_value.type.data(),
                         type_and_value.type.size())) {
        return false;
      }

      if (IsNormalizableDirectoryString(type_and_value.value_tag)) {
        std::string normalized_value;
        if (!NormalizeValue(type_and_value, &normalized_value, errors)) {
          return false;
        }
        if (!CBB_add_asn1(&attribute_type_and_value_cbb, &value_cbb,
                          CBS_ASN1_UTF8STRING) ||
            !CBB_add_bytes(
                &value_cbb,
                reinterpret_cast<const uint8_t *>(normalized_value.data()),
                normalized_value.size())) {
          return false;
        }
      } else {
        if (!CBB_add_asn1(&attribute_type_and_value_cbb, &value_cbb,
                          type_and_value.value_tag) ||
            !CBB_add_bytes(&value_cbb, type_and_value.value.data(),
                           type_and_value.value.size())) {
          return false;
        }
      }

      if (!CBB_flush(&rdn_cbb)) {
        return false;
      }
    }

    if (!CBB_flush_asn1_set_of(&rdn_cbb) || !CBB_flush(cbb.get())) {
      return false;
    }
  }

  normalized_rdn_sequence->assign(CBB_data(cbb.get()),
                                  CBB_data(cbb.get()) + CBB_len(cbb.get()));
  return true;
}

bool VerifyNameMatch(der::Input a_rdn_sequence, der::Input b_rdn_sequence) {
  return VerifyNameMatchInternal(a_rdn_sequence, b_rdn_sequence, EXACT_MATCH);
}

bool VerifyNameInSubtree(der::Input name_rdn_sequence,
                         der::Input parent_rdn_sequence) {
  return VerifyNameMatchInternal(name_rdn_sequence, parent_rdn_sequence,
                                 SUBTREE_MATCH);
}

bool FindEmailAddressesInName(
    der::Input name_rdn_sequence,
    std::vector<std::string> *contained_email_addresses) {
  contained_email_addresses->clear();

  der::Parser rdn_sequence_parser(name_rdn_sequence);
  while (rdn_sequence_parser.HasMore()) {
    der::Parser rdn_parser;
    if (!rdn_sequence_parser.ReadConstructed(CBS_ASN1_SET, &rdn_parser)) {
      return false;
    }

    RelativeDistinguishedName type_and_values;
    if (!ReadRdn(&rdn_parser, &type_and_values)) {
      return false;
    }

    for (const auto &type_and_value : type_and_values) {
      if (type_and_value.type == der::Input(kTypeEmailAddressOid)) {
        std::string email_address;
        if (!type_and_value.ValueAsString(&email_address)) {
          return false;
        }
        contained_email_addresses->push_back(std::move(email_address));
      }
    }
  }

  return true;
}

BSSL_NAMESPACE_END
