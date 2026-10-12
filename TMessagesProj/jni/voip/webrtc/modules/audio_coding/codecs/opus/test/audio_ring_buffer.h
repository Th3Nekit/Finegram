/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_AUDIO_CODING_CODECS_OPUS_TEST_AUDIO_RING_BUFFER_H_
#define MODULES_AUDIO_CODING_CODECS_OPUS_TEST_AUDIO_RING_BUFFER_H_

#include <stddef.h>

#include <memory>
#include <vector>

struct RingBuffer;

namespace webrtc {

class AudioRingBuffer final {
 public:

  AudioRingBuffer(size_t channels, size_t max_frames);
  ~AudioRingBuffer();

  void Write(const float* const* data, size_t channels, size_t frames);

  void Read(float* const* data, size_t channels, size_t frames);

  size_t ReadFramesAvailable() const;
  size_t WriteFramesAvailable() const;

  void MoveReadPositionForward(size_t frames);
  void MoveReadPositionBackward(size_t frames);

 private:

  std::vector<RingBuffer*> buffers_;
};

}

#endif
