/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STRING_TO_NUMBER_H_
#define RTC_BASE_STRING_TO_NUMBER_H_

#include <limits>
#include <string>
#include <type_traits>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

namespace rtc {

namespace string_to_number_internal {

using unsigned_type = unsigned long long;
using signed_type = long long;

absl::optional<signed_type> ParseSigned(absl::string_view str, int base);
absl::optional<unsigned_type> ParseUnsigned(absl::string_view str, int base);

template <typename T>
absl::optional<T> ParseFloatingPoint(absl::string_view str);
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value,
                        absl::optional<T>>::type
StringToNumber(absl::string_view str, int base = 10) {
  using string_to_number_internal::signed_type;
  static_assert(
      std::numeric_limits<T>::max() <=
              std::numeric_limits<signed_type>::max() &&
          std::numeric_limits<T>::lowest() >=
              std::numeric_limits<signed_type>::lowest(),
      "StringToNumber only supports signed integers as large as long long int");
  absl::optional<signed_type> value =
      string_to_number_internal::ParseSigned(str, base);
  if (value && *value >= std::numeric_limits<T>::lowest() &&
      *value <= std::numeric_limits<T>::max()) {
    return static_cast<T>(*value);
  }
  return absl::nullopt;
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value &&
                            std::is_unsigned<T>::value,
                        absl::optional<T>>::type
StringToNumber(absl::string_view str, int base = 10) {
  using string_to_number_internal::unsigned_type;
  static_assert(std::numeric_limits<T>::max() <=
                    std::numeric_limits<unsigned_type>::max(),
                "StringToNumber only supports unsigned integers as large as "
                "unsigned long long int");
  absl::optional<unsigned_type> value =
      string_to_number_internal::ParseUnsigned(str, base);
  if (value && *value <= std::numeric_limits<T>::max()) {
    return static_cast<T>(*value);
  }
  return absl::nullopt;
}

template <typename T>
typename std::enable_if<std::is_floating_point<T>::value,
                        absl::optional<T>>::type
StringToNumber(absl::string_view str, int base = 10) {
  static_assert(
      std::numeric_limits<T>::max() <= std::numeric_limits<long double>::max(),
      "StringToNumber only supports floating-point numbers as large "
      "as long double");
  return string_to_number_internal::ParseFloatingPoint<T>(str);
}

}

#endif
