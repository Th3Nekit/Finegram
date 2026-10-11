/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_FRAME_RATE_ESTIMATOR_H_
#define COMMON_VIDEO_FRAME_RATE_ESTIMATOR_H_

#include <deque>

#include "absl/types/optional.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {

class FrameRateEstimator {
 public:
  explicit FrameRateEstimator(TimeDelta averaging_window);

  void OnFrame(Timestamp time);

  absl::optional<double> GetAverageFps() const;

  absl::optional<double> GetAverageFps(Timestamp now);

  void Reset();

 private:
  void CullOld(Timestamp now);
  const TimeDelta averaging_window_;
  std::deque<Timestamp> frame_times_;
};

}

#endif
