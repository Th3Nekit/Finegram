/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_UTILITY_PFFFT_WRAPPER_H_
#define MODULES_AUDIO_PROCESSING_UTILITY_PFFFT_WRAPPER_H_

#include <memory>

#include "api/array_view.h"

struct PFFFT_Setup;

namespace webrtc {

class Pffft {
 public:
  enum class FftType { kReal, kComplex };

  class FloatBuffer {
   public:
    FloatBuffer(const FloatBuffer&) = delete;
    FloatBuffer& operator=(const FloatBuffer&) = delete;
    ~FloatBuffer();

    rtc::ArrayView<const float> GetConstView() const;
    rtc::ArrayView<float> GetView();

   private:
    friend class Pffft;
    FloatBuffer(size_t fft_size, FftType fft_type);
    const float* const_data() const { return data_; }
    float* data() { return data_; }
    size_t size() const { return size_; }

    const size_t size_;
    float* const data_;
  };

  Pffft(size_t fft_size, FftType fft_type);
  Pffft(const Pffft&) = delete;
  Pffft& operator=(const Pffft&) = delete;
  ~Pffft();

  static bool IsValidFftSize(size_t fft_size, FftType fft_type);

  static bool IsSimdEnabled();

  std::unique_ptr<FloatBuffer> CreateBuffer() const;

  void ForwardTransform(const FloatBuffer& in, FloatBuffer* out, bool ordered);

  void BackwardTransform(const FloatBuffer& in, FloatBuffer* out, bool ordered);

  void FrequencyDomainConvolve(const FloatBuffer& fft_x,
                               const FloatBuffer& fft_y,
                               FloatBuffer* out,
                               float scaling = 1.f);

 private:
  const size_t fft_size_;
  const FftType fft_type_;
  PFFFT_Setup* pffft_status_;
  float* const scratch_buffer_;
};

}

#endif
