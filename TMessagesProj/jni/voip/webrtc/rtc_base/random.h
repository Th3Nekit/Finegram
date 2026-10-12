/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_RANDOM_H_
#define RTC_BASE_RANDOM_H_

#include <stdint.h>

#include <limits>

#include "rtc_base/checks.h"

namespace webrtc {

class Random {
 public:

  explicit Random(uint64_t seed);

  Random() = delete;
  Random(const Random&) = delete;
  Random& operator=(const Random&) = delete;

  template <typename T>
  T Rand() {
    static_assert(std::numeric_limits<T>::is_integer &&
                      std::numeric_limits<T>::radix == 2 &&
                      std::numeric_limits<T>::digits <= 32,
                  "Rand is only supported for built-in integer types that are "
                  "32 bits or smaller.");
    return static_cast<T>(NextOutput());
  }

  uint32_t Rand(uint32_t t);

  uint32_t Rand(uint32_t low, uint32_t high);

  int32_t Rand(int32_t low, int32_t high);

  double Gaussian(double mean, double standard_deviation);

  double Exponential(double lambda);

 private:

  uint64_t NextOutput() {
    state_ ^= state_ >> 12;
    state_ ^= state_ << 25;
    state_ ^= state_ >> 27;
    RTC_DCHECK(state_ != 0x0ULL);
    return state_ * 2685821657736338717ull;
  }

  uint64_t state_;
};

template <>
float Random::Rand<float>();

template <>
double Random::Rand<double>();

template <>
bool Random::Rand<bool>();

}

#endif
