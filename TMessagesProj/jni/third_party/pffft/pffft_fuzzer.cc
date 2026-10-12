// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>

#include "third_party/pffft/src/pffft.h"

namespace {

#if defined(TRANSFORM_REAL)

constexpr pffft_transform_t kTransform = PFFFT_REAL;
constexpr size_t kSizeOfOneSample = sizeof(float);
#elif defined(TRANSFORM_COMPLEX)

constexpr pffft_transform_t kTransform = PFFFT_COMPLEX;
constexpr size_t kSizeOfOneSample = 2 * sizeof(float);
#else
#error FFT transform type not defined.
#endif

bool IsValidSize(size_t n) {
  if (n == 0) {
    return false;
  }

  constexpr std::array<int, 3> kFactors = {2, 3, 5};
  std::array<int, kFactors.size()> factorization{};
  for (size_t i = 0; i < kFactors.size(); ++i) {
    const int factor = kFactors[i];
    while (n % factor == 0) {
      n /= factor;
      factorization[i]++;
    }
  }
  return factorization[0] >= 5 && n == 1;
}

float* AllocatePffftBuffer(size_t number_of_bytes) {
  return static_cast<float*>(pffft_aligned_malloc(number_of_bytes));
}

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {

  const size_t fft_size = size / kSizeOfOneSample;
  if (!IsValidSize(fft_size)) {
    return 0;
  }

  const size_t number_of_bytes = fft_size * kSizeOfOneSample;
  assert(number_of_bytes <= size);

  float* in = AllocatePffftBuffer(number_of_bytes);
  float* out = AllocatePffftBuffer(number_of_bytes);

  std::memcpy(in, reinterpret_cast<const float*>(data), number_of_bytes);

  PFFFT_Setup* pffft_setup = pffft_new_setup(fft_size, kTransform);

  pffft_transform(pffft_setup, in, out, nullptr, PFFFT_FORWARD);
  pffft_zconvolve_accumulate(pffft_setup, out, out, out, 1.f);
  pffft_transform_ordered(pffft_setup, in, out, nullptr, PFFFT_BACKWARD);

  pffft_aligned_free(in);
  pffft_aligned_free(out);
  pffft_destroy_setup(pffft_setup);

  return 0;
}
