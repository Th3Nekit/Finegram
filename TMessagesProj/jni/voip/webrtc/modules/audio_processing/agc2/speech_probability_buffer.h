/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_SPEECH_PROBABILITY_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AGC2_SPEECH_PROBABILITY_BUFFER_H_

#include <vector>

#include "rtc_base/gtest_prod_util.h"

namespace webrtc {

class SpeechProbabilityBuffer {
 public:

  explicit SpeechProbabilityBuffer(float low_probability_threshold);
  ~SpeechProbabilityBuffer() {}
  SpeechProbabilityBuffer(const SpeechProbabilityBuffer&) = delete;
  SpeechProbabilityBuffer& operator=(const SpeechProbabilityBuffer&) = delete;

  void Update(float probability);

  void Reset();

  bool IsActiveSegment() const;

 private:
  void RemoveTransient();

  float GetSumProbabilities() const { return sum_probabilities_; }

  FRIEND_TEST_ALL_PREFIXES(SpeechProbabilityBufferTest,
                           CheckSumAfterInitialization);
  FRIEND_TEST_ALL_PREFIXES(SpeechProbabilityBufferTest, CheckSumAfterUpdate);
  FRIEND_TEST_ALL_PREFIXES(SpeechProbabilityBufferTest, CheckSumAfterReset);
  FRIEND_TEST_ALL_PREFIXES(SpeechProbabilityBufferTest,
                           CheckSumAfterTransientNotRemoved);
  FRIEND_TEST_ALL_PREFIXES(SpeechProbabilityBufferTest,
                           CheckSumAfterTransientRemoved);

  const float low_probability_threshold_;

  float sum_probabilities_ = 0.0f;

  std::vector<float> probabilities_;

  int buffer_index_ = 0;

  int buffer_is_full_ = false;

  int num_high_probability_observations_ = 0;
};

}

#endif
