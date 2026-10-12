/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_REAL_FOURIER_H_
#define COMMON_AUDIO_REAL_FOURIER_H_

#include <stddef.h>

#include <complex>
#include <memory>

#include "rtc_base/memory/aligned_malloc.h"

namespace webrtc {

class RealFourier {
 public:

  typedef std::unique_ptr<float[], AlignedFreeDeleter> fft_real_scoper;
  typedef std::unique_ptr<std::complex<float>[], AlignedFreeDeleter>
      fft_cplx_scoper;

  static const size_t kFftBufferAlignment;

  static std::unique_ptr<RealFourier> Create(int fft_order);
  virtual ~RealFourier() {}

  static int FftOrder(size_t length);

  static size_t FftLength(int order);

  static size_t ComplexLength(int order);

  static fft_real_scoper AllocRealBuffer(int count);
  static fft_cplx_scoper AllocCplxBuffer(int count);

  virtual void Forward(const float* src, std::complex<float>* dest) const = 0;

  virtual void Inverse(const std::complex<float>* src, float* dest) const = 0;

  virtual int order() const = 0;
};

}

#endif
