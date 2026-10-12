/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_REMOTE_NTP_TIME_ESTIMATOR_H_
#define MODULES_RTP_RTCP_INCLUDE_REMOTE_NTP_TIME_ESTIMATOR_H_

#include <stdint.h>

#include "absl/types/optional.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/numerics/moving_percentile_filter.h"
#include "system_wrappers/include/rtp_to_ntp_estimator.h"

namespace webrtc {

class Clock;

class RemoteNtpTimeEstimator {
 public:
  explicit RemoteNtpTimeEstimator(Clock* clock);
  RemoteNtpTimeEstimator(const RemoteNtpTimeEstimator&) = delete;
  RemoteNtpTimeEstimator& operator=(const RemoteNtpTimeEstimator&) = delete;
  ~RemoteNtpTimeEstimator() = default;

  bool UpdateRtcpTimestamp(TimeDelta rtt,
                           NtpTime sender_send_time,
                           uint32_t rtp_timestamp);

  int64_t Estimate(uint32_t rtp_timestamp) {
    NtpTime ntp_time = EstimateNtp(rtp_timestamp);
    if (!ntp_time.Valid()) {
      return -1;
    }
    return ntp_time.ToMs();
  }

  NtpTime EstimateNtp(uint32_t rtp_timestamp);

  absl::optional<int64_t> EstimateRemoteToLocalClockOffset();

 private:
  Clock* clock_;

  MovingMedianFilter<int64_t> ntp_clocks_offset_estimator_;
  RtpToNtpEstimator rtp_to_ntp_;
  Timestamp last_timing_log_ = Timestamp::MinusInfinity();
};

}

#endif
