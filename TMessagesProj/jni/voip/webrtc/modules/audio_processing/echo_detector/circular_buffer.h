/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_ECHO_DETECTOR_CIRCULAR_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_ECHO_DETECTOR_CIRCULAR_BUFFER_H_

#include <stddef.h>

#include <vector>

#include "absl/types/optional.h"

namespace webrtc {

struct CircularBuffer {
 public:
  explicit CircularBuffer(size_t size);
  ~CircularBuffer();

  void Push(float value);
  absl::optional<float> Pop();
  size_t Size() const { return nr_elements_in_buffer_; }

  void Clear();

 private:
  std::vector<float> buffer_;
  size_t next_insertion_index_ = 0;

  size_t nr_elements_in_buffer_ = 0;
};

}

#endif
