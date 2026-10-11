/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_MERGE_H_
#define MODULES_AUDIO_CODING_NETEQ_MERGE_H_

#include "modules/audio_coding/neteq/audio_multi_vector.h"

namespace webrtc {

class Expand;
class SyncBuffer;

class Merge {
 public:
  Merge(int fs_hz,
        size_t num_channels,
        Expand* expand,
        SyncBuffer* sync_buffer);
  virtual ~Merge();

  Merge(const Merge&) = delete;
  Merge& operator=(const Merge&) = delete;

  virtual size_t Process(int16_t* input,
                         size_t input_length,
                         AudioMultiVector* output);

  virtual size_t RequiredFutureSamples();

 protected:
  const int fs_hz_;
  const size_t num_channels_;

 private:
  static const int kMaxSampleRate = 48000;
  static const size_t kExpandDownsampLength = 100;
  static const size_t kInputDownsampLength = 40;
  static const size_t kMaxCorrelationLength = 60;

  size_t GetExpandedSignal(size_t* old_length, size_t* expand_period);

  int16_t SignalScaling(const int16_t* input,
                        size_t input_length,
                        const int16_t* expanded_signal) const;

  void Downsample(const int16_t* input,
                  size_t input_length,
                  const int16_t* expanded_signal,
                  size_t expanded_length);

  size_t CorrelateAndPeakSearch(size_t start_position,
                                size_t input_length,
                                size_t expand_period) const;

  const int fs_mult_;
  const size_t timestamps_per_call_;
  Expand* expand_;
  SyncBuffer* sync_buffer_;
  int16_t expanded_downsampled_[kExpandDownsampLength];
  int16_t input_downsampled_[kInputDownsampLength];
  AudioMultiVector expanded_;
  std::vector<int16_t> temp_data_;
};

}
#endif
