/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STRING_ENCODE_H_
#define RTC_BASE_STRING_ENCODE_H_

#include <stddef.h>

#include <string>
#include <type_traits>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/string_to_number.h"

namespace rtc {

std::string hex_encode(absl::string_view str);
std::string hex_encode_with_delimiter(absl::string_view source, char delimiter);

size_t hex_decode(ArrayView<char> buffer, absl::string_view source);

size_t hex_decode_with_delimiter(ArrayView<char> buffer,
                                 absl::string_view source,
                                 char delimiter);

std::vector<absl::string_view> split(absl::string_view source, char delimiter);

size_t tokenize(absl::string_view source,
                char delimiter,
                std::vector<std::string>* fields);

bool tokenize_first(absl::string_view source,
                    char delimiter,
                    std::string* token,
                    std::string* rest);

std::string ToString(bool b);

std::string ToString(absl::string_view s);

std::string ToString(const char* s);

std::string ToString(short s);
std::string ToString(unsigned short s);
std::string ToString(int s);
std::string ToString(unsigned int s);
std::string ToString(long int s);
std::string ToString(unsigned long int s);
std::string ToString(long long int s);
std::string ToString(unsigned long long int s);

std::string ToString(double t);
std::string ToString(long double t);

std::string ToString(const void* p);

template <typename T,
          typename std::enable_if<std::is_arithmetic<T>::value &&
                                      !std::is_same<T, bool>::value,
                                  int>::type = 0>
static bool FromString(absl::string_view s, T* t) {
  RTC_DCHECK(t);
  absl::optional<T> result = StringToNumber<T>(s);

  if (result)
    *t = *result;

  return result.has_value();
}

bool FromString(absl::string_view s, bool* b);

template <typename T>
static inline T FromString(absl::string_view str) {
  T val;
  FromString(str, &val);
  return val;
}

}

#endif
