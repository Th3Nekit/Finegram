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

#ifndef BSSL_PKI_STRING_UTIL_H_
#define BSSL_PKI_STRING_UTIL_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <openssl/base.h>
#include <openssl/span.h>

BSSL_NAMESPACE_BEGIN
namespace string_util {

OPENSSL_EXPORT bool IsAscii(std::string_view str);

OPENSSL_EXPORT bool IsEqualNoCase(std::string_view str1, std::string_view str2);

OPENSSL_EXPORT bool StartsWithNoCase(std::string_view str,
                                     std::string_view prefix);

OPENSSL_EXPORT bool EndsWithNoCase(std::string_view str,
                                   std::string_view suffix);

OPENSSL_EXPORT std::string FindAndReplace(std::string_view str,
                                          std::string_view find,
                                          std::string_view replace);

OPENSSL_EXPORT bool StartsWith(std::string_view str, std::string_view prefix);

OPENSSL_EXPORT bool EndsWith(std::string_view str, std::string_view suffix);

OPENSSL_EXPORT std::string HexEncode(Span<const uint8_t> data);

OPENSSL_EXPORT std::string NumberToDecimalString(int i);

OPENSSL_EXPORT std::vector<std::string_view> SplitString(std::string_view str,
                                                         char split_char);

OPENSSL_EXPORT std::string CollapseWhitespaceASCII(
    std::string_view text, bool trim_sequences_with_line_breaks);

OPENSSL_EXPORT bool Base64Encode(const std::string_view &input,
                                 std::string *output);

OPENSSL_EXPORT bool Base64Decode(const std::string_view &input,
                                 std::string *output);

}
BSSL_NAMESPACE_END

#endif
