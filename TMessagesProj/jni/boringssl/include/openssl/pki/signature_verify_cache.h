// Copyright 2022 The Chromium Authors
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

#if !defined(BSSL_PKI_SIGNATURE_VERIFY_CACHE_H_) && defined(__cplusplus)
#define BSSL_PKI_SIGNATURE_VERIFY_CACHE_H_

#include <openssl/base.h>
#include <string>

BSSL_NAMESPACE_BEGIN

class OPENSSL_EXPORT SignatureVerifyCache {
 public:
  enum class Value {
    kValid,
    kInvalid,
    kUnknown,
  };

  virtual ~SignatureVerifyCache() = default;

  virtual void Store(const std::string &key, Value value) = 0;

  virtual Value Check(const std::string &key) = 0;
};

BSSL_NAMESPACE_END

#endif
