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

#ifndef BSSL_PKI_CERT_ERROR_PARAMS_H_
#define BSSL_PKI_CERT_ERROR_PARAMS_H_

#include <memory>
#include <string>

#include <openssl/base.h>

BSSL_NAMESPACE_BEGIN

namespace der {
class Input;
}

class OPENSSL_EXPORT CertErrorParams {
 public:
  CertErrorParams();

  CertErrorParams(const CertErrorParams &) = delete;
  CertErrorParams &operator=(const CertErrorParams &) = delete;

  virtual ~CertErrorParams();

  virtual std::string ToDebugString() const = 0;
};

OPENSSL_EXPORT std::unique_ptr<CertErrorParams> CreateCertErrorParams1Der(
    const char *name, der::Input der);

OPENSSL_EXPORT std::unique_ptr<CertErrorParams> CreateCertErrorParams2Der(
    const char *name1, der::Input der1, const char *name2, der::Input der2);

OPENSSL_EXPORT std::unique_ptr<CertErrorParams> CreateCertErrorParams1SizeT(
    const char *name, size_t value);

OPENSSL_EXPORT std::unique_ptr<CertErrorParams> CreateCertErrorParams2SizeT(
    const char *name1, size_t value1, const char *name2, size_t value2);

BSSL_NAMESPACE_END

#endif
