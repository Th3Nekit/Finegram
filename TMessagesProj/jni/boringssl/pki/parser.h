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

#ifndef BSSL_DER_PARSER_H_
#define BSSL_DER_PARSER_H_

#include <stdint.h>

#include <optional>

#include <openssl/base.h>
#include <openssl/bytestring.h>

#include "input.h"

BSSL_NAMESPACE_BEGIN
namespace der {

class BitString;
struct GeneralizedTime;

class OPENSSL_EXPORT Parser {
 public:

  Parser();

  explicit Parser(Input input);

  Parser(const Parser &) = default;
  Parser &operator=(const Parser &) = default;

  bool HasMore();

  [[nodiscard]] bool ReadTagAndValue(CBS_ASN1_TAG *tag, Input *out);

  [[nodiscard]] bool ReadRawTLV(Input *out);

  [[nodiscard]] bool ReadOptionalTag(CBS_ASN1_TAG tag, std::optional<Input> *out);

  [[nodiscard]] bool ReadOptionalTag(CBS_ASN1_TAG tag, Input *out, bool *was_present);

  [[nodiscard]] bool SkipOptionalTag(CBS_ASN1_TAG tag, bool *was_present);

  [[nodiscard]] bool ReadTag(CBS_ASN1_TAG tag, Input *out);

  [[nodiscard]] bool SkipTag(CBS_ASN1_TAG tag);

  [[nodiscard]] bool ReadConstructed(CBS_ASN1_TAG tag, Parser *out);

  [[nodiscard]] bool ReadSequence(Parser *out);

  [[nodiscard]] bool ReadUint8(uint8_t *out);

  [[nodiscard]] bool ReadUint64(uint64_t *out);

  [[nodiscard]] std::optional<BitString> ReadBitString();

  [[nodiscard]] bool ReadGeneralizedTime(GeneralizedTime *out);

  [[nodiscard]] bool PeekTagAndValue(CBS_ASN1_TAG *tag, Input *out);

  bool Advance();

 private:
  CBS cbs_;
  size_t advance_len_ = 0;
};

}
BSSL_NAMESPACE_END

#endif
