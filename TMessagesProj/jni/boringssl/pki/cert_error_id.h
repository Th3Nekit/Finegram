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

#ifndef BSSL_PKI_CERT_ERROR_ID_H_
#define BSSL_PKI_CERT_ERROR_ID_H_

#include <openssl/base.h>

BSSL_NAMESPACE_BEGIN

using CertErrorId = const void *;

#define DEFINE_CERT_ERROR_ID(name, c_str_literal) \
  const CertErrorId name = c_str_literal

OPENSSL_EXPORT const char *CertErrorIdToDebugString(CertErrorId id);

BSSL_NAMESPACE_END

#endif
