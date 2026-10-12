/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_OPUS_TEST_LAPPED_TRANSFORM_H_
#define MODULES_AUDIO_CODING_CODECS_OPUS_TEST_LAPPED_TRANSFORM_H_

#include <complex>
#include <memory>

#include "common_audio/real_fourier.h"
#include "modules/audio_coding/codecs/opus/test/blocker.h"
#include "rtc_base/memory/aligned_malloc.h"

namespace webrtc {

template <typename T>
class AlignedArray {
 public:
  AlignedArray(size_t rows, size_t cols, size_t alignment)
      : rows_(rows), cols_(cols) {
    RTC_CHECK_GT(alignment, 0);
    head_row_ =
        static_cast<T**>(AlignedMalloc(rows_ * sizeof(*head_row_), alignment));
    for (size_t i = 0; i < rows_; ++i) {
      head_row_[i] = static_cast<T*>(
          AlignedMalloc(cols_ * sizeof(**head_row_), alignment));
    }
  }

  ~AlignedArray() {
    for (size_t i = 0; i < rows_; ++i) {
      AlignedFree(head_row_[i]);
    }
    AlignedFree(head_row_);
  }

  T* const* Array() { return head_row_; }

  const T* const* Array() const { return head_row_; }

  T* Row(size_t row) {
    RTC_CHECK_LE(row, rows_);
    return head_row_[row];
  }

  const T* Row(size_t row) const {
    RTC_CHECK_LE(row, rows_);
    return head_row_[row];
  }

 private:
  size_t rows_;
  size_t cols_;
  T** head_row_;
};

class LappedTransform {
 public:
  class Callback {
   public:
    virtual ~Callback() {}

    virtual void ProcessAudioBlock(const std::complex<float>* const* in_block,
                                   size_t num_in_channels,
                                   size_t frames,
                                   size_t num_out_channels,
                                   std::complex<float>* const* out_block) = 0;
  };

  LappedTransform(size_t num_in_channels,
                  size_t num_out_channels,
                  size_t chunk_length,
                  const float* window,
                  size_t block_length,
                  size_t shift_amount,
                  Callback* callback);
  ~LappedTransform();

  void ProcessChunk(const float* const* in_chunk, float* const* out_chunk);

  size_t chunk_length() const { return chunk_length_; }

  size_t num_in_channels() const { return num_in_channels_; }

  size_t num_out_channels() const { return num_out_channels_; }

  size_t initial_delay() const { return blocker_.initial_delay(); }

 private:

  class BlockThunk : public BlockerCallback {
   public:
    explicit BlockThunk(LappedTransform* parent) : parent_(parent) {}

    void ProcessBlock(const float* const* input,
                      size_t num_frames,
                      size_t num_input_channels,
                      size_t num_output_channels,
                      float* const* output) override;

   private:
    LappedTransform* const parent_;
  } blocker_callback_;

  const size_t num_in_channels_;
  const size_t num_out_channels_;

  const size_t block_length_;
  const size_t chunk_length_;

  Callback* const block_processor_;
  Blocker blocker_;

  std::unique_ptr<RealFourier> fft_;
  const size_t cplx_length_;
  AlignedArray<float> real_buf_;
  AlignedArray<std::complex<float> > cplx_pre_;
  AlignedArray<std::complex<float> > cplx_post_;
};

}

#endif
