// Copyright 2018 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_TEST_WYCHEPROOF_UTIL_H
#define OPENSSL_HEADER_CRYPTO_TEST_WYCHEPROOF_UTIL_H

#include <openssl/base.h>

#include <string>
#include <vector>

class FileTest;

enum class WycheproofRawResult {
  kValid,
  kInvalid,
  kAcceptable,
};

struct WycheproofResult {
  WycheproofRawResult raw_result;
  std::vector<std::string> flags;

  bool IsValid(const std::vector<std::string> &acceptable_flags = {}) const;
};

bool GetWycheproofResult(FileTest *t, WycheproofResult *out);

const EVP_MD *GetWycheproofDigest(FileTest *t, const char *key,
                                  bool instruction);

const EC_GROUP *GetWycheproofCurve(FileTest *t, const char *key,
                                   bool instruction);

bssl::UniquePtr<BIGNUM> GetWycheproofBIGNUM(FileTest *t, const char *key,
                                            bool instruction);

#endif
