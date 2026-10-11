// Copyright 2015 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_TEST_TEST_UTIL_H
#define OPENSSL_HEADER_CRYPTO_TEST_TEST_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <iosfwd>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/span.h>

#include "../internal.h"

void hexdump(FILE *fp, const char *msg, const void *in, size_t len);

struct Bytes {
  Bytes(const uint8_t *data_arg, size_t len_arg)
      : span_(data_arg, len_arg) {}
  Bytes(const char *data_arg, size_t len_arg)
      : span_(reinterpret_cast<const uint8_t *>(data_arg), len_arg) {}

  explicit Bytes(std::string_view str) : span_(bssl::StringAsBytes(str)) {}
  explicit Bytes(bssl::Span<const uint8_t> span) : span_(span) {}

  bssl::Span<const uint8_t> span_;
};

inline bool operator==(const Bytes &a, const Bytes &b) {
  return a.span_ == b.span_;
}

inline bool operator!=(const Bytes &a, const Bytes &b) { return !(a == b); }

inline std::vector<uint8_t> Declassified(bssl::Span<const uint8_t> in) {
  std::vector<uint8_t> copy(in.begin(), in.end());
  CONSTTIME_DECLASSIFY(copy.data(), copy.size());
  return copy;
}

std::ostream &operator<<(std::ostream &os, const Bytes &in);

bool DecodeHex(std::vector<uint8_t> *out, const std::string &in);

std::string EncodeHex(bssl::Span<const uint8_t> in);

testing::AssertionResult ErrorEquals(uint32_t err, int lib, int reason);

#endif
