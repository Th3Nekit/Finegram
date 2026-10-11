/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_BUFFER_LEVEL_FILTER_H_
#define MODULES_AUDIO_CODING_NETEQ_BUFFER_LEVEL_FILTER_H_

#include <stddef.h>
#include <stdint.h>

namespace webrtc {

class BufferLevelFilter {
 public:
  BufferLevelFilter();
  virtual ~BufferLevelFilter() {}

  BufferLevelFilter(const BufferLevelFilter&) = delete;
  BufferLevelFilter& operator=(const BufferLevelFilter&) = delete;

  virtual void Reset();

  virtual void Update(size_t buffer_size_samples, int time_stretched_samples);

  virtual void SetFilteredBufferLevel(int buffer_size_samples);

  virtual void SetTargetBufferLevel(int target_buffer_level_ms);

  virtual int filtered_current_level() const {

    return (int64_t{filtered_current_level_} + (1 << 7)) >> 8;
  }

 private:
  int level_factor_;
  int filtered_current_level_;
};

}
#endif
