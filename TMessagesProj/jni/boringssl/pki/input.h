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

#ifndef BSSL_DER_INPUT_H_
#define BSSL_DER_INPUT_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>

#include <openssl/base.h>
#include <openssl/span.h>

#if __has_include(<version>)
#include <version>
#endif

#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 201911L
#include <ranges>
BSSL_NAMESPACE_BEGIN
namespace der {
class OPENSSL_EXPORT Input;
}
BSSL_NAMESPACE_END

template <>
inline constexpr bool std::ranges::enable_view<bssl::der::Input> = true;
template <>
inline constexpr bool std::ranges::enable_borrowed_range<bssl::der::Input> =
    true;
#endif

BSSL_NAMESPACE_BEGIN
namespace der {

class OPENSSL_EXPORT Input {
 public:

  constexpr Input() = default;

  constexpr Input(bssl::Span<const uint8_t> data) : data_(data) {}

  constexpr explicit Input(const uint8_t *data, size_t len)
      : data_(Span(data, len)) {}

  explicit Input(std::string_view str) : data_(StringAsBytes(str)) {}

  constexpr Span<const uint8_t>::iterator begin() const {
    return data_.begin();
  }
  constexpr Span<const uint8_t>::iterator end() const { return data_.end(); }
  constexpr const uint8_t *data() const { return data_.data(); }
  constexpr size_t size() const { return data_.size(); }
  constexpr bool empty() const { return data_.empty(); }
  constexpr uint8_t operator[](size_t idx) const { return data_[idx]; }
  constexpr uint8_t front() const { return data_.front(); }
  constexpr uint8_t back() const { return data_.back(); }
  constexpr Input subspan(size_t pos = 0,
                          size_t len = Span<const uint8_t>::npos) const {
    return Input(data_.subspan(pos, len));
  }
  constexpr Input first(size_t len) const { return Input(data_.first(len)); }
  constexpr Input last(size_t len) const { return Input(data_.last(len)); }

  std::string AsString() const;

  std::string_view AsStringView() const { return BytesAsStringView(data_); }

  Span<const uint8_t> AsSpan() const { return *this; }

  constexpr size_t Length() const { return size(); }

  constexpr const uint8_t *UnsafeData() const { return data(); }

 private:

  Span<const uint8_t> data_;
};

OPENSSL_EXPORT bool operator==(Input lhs, Input rhs);

OPENSSL_EXPORT bool operator!=(Input lhs, Input rhs);

OPENSSL_EXPORT constexpr bool operator<(Input lhs, Input rhs) {

  auto *it1 = lhs.data();
  auto *it2 = rhs.data();
  const auto *end1 = lhs.data() + lhs.size();
  const auto *end2 = rhs.data() + rhs.size();
  for (; it1 != end1 && it2 != end2; ++it1, ++it2) {
    if (*it1 < *it2) {
      return true;
    } else if (*it2 < *it1) {
      return false;
    }
  }

  return it2 != end2;
}

class OPENSSL_EXPORT ByteReader {
 public:

  explicit ByteReader(Input in);

  [[nodiscard]] bool ReadByte(uint8_t *out);

  [[nodiscard]] bool ReadBytes(size_t len, Input *out);

  size_t BytesLeft() const { return data_.size(); }

  bool HasMore();

 private:
  void Advance(size_t len);

  bssl::Span<const uint8_t> data_;
};

}
BSSL_NAMESPACE_END

#endif
