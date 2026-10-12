/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Modified from the Chromium original here:
// src/media/base/sinc_resampler.h

#ifndef COMMON_AUDIO_RESAMPLER_SINC_RESAMPLER_H_
#define COMMON_AUDIO_RESAMPLER_SINC_RESAMPLER_H_

#include <stddef.h>

#include <memory>

#include "rtc_base/gtest_prod_util.h"
#include "rtc_base/memory/aligned_malloc.h"
#include "rtc_base/system/arch.h"

namespace webrtc {

class SincResamplerCallback {
 public:
  virtual ~SincResamplerCallback() {}
  virtual void Run(size_t frames, float* destination) = 0;
};

class SincResampler {
 public:

  static const size_t kKernelSize = 32;

  static const size_t kDefaultRequestSize = 512;

  static const size_t kKernelOffsetCount = 32;
  static const size_t kKernelStorageSize =
      kKernelSize * (kKernelOffsetCount + 1);

  SincResampler(double io_sample_rate_ratio,
                size_t request_frames,
                SincResamplerCallback* read_cb);
  virtual ~SincResampler();

  SincResampler(const SincResampler&) = delete;
  SincResampler& operator=(const SincResampler&) = delete;

  void Resample(size_t frames, float* destination);

  size_t ChunkSize() const;

  size_t request_frames() const { return request_frames_; }

  void Flush();

  void SetRatio(double io_sample_rate_ratio);

  float* get_kernel_for_testing() { return kernel_storage_.get(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(SincResamplerTest, Convolve);
  FRIEND_TEST_ALL_PREFIXES(SincResamplerTest, ConvolveBenchmark);

  void InitializeKernel();
  void UpdateRegions(bool second_load);

  void InitializeCPUSpecificFeatures();

  static float Convolve_C(const float* input_ptr,
                          const float* k1,
                          const float* k2,
                          double kernel_interpolation_factor);
#if defined(WEBRTC_ARCH_X86_FAMILY)
  static float Convolve_SSE(const float* input_ptr,
                            const float* k1,
                            const float* k2,
                            double kernel_interpolation_factor);
  static float Convolve_AVX2(const float* input_ptr,
                             const float* k1,
                             const float* k2,
                             double kernel_interpolation_factor);
#elif defined(WEBRTC_HAS_NEON)
  static float Convolve_NEON(const float* input_ptr,
                             const float* k1,
                             const float* k2,
                             double kernel_interpolation_factor);
#endif

  double io_sample_rate_ratio_;

  double virtual_source_idx_;

  bool buffer_primed_;

  SincResamplerCallback* read_cb_;

  const size_t request_frames_;

  size_t block_size_;

  const size_t input_buffer_size_;

  std::unique_ptr<float[], AlignedFreeDeleter> kernel_storage_;
  std::unique_ptr<float[], AlignedFreeDeleter> kernel_pre_sinc_storage_;
  std::unique_ptr<float[], AlignedFreeDeleter> kernel_window_storage_;

  std::unique_ptr<float[], AlignedFreeDeleter> input_buffer_;

  typedef float (*ConvolveProc)(const float*,
                                const float*,
                                const float*,
                                double);
  ConvolveProc convolve_proc_;

  float* r0_;
  float* const r1_;
  float* const r2_;
  float* r3_;
  float* r4_;
};

}

#endif
