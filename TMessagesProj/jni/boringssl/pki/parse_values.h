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

#ifndef BSSL_DER_PARSE_VALUES_H_
#define BSSL_DER_PARSE_VALUES_H_

#include <stdint.h>

#include <optional>

#include <openssl/base.h>

#include "input.h"

BSSL_NAMESPACE_BEGIN
namespace der {

[[nodiscard]] OPENSSL_EXPORT bool ParseBool(Input in, bool *out);

[[nodiscard]] OPENSSL_EXPORT bool ParseBoolRelaxed(Input in, bool *out);

[[nodiscard]] OPENSSL_EXPORT bool IsValidInteger(Input in, bool *negative);

[[nodiscard]] OPENSSL_EXPORT bool ParseUint64(Input in, uint64_t *out);

[[nodiscard]] OPENSSL_EXPORT bool ParseUint8(Input in, uint8_t *out);

class OPENSSL_EXPORT BitString {
 public:
  BitString() = default;

  BitString(Input bytes, uint8_t unused_bits);

  Input bytes() const { return bytes_; }
  uint8_t unused_bits() const { return unused_bits_; }

  [[nodiscard]] bool AssertsBit(size_t bit_index) const;

 private:
  Input bytes_;
  uint8_t unused_bits_ = 0;

};

[[nodiscard]] OPENSSL_EXPORT std::optional<BitString> ParseBitString(Input in);

struct OPENSSL_EXPORT GeneralizedTime {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;

  bool InUTCTimeRange() const;
};

OPENSSL_EXPORT bool operator<(const GeneralizedTime &lhs,
                              const GeneralizedTime &rhs);
OPENSSL_EXPORT bool operator<=(const GeneralizedTime &lhs,
                               const GeneralizedTime &rhs);
OPENSSL_EXPORT bool operator>(const GeneralizedTime &lhs,
                              const GeneralizedTime &rhs);
OPENSSL_EXPORT bool operator>=(const GeneralizedTime &lhs,
                               const GeneralizedTime &rhs);

[[nodiscard]] OPENSSL_EXPORT bool ParseUTCTime(Input in, GeneralizedTime *out);

[[nodiscard]] OPENSSL_EXPORT bool ParseGeneralizedTime(Input in,
                                                       GeneralizedTime *out);

[[nodiscard]] OPENSSL_EXPORT bool ParseIA5String(Input in, std::string *out);

[[nodiscard]] OPENSSL_EXPORT bool ParseVisibleString(Input in,
                                                     std::string *out);

[[nodiscard]] OPENSSL_EXPORT bool ParsePrintableString(Input in,
                                                       std::string *out);

[[nodiscard]] OPENSSL_EXPORT bool ParseTeletexStringAsLatin1(Input in,
                                                             std::string *out);

[[nodiscard]] OPENSSL_EXPORT bool ParseUniversalString(Input in,
                                                       std::string *out);

[[nodiscard]] OPENSSL_EXPORT bool ParseBmpString(Input in, std::string *out);

}
BSSL_NAMESPACE_END

#endif
