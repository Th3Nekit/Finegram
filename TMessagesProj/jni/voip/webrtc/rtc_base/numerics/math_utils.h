/*
 *  Copyright 2005 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_NUMERICS_MATH_UTILS_H_
#define API_NUMERICS_MATH_UTILS_H_

#include <limits>
#include <type_traits>

#include "rtc_base/checks.h"

namespace webrtc {
namespace webrtc_impl {

template <typename T>
typename std::make_unsigned<T>::type unsigned_difference(T x, T y) {
  static_assert(
      std::is_signed<T>::value,
      "Function unsigned_difference is only meaningful for signed types.");
  RTC_DCHECK_GE(x, y);
  typedef typename std::make_unsigned<T>::type unsigned_type;

  return static_cast<unsigned_type>(x) - static_cast<unsigned_type>(y);
}

template <typename T,
          typename std::enable_if<std::numeric_limits<T>::has_infinity>::type* =
              nullptr>
constexpr T infinity_or_max() {
  return std::numeric_limits<T>::infinity();
}

template <typename T,
          typename std::enable_if<
              !std::numeric_limits<T>::has_infinity>::type* = nullptr>
constexpr T infinity_or_max() {

  return std::numeric_limits<T>::max();
}

template <typename T,
          typename std::enable_if<std::numeric_limits<T>::has_infinity>::type* =
              nullptr>
constexpr T minus_infinity_or_min() {
  static_assert(std::is_signed<T>::value, "Unsupported. Please open a bug.");
  return -std::numeric_limits<T>::infinity();
}

template <typename T,
          typename std::enable_if<
              !std::numeric_limits<T>::has_infinity>::type* = nullptr>
constexpr T minus_infinity_or_min() {

  return std::numeric_limits<T>::min();
}

}
}

#endif
