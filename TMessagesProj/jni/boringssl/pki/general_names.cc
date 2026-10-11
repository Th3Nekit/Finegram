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

#include "general_names.h"

#include <openssl/base.h>
#include <openssl/bytestring.h>

#include <climits>
#include <cstring>

#include "cert_error_params.h"
#include "cert_errors.h"
#include "input.h"
#include "ip_util.h"
#include "parser.h"
#include "string_util.h"

BSSL_NAMESPACE_BEGIN

DEFINE_CERT_ERROR_ID(kFailedParsingGeneralName, "Failed parsing GeneralName");

namespace {

DEFINE_CERT_ERROR_ID(kRFC822NameNotAscii, "rfc822Name is not ASCII");
DEFINE_CERT_ERROR_ID(kDnsNameNotAscii, "dNSName is not ASCII");
DEFINE_CERT_ERROR_ID(kURINotAscii, "uniformResourceIdentifier is not ASCII");
DEFINE_CERT_ERROR_ID(kFailedParsingIp, "Failed parsing iPAddress");
DEFINE_CERT_ERROR_ID(kUnknownGeneralNameType, "Unknown GeneralName type");
DEFINE_CERT_ERROR_ID(kFailedReadingGeneralNames,
                     "Failed reading GeneralNames SEQUENCE");
DEFINE_CERT_ERROR_ID(kGeneralNamesTrailingData,
                     "GeneralNames contains trailing data after the sequence");
DEFINE_CERT_ERROR_ID(kGeneralNamesEmpty,
                     "GeneralNames is a sequence of 0 elements");
DEFINE_CERT_ERROR_ID(kFailedReadingGeneralName,
                     "Failed reading GeneralName TLV");

}

GeneralNames::GeneralNames() = default;

GeneralNames::~GeneralNames() = default;

std::unique_ptr<GeneralNames> GeneralNames::Create(der::Input general_names_tlv,
                                                   CertErrors *errors) {
  BSSL_CHECK(errors);

  der::Parser parser(general_names_tlv);
  der::Input sequence_value;
  if (!parser.ReadTag(CBS_ASN1_SEQUENCE, &sequence_value)) {
    errors->AddError(kFailedReadingGeneralNames);
    return nullptr;
  }

  if (parser.HasMore()) {
    errors->AddError(kGeneralNamesTrailingData);
    return nullptr;
  }
  return CreateFromValue(sequence_value, errors);
}

std::unique_ptr<GeneralNames> GeneralNames::CreateFromValue(
    der::Input general_names_value, CertErrors *errors) {
  BSSL_CHECK(errors);

  auto general_names = std::make_unique<GeneralNames>();

  der::Parser sequence_parser(general_names_value);

  if (!sequence_parser.HasMore()) {
    errors->AddError(kGeneralNamesEmpty);
    return nullptr;
  }

  while (sequence_parser.HasMore()) {
    der::Input raw_general_name;
    if (!sequence_parser.ReadRawTLV(&raw_general_name)) {
      errors->AddError(kFailedReadingGeneralName);
      return nullptr;
    }

    if (!ParseGeneralName(raw_general_name, IP_ADDRESS_ONLY,
                          general_names.get(), errors)) {
      errors->AddError(kFailedParsingGeneralName);
      return nullptr;
    }
  }

  return general_names;
}

[[nodiscard]] bool ParseGeneralName(
    der::Input input,
    GeneralNames::ParseGeneralNameIPAddressType ip_address_type,
    GeneralNames *subtrees, CertErrors *errors) {
  BSSL_CHECK(errors);
  der::Parser parser(input);
  CBS_ASN1_TAG tag;
  der::Input value;
  if (!parser.ReadTagAndValue(&tag, &value)) {
    return false;
  }
  GeneralNameTypes name_type = GENERAL_NAME_NONE;
  if (tag == (CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0)) {

    name_type = GENERAL_NAME_OTHER_NAME;
    subtrees->other_names.push_back(value);
  } else if (tag == (CBS_ASN1_CONTEXT_SPECIFIC | 1)) {

    name_type = GENERAL_NAME_RFC822_NAME;
    const std::string_view s = BytesAsStringView(value);
    if (!bssl::string_util::IsAscii(s)) {
      errors->AddError(kRFC822NameNotAscii);
      return false;
    }
    subtrees->rfc822_names.push_back(s);
  } else if (tag == (CBS_ASN1_CONTEXT_SPECIFIC | 2)) {

    name_type = GENERAL_NAME_DNS_NAME;
    const std::string_view s = BytesAsStringView(value);
    if (!bssl::string_util::IsAscii(s)) {
      errors->AddError(kDnsNameNotAscii);
      return false;
    }
    subtrees->dns_names.push_back(s);
  } else if (tag == (CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 3)) {

    name_type = GENERAL_NAME_X400_ADDRESS;
    subtrees->x400_addresses.push_back(value);
  } else if (tag == (CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 4)) {

    name_type = GENERAL_NAME_DIRECTORY_NAME;

    der::Parser name_parser(value);
    der::Input name_value;
    if (!name_parser.ReadTag(CBS_ASN1_SEQUENCE, &name_value) ||
        parser.HasMore()) {
      return false;
    }
    subtrees->directory_names.push_back(name_value);
  } else if (tag == (CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 5)) {

    name_type = GENERAL_NAME_EDI_PARTY_NAME;
    subtrees->edi_party_names.push_back(value);
  } else if (tag == (CBS_ASN1_CONTEXT_SPECIFIC | 6)) {

    name_type = GENERAL_NAME_UNIFORM_RESOURCE_IDENTIFIER;
    const std::string_view s = BytesAsStringView(value);
    if (!bssl::string_util::IsAscii(s)) {
      errors->AddError(kURINotAscii);
      return false;
    }
    subtrees->uniform_resource_identifiers.push_back(s);
  } else if (tag == (CBS_ASN1_CONTEXT_SPECIFIC | 7)) {

    name_type = GENERAL_NAME_IP_ADDRESS;
    if (ip_address_type == GeneralNames::IP_ADDRESS_ONLY) {

      if ((value.size() != kIPv4AddressSize &&
           value.size() != kIPv6AddressSize)) {
        errors->AddError(kFailedParsingIp);
        return false;
      }
      subtrees->ip_addresses.push_back(value);
    } else {
      BSSL_CHECK(ip_address_type == GeneralNames::IP_ADDRESS_AND_NETMASK);

      if (value.size() != kIPv4AddressSize * 2 &&
          value.size() != kIPv6AddressSize * 2) {
        errors->AddError(kFailedParsingIp);
        return false;
      }
      der::Input addr = value.first(value.size() / 2);
      der::Input mask = value.subspan(value.size() / 2);
      if (!IsValidNetmask(mask)) {
        errors->AddError(kFailedParsingIp);
        return false;
      }
      subtrees->ip_address_ranges.emplace_back(addr, mask);
    }
  } else if (tag == (CBS_ASN1_CONTEXT_SPECIFIC | 8)) {

    name_type = GENERAL_NAME_REGISTERED_ID;
    subtrees->registered_ids.push_back(value);
  } else {
    errors->AddError(kUnknownGeneralNameType,
                     CreateCertErrorParams1SizeT("tag", tag));
    return false;
  }
  BSSL_CHECK(GENERAL_NAME_NONE != name_type);
  subtrees->present_name_types |= name_type;
  return true;
}

BSSL_NAMESPACE_END
