/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TRANSIENT_TRANSIENT_DETECTOR_H_
#define MODULES_AUDIO_PROCESSING_TRANSIENT_TRANSIENT_DETECTOR_H_

#include <stddef.h>

#include <deque>
#include <memory>

#include "modules/audio_processing/transient/moving_moments.h"
#include "modules/audio_processing/transient/wpd_tree.h"

namespace webrtc {

class TransientDetector {
 public:

  TransientDetector(int sample_rate_hz);

  ~TransientDetector();

  float Detect(const float* data,
               size_t data_length,
               const float* reference_data,
               size_t reference_length);

  bool using_reference() { return using_reference_; }

 private:
  float ReferenceDetectionValue(const float* data, size_t length);

  static const size_t kLevels = 3;
  static const size_t kLeaves = 1 << kLevels;

  size_t samples_per_chunk_;

  std::unique_ptr<WPDTree> wpd_tree_;
  size_t tree_leaves_data_length_;

  std::unique_ptr<MovingMoments> moving_moments_[kLeaves];

  std::unique_ptr<float[]> first_moments_;
  std::unique_ptr<float[]> second_moments_;

  float last_first_moment_[kLeaves];
  float last_second_moment_[kLeaves];

  std::deque<float> previous_results_;

  int chunks_at_startup_left_to_delete_;

  float reference_energy_;

  bool using_reference_;
};

}

#endif
