/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_CAPTURE_CLOCK_OFFSET_UPDATER_H_
#define MODULES_RTP_RTCP_SOURCE_CAPTURE_CLOCK_OFFSET_UPDATER_H_

#include <stdint.h>

#include "absl/types/optional.h"
#include "api/units/time_delta.h"

namespace webrtc {

class CaptureClockOffsetUpdater {
 public:

  absl::optional<int64_t> AdjustEstimatedCaptureClockOffset(
      absl::optional<int64_t> remote_capture_clock_offset) const;

  void SetRemoteToLocalClockOffset(absl::optional<int64_t> offset_q32x32);

  static absl::optional<TimeDelta> ConvertsToTimeDela(
      absl::optional<int64_t> q32x32);

 private:
  absl::optional<int64_t> remote_to_local_clock_offset_;
};

}

#endif
