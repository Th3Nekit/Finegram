/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_MOD_OPS_H_
#define RTC_BASE_NUMERICS_MOD_OPS_H_

#include <algorithm>
#include <type_traits>

#include "rtc_base/checks.h"

namespace webrtc {

template <unsigned long M>
inline unsigned long Add(unsigned long a, unsigned long b) {
  RTC_DCHECK_LT(a, M);
  unsigned long t = M - b % M;
  unsigned long res = a - t;
  if (t > a)
    return res + M;
  return res;
}

template <unsigned long M>
inline unsigned long Subtract(unsigned long a, unsigned long b) {
  RTC_DCHECK_LT(a, M);
  unsigned long sub = b % M;
  if (a < sub)
    return M - (sub - a);
  return a - sub;
}

template <typename T, T M>
inline typename std::enable_if<(M > 0), T>::type ForwardDiff(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  RTC_DCHECK_LT(a, M);
  RTC_DCHECK_LT(b, M);
  return a <= b ? b - a : M - (a - b);
}

template <typename T, T M>
inline typename std::enable_if<(M == 0), T>::type ForwardDiff(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  return b - a;
}

template <typename T>
inline T ForwardDiff(T a, T b) {
  return ForwardDiff<T, 0>(a, b);
}

template <typename T, T M>
inline typename std::enable_if<(M > 0), T>::type ReverseDiff(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  RTC_DCHECK_LT(a, M);
  RTC_DCHECK_LT(b, M);
  return b <= a ? a - b : M - (b - a);
}

template <typename T, T M>
inline typename std::enable_if<(M == 0), T>::type ReverseDiff(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  return a - b;
}

template <typename T>
inline T ReverseDiff(T a, T b) {
  return ReverseDiff<T, 0>(a, b);
}

template <typename T, T M = 0>
inline T MinDiff(T a, T b) {
  static_assert(std::is_unsigned<T>::value,
                "Type must be an unsigned integer.");
  return std::min(ForwardDiff<T, M>(a, b), ReverseDiff<T, M>(a, b));
}

}

#endif
