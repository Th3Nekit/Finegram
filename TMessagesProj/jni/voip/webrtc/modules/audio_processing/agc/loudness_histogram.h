/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC_LOUDNESS_HISTOGRAM_H_
#define MODULES_AUDIO_PROCESSING_AGC_LOUDNESS_HISTOGRAM_H_

#include <stdint.h>

#include <memory>

namespace webrtc {

class LoudnessHistogram {
 public:

  static LoudnessHistogram* Create();

  static LoudnessHistogram* Create(int window_size);
  ~LoudnessHistogram();

  void Update(double rms, double activity_probability);

  void Reset();

  double CurrentRms() const;

  double AudioContent() const;

  int num_updates() const { return num_updates_; }

 private:
  LoudnessHistogram();
  explicit LoudnessHistogram(int window);

  int GetBinIndex(double rms);

  void RemoveOldestEntryAndUpdate();
  void InsertNewestEntryAndUpdate(int activity_prob_q10, int hist_index);
  void UpdateHist(int activity_prob_q10, int hist_index);
  void RemoveTransient();

  static const int kHistSize = 77;

  int num_updates_;

  int64_t audio_content_q10_;

  int64_t bin_count_q10_[kHistSize];

  std::unique_ptr<int[]> activity_probability_;

  std::unique_ptr<int[]> hist_bin_index_;

  int buffer_index_;

  int buffer_is_full_;

  int len_circular_buffer_;
  int len_high_activity_;
};

}

#endif
