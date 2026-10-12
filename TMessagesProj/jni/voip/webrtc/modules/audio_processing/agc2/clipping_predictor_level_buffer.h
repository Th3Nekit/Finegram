/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_CLIPPING_PREDICTOR_LEVEL_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AGC2_CLIPPING_PREDICTOR_LEVEL_BUFFER_H_

#include <memory>
#include <vector>

#include "absl/types/optional.h"

namespace webrtc {

class ClippingPredictorLevelBuffer {
 public:
  struct Level {
    float average;
    float max;
    bool operator==(const Level& level) const;
  };

  static constexpr int kMaxCapacity = 100;

  explicit ClippingPredictorLevelBuffer(int capacity);
  ~ClippingPredictorLevelBuffer() {}
  ClippingPredictorLevelBuffer(const ClippingPredictorLevelBuffer&) = delete;
  ClippingPredictorLevelBuffer& operator=(const ClippingPredictorLevelBuffer&) =
      delete;

  void Reset();

  int Size() const { return size_; }

  int Capacity() const { return data_.size(); }

  void Push(Level level);

  absl::optional<Level> ComputePartialMetrics(int delay, int num_items) const;

 private:
  int tail_;
  int size_;
  std::vector<Level> data_;
};

}

#endif
