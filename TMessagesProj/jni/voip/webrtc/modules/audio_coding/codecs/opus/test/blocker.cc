/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/opus/test/blocker.h"

#include <string.h>

#include "rtc_base/checks.h"

namespace {

void AddFrames(const float* const* a,
               size_t a_start_index,
               const float* const* b,
               int b_start_index,
               size_t num_frames,
               size_t num_channels,
               float* const* result,
               size_t result_start_index) {
  for (size_t i = 0; i < num_channels; ++i) {
    for (size_t j = 0; j < num_frames; ++j) {
      result[i][j + result_start_index] =
          a[i][j + a_start_index] + b[i][j + b_start_index];
    }
  }
}

void CopyFrames(const float* const* src,
                size_t src_start_index,
                size_t num_frames,
                size_t num_channels,
                float* const* dst,
                size_t dst_start_index) {
  for (size_t i = 0; i < num_channels; ++i) {
    memcpy(&dst[i][dst_start_index], &src[i][src_start_index],
           num_frames * sizeof(dst[i][dst_start_index]));
  }
}

void MoveFrames(const float* const* src,
                size_t src_start_index,
                size_t num_frames,
                size_t num_channels,
                float* const* dst,
                size_t dst_start_index) {
  for (size_t i = 0; i < num_channels; ++i) {
    memmove(&dst[i][dst_start_index], &src[i][src_start_index],
            num_frames * sizeof(dst[i][dst_start_index]));
  }
}

void ZeroOut(float* const* buffer,
             size_t starting_idx,
             size_t num_frames,
             size_t num_channels) {
  for (size_t i = 0; i < num_channels; ++i) {
    memset(&buffer[i][starting_idx], 0,
           num_frames * sizeof(buffer[i][starting_idx]));
  }
}

void ApplyWindow(const float* window,
                 size_t num_frames,
                 size_t num_channels,
                 float* const* frames) {
  for (size_t i = 0; i < num_channels; ++i) {
    for (size_t j = 0; j < num_frames; ++j) {
      frames[i][j] = frames[i][j] * window[j];
    }
  }
}

size_t gcd(size_t a, size_t b) {
  size_t tmp;
  while (b) {
    tmp = a;
    a = b;
    b = tmp % b;
  }
  return a;
}

}

namespace webrtc {

Blocker::Blocker(size_t chunk_size,
                 size_t block_size,
                 size_t num_input_channels,
                 size_t num_output_channels,
                 const float* window,
                 size_t shift_amount,
                 BlockerCallback* callback)
    : chunk_size_(chunk_size),
      block_size_(block_size),
      num_input_channels_(num_input_channels),
      num_output_channels_(num_output_channels),
      initial_delay_(block_size_ - gcd(chunk_size, shift_amount)),
      frame_offset_(0),
      input_buffer_(num_input_channels_, chunk_size_ + initial_delay_),
      output_buffer_(chunk_size_ + initial_delay_, num_output_channels_),
      input_block_(block_size_, num_input_channels_),
      output_block_(block_size_, num_output_channels_),
      window_(new float[block_size_]),
      shift_amount_(shift_amount),
      callback_(callback) {
  RTC_CHECK_LE(num_output_channels_, num_input_channels_);
  RTC_CHECK_LE(shift_amount_, block_size_);

  memcpy(window_.get(), window, block_size_ * sizeof(*window_.get()));
  input_buffer_.MoveReadPositionBackward(initial_delay_);
}

Blocker::~Blocker() = default;

void Blocker::ProcessChunk(const float* const* input,
                           size_t chunk_size,
                           size_t num_input_channels,
                           size_t num_output_channels,
                           float* const* output) {
  RTC_CHECK_EQ(chunk_size, chunk_size_);
  RTC_CHECK_EQ(num_input_channels, num_input_channels_);
  RTC_CHECK_EQ(num_output_channels, num_output_channels_);

  input_buffer_.Write(input, num_input_channels, chunk_size_);
  size_t first_frame_in_block = frame_offset_;

  while (first_frame_in_block < chunk_size_) {
    input_buffer_.Read(input_block_.channels(), num_input_channels,
                       block_size_);
    input_buffer_.MoveReadPositionBackward(block_size_ - shift_amount_);

    ApplyWindow(window_.get(), block_size_, num_input_channels_,
                input_block_.channels());
    callback_->ProcessBlock(input_block_.channels(), block_size_,
                            num_input_channels_, num_output_channels_,
                            output_block_.channels());
    ApplyWindow(window_.get(), block_size_, num_output_channels_,
                output_block_.channels());

    AddFrames(output_buffer_.channels(), first_frame_in_block,
              output_block_.channels(), 0, block_size_, num_output_channels_,
              output_buffer_.channels(), first_frame_in_block);

    first_frame_in_block += shift_amount_;
  }

  CopyFrames(output_buffer_.channels(), 0, chunk_size_, num_output_channels_,
             output, 0);

  MoveFrames(output_buffer_.channels(), chunk_size, initial_delay_,
             num_output_channels_, output_buffer_.channels(), 0);
  ZeroOut(output_buffer_.channels(), initial_delay_, chunk_size_,
          num_output_channels_);

  frame_offset_ = first_frame_in_block - chunk_size_;
}

}
