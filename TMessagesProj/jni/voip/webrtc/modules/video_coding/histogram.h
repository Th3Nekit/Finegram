/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_HISTOGRAM_H_
#define MODULES_VIDEO_CODING_HISTOGRAM_H_

#include <cstddef>
#include <vector>

namespace webrtc {
namespace video_coding {
class Histogram {
 public:

  Histogram(size_t num_buckets, size_t max_num_values);

  void Add(size_t value);

  size_t InverseCdf(float probability) const;

  size_t NumValues() const;

 private:

  std::vector<size_t> values_;
  std::vector<size_t> buckets_;
  size_t index_;
};

}
}

#endif
