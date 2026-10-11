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

#ifndef BSSL_PKI_VERIFY_NAME_MATCH_H_
#define BSSL_PKI_VERIFY_NAME_MATCH_H_

#include <string>
#include <vector>

#include <openssl/base.h>

BSSL_NAMESPACE_BEGIN

class CertErrors;

namespace der {
class Input;
}

OPENSSL_EXPORT bool NormalizeName(der::Input name_rdn_sequence,
                                  std::string *normalized_rdn_sequence,
                                  CertErrors *errors);

OPENSSL_EXPORT bool VerifyNameMatch(der::Input a_rdn_sequence,
                                    der::Input b_rdn_sequence);

OPENSSL_EXPORT bool VerifyNameInSubtree(der::Input name_rdn_sequence,
                                        der::Input parent_rdn_sequence);

[[nodiscard]] bool FindEmailAddressesInName(
    der::Input name_rdn_sequence,
    std::vector<std::string> *contained_email_addresses);

BSSL_NAMESPACE_END

#endif
