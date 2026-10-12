/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_OPUS_TEST_BLOCKER_H_
#define MODULES_AUDIO_CODING_CODECS_OPUS_TEST_BLOCKER_H_

#include <memory>

#include "common_audio/channel_buffer.h"
#include "modules/audio_coding/codecs/opus/test/audio_ring_buffer.h"

namespace webrtc {

class BlockerCallback {
 public:
  virtual ~BlockerCallback() {}

  virtual void ProcessBlock(const float* const* input,
                            size_t num_frames,
                            size_t num_input_channels,
                            size_t num_output_channels,
                            float* const* output) = 0;
};

class Blocker {
 public:
  Blocker(size_t chunk_size,
          size_t block_size,
          size_t num_input_channels,
          size_t num_output_channels,
          const float* window,
          size_t shift_amount,
          BlockerCallback* callback);
  ~Blocker();

  void ProcessChunk(const float* const* input,
                    size_t chunk_size,
                    size_t num_input_channels,
                    size_t num_output_channels,
                    float* const* output);

  size_t initial_delay() const { return initial_delay_; }

 private:
  const size_t chunk_size_;
  const size_t block_size_;
  const size_t num_input_channels_;
  const size_t num_output_channels_;

  const size_t initial_delay_;

  size_t frame_offset_;

  AudioRingBuffer input_buffer_;
  ChannelBuffer<float> output_buffer_;

  ChannelBuffer<float> input_block_;

  ChannelBuffer<float> output_block_;

  std::unique_ptr<float[]> window_;

  size_t shift_amount_;

  BlockerCallback* callback_;
};

}

#endif
