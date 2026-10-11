/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/random.h"

#include <math.h>

#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

Random::Random(uint64_t seed) {
  RTC_DCHECK(seed != 0x0ull);
  state_ = seed;
}

uint32_t Random::Rand(uint32_t t) {

  uint32_t x = NextOutput();

  uint64_t result = x * (static_cast<uint64_t>(t) + 1);
  result >>= 32;
  return result;
}

uint32_t Random::Rand(uint32_t low, uint32_t high) {
  RTC_DCHECK(low <= high);
  return Rand(high - low) + low;
}

int32_t Random::Rand(int32_t low, int32_t high) {
  RTC_DCHECK(low <= high);
  const int64_t low_i64{low};
  return rtc::dchecked_cast<int32_t>(
      Rand(rtc::dchecked_cast<uint32_t>(high - low_i64)) + low_i64);
}

template <>
float Random::Rand<float>() {
  double result = NextOutput() - 1;
  result = result / static_cast<double>(0xFFFFFFFFFFFFFFFFull);
  return static_cast<float>(result);
}

template <>
double Random::Rand<double>() {
  double result = NextOutput() - 1;
  result = result / static_cast<double>(0xFFFFFFFFFFFFFFFFull);
  return result;
}

template <>
bool Random::Rand<bool>() {
  return Rand(0, 1) == 1;
}

double Random::Gaussian(double mean, double standard_deviation) {

  const double kPi = 3.14159265358979323846;
  double u1 = static_cast<double>(NextOutput()) /
              static_cast<double>(0xFFFFFFFFFFFFFFFFull);
  double u2 = static_cast<double>(NextOutput()) /
              static_cast<double>(0xFFFFFFFFFFFFFFFFull);
  return mean + standard_deviation * sqrt(-2 * log(u1)) * cos(2 * kPi * u2);
}

double Random::Exponential(double lambda) {
  double uniform = Rand<double>();
  return -log(uniform) / lambda;
}

}
