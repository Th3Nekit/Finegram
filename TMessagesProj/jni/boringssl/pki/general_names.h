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

#ifndef BSSL_PKI_GENERAL_NAMES_H_
#define BSSL_PKI_GENERAL_NAMES_H_

#include <memory>
#include <string_view>
#include <vector>

#include <openssl/base.h>

#include "cert_error_id.h"

BSSL_NAMESPACE_BEGIN

class CertErrors;

OPENSSL_EXPORT extern const CertErrorId kFailedParsingGeneralName;

namespace der {
class Input;
}

enum GeneralNameTypes {
  GENERAL_NAME_NONE = 0,
  GENERAL_NAME_OTHER_NAME = 1 << 0,
  GENERAL_NAME_RFC822_NAME = 1 << 1,
  GENERAL_NAME_DNS_NAME = 1 << 2,
  GENERAL_NAME_X400_ADDRESS = 1 << 3,
  GENERAL_NAME_DIRECTORY_NAME = 1 << 4,
  GENERAL_NAME_EDI_PARTY_NAME = 1 << 5,
  GENERAL_NAME_UNIFORM_RESOURCE_IDENTIFIER = 1 << 6,
  GENERAL_NAME_IP_ADDRESS = 1 << 7,
  GENERAL_NAME_REGISTERED_ID = 1 << 8,
  GENERAL_NAME_ALL_TYPES = (1 << 9) - 1,
};

struct OPENSSL_EXPORT GeneralNames {

  enum ParseGeneralNameIPAddressType {
    IP_ADDRESS_ONLY,
    IP_ADDRESS_AND_NETMASK,
  };

  GeneralNames();
  ~GeneralNames();

  static std::unique_ptr<GeneralNames> Create(der::Input general_names_tlv,
                                              CertErrors *errors);

  static std::unique_ptr<GeneralNames> CreateFromValue(
      der::Input general_names_value, CertErrors *errors);

  std::vector<der::Input> other_names;

  std::vector<std::string_view> rfc822_names;

  std::vector<std::string_view> dns_names;

  std::vector<der::Input> x400_addresses;

  std::vector<der::Input> directory_names;

  std::vector<der::Input> edi_party_names;

  std::vector<std::string_view> uniform_resource_identifiers;

  std::vector<der::Input> ip_addresses;

  std::vector<std::pair<der::Input, der::Input>> ip_address_ranges;

  std::vector<der::Input> registered_ids;

  int present_name_types = GENERAL_NAME_NONE;
};

[[nodiscard]] OPENSSL_EXPORT bool ParseGeneralName(
    der::Input input,
    GeneralNames::ParseGeneralNameIPAddressType ip_address_type,
    GeneralNames *subtrees, CertErrors *errors);

BSSL_NAMESPACE_END

#endif
