/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_QUALITY_LIMITATION_REASON_TRACKER_H_
#define VIDEO_QUALITY_LIMITATION_REASON_TRACKER_H_

#include <map>

#include "common_video/include/quality_limitation_reason.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class QualityLimitationReasonTracker {
 public:

  explicit QualityLimitationReasonTracker(Clock* clock);

  QualityLimitationReason current_reason() const;
  void SetReason(QualityLimitationReason reason);
  std::map<QualityLimitationReason, int64_t> DurationsMs() const;

 private:
  Clock* const clock_;
  QualityLimitationReason current_reason_;
  int64_t current_reason_updated_timestamp_ms_;

  std::map<QualityLimitationReason, int64_t> durations_ms_;
};

}

#endif
