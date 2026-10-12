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

#ifndef BSSL_PKI_NAME_CONSTRAINTS_H_
#define BSSL_PKI_NAME_CONSTRAINTS_H_

#include <memory>

#include <openssl/base.h>

#include "general_names.h"

BSSL_NAMESPACE_BEGIN

class CertErrors;

namespace der {
class Input;
}

class OPENSSL_EXPORT NameConstraints {
 public:
  ~NameConstraints();

  static std::unique_ptr<NameConstraints> Create(der::Input extension_value,
                                                 bool is_critical,
                                                 CertErrors *errors);

  static std::unique_ptr<NameConstraints> CreateFromPermittedSubtrees(
      GeneralNames permitted_subtrees);

  void IsPermittedCert(der::Input subject_rdn_sequence,
                       const GeneralNames *subject_alt_names,
                       CertErrors *errors) const;

  bool IsPermittedRfc822Name(std::string_view name,
                             bool case_insensitive_exclude_localpart) const;

  bool IsPermittedDNSName(std::string_view name) const;

  bool IsPermittedDirectoryName(der::Input name_rdn_sequence) const;

  bool IsPermittedIP(der::Input ip) const;

  int constrained_name_types() const { return constrained_name_types_; }

  const GeneralNames &permitted_subtrees() const { return permitted_subtrees_; }
  const GeneralNames &excluded_subtrees() const { return excluded_subtrees_; }

 private:
  [[nodiscard]] bool Parse(der::Input extension_value, bool is_critical,
                           CertErrors *errors);

  GeneralNames permitted_subtrees_;
  GeneralNames excluded_subtrees_;
  int constrained_name_types_ = GENERAL_NAME_NONE;
};

BSSL_NAMESPACE_END

#endif
