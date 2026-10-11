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

#ifndef BSSL_PKI_PARSE_NAME_H_
#define BSSL_PKI_PARSE_NAME_H_

#include <vector>

#include <openssl/base.h>
#include <openssl/bytestring.h>

#include "input.h"
#include "parser.h"

BSSL_NAMESPACE_BEGIN

inline constexpr uint8_t kTypeCommonNameOid[] = {0x55, 0x04, 0x03};

inline constexpr uint8_t kTypeSurnameOid[] = {0x55, 0x04, 0x04};

inline constexpr uint8_t kTypeSerialNumberOid[] = {0x55, 0x04, 0x05};

inline constexpr uint8_t kTypeCountryNameOid[] = {0x55, 0x04, 0x06};

inline constexpr uint8_t kTypeLocalityNameOid[] = {0x55, 0x04, 0x07};

inline constexpr uint8_t kTypeStateOrProvinceNameOid[] = {0x55, 0x04, 0x08};

inline constexpr uint8_t kTypeStreetAddressOid[] = {0x55, 0x04, 0x09};

inline constexpr uint8_t kTypeOrganizationNameOid[] = {0x55, 0x04, 0x0a};

inline constexpr uint8_t kTypeOrganizationUnitNameOid[] = {0x55, 0x04, 0x0b};

inline constexpr uint8_t kTypeTitleOid[] = {0x55, 0x04, 0x0c};

inline constexpr uint8_t kTypeNameOid[] = {0x55, 0x04, 0x29};

inline constexpr uint8_t kTypeGivenNameOid[] = {0x55, 0x04, 0x2a};

inline constexpr uint8_t kTypeInitialsOid[] = {0x55, 0x04, 0x2b};

inline constexpr uint8_t kTypeGenerationQualifierOid[] = {0x55, 0x04, 0x2c};

inline constexpr uint8_t kTypeDomainComponentOid[] = {
    0x09, 0x92, 0x26, 0x89, 0x93, 0xF2, 0x2C, 0x64, 0x01, 0x19};

inline constexpr uint8_t kTypeEmailAddressOid[] = {0x2A, 0x86, 0x48, 0x86, 0xF7,
                                                   0x0D, 0x01, 0x09, 0x01};

struct OPENSSL_EXPORT X509NameAttribute {
  X509NameAttribute(der::Input in_type, CBS_ASN1_TAG in_value_tag,
                    der::Input in_value)
      : type(in_type), value_tag(in_value_tag), value(in_value) {}

  enum class PrintableStringHandling { kDefault, kAsUTF8Hack };

  [[nodiscard]] bool ValueAsString(std::string *out) const;

  [[nodiscard]] bool ValueAsStringWithUnsafeOptions(
      PrintableStringHandling printable_string_handling,
      std::string *out) const;

  [[nodiscard]] bool ValueAsStringUnsafe(std::string *out) const;

  [[nodiscard]] bool AsRFC2253String(std::string *out) const;

  der::Input type;
  CBS_ASN1_TAG value_tag;
  der::Input value;
};

typedef std::vector<X509NameAttribute> RelativeDistinguishedName;
typedef std::vector<RelativeDistinguishedName> RDNSequence;

[[nodiscard]] OPENSSL_EXPORT bool ReadRdn(der::Parser *parser,
                                          RelativeDistinguishedName *out);

[[nodiscard]] OPENSSL_EXPORT bool ParseName(der::Input name_tlv,
                                            RDNSequence *out);

[[nodiscard]] OPENSSL_EXPORT bool ParseNameValue(der::Input name_value,
                                                 RDNSequence *out);

[[nodiscard]] OPENSSL_EXPORT bool ConvertToRFC2253(
    const RDNSequence &rdn_sequence, std::string *out);
BSSL_NAMESPACE_END

#endif
