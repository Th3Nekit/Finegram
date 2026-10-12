/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VIDEO_UNIQUE_TIMESTAMP_COUNTER_H_
#define VIDEO_UNIQUE_TIMESTAMP_COUNTER_H_

#include <cstdint>
#include <memory>
#include <set>

namespace webrtc {

class UniqueTimestampCounter {
 public:
  UniqueTimestampCounter();
  UniqueTimestampCounter(const UniqueTimestampCounter&) = delete;
  UniqueTimestampCounter& operator=(const UniqueTimestampCounter&) = delete;
  ~UniqueTimestampCounter() = default;

  void Add(uint32_t timestamp);

  int GetUniqueSeen() const { return unique_seen_; }

 private:
  int unique_seen_ = 0;

  std::set<uint32_t> search_index_;

  std::unique_ptr<uint32_t[]> latest_;

  int64_t last_ = -1;
};

}

#endif
