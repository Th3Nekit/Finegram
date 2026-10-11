/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_HISTOGRAM_H_
#define MODULES_AUDIO_CODING_NETEQ_HISTOGRAM_H_

#include <string.h>

#include <vector>

#include "absl/types/optional.h"

namespace webrtc {

class Histogram {
 public:

  Histogram(size_t num_buckets,
            int forget_factor,
            absl::optional<double> start_forget_weight = absl::nullopt);

  virtual ~Histogram();

  virtual void Reset();

  virtual void Add(int index);

  virtual int Quantile(int probability);

  virtual int NumBuckets() const;

  const std::vector<int>& buckets() const { return buckets_; }

  int base_forget_factor_for_testing() const { return base_forget_factor_; }
  int forget_factor_for_testing() const { return forget_factor_; }
  absl::optional<double> start_forget_weight_for_testing() const {
    return start_forget_weight_;
  }

 private:
  std::vector<int> buckets_;
  int forget_factor_;
  const int base_forget_factor_;
  int add_count_;
  const absl::optional<double> start_forget_weight_;
};

}

#endif
