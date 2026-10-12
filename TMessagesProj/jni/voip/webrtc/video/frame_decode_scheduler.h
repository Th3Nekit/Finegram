/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_FRAME_DECODE_SCHEDULER_H_
#define VIDEO_FRAME_DECODE_SCHEDULER_H_

#include <stdint.h>

#include "absl/functional/any_invocable.h"
#include "absl/types/optional.h"
#include "api/units/timestamp.h"
#include "video/frame_decode_timing.h"

namespace webrtc {

class FrameDecodeScheduler {
 public:

  using FrameReleaseCallback =
      absl::AnyInvocable<void(uint32_t rtp_timestamp,
                              Timestamp render_time) &&>;

  virtual ~FrameDecodeScheduler() = default;

  virtual absl::optional<uint32_t> ScheduledRtpTimestamp() = 0;

  virtual void ScheduleFrame(uint32_t rtp,
                             FrameDecodeTiming::FrameSchedule schedule,
                             FrameReleaseCallback callback) = 0;

  virtual void CancelOutstanding() = 0;

  virtual void Stop() = 0;
};

}

#endif
