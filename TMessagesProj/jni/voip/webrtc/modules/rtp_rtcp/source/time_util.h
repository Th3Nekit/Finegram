/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_TIME_UTIL_H_
#define MODULES_RTP_RTCP_SOURCE_TIME_UTIL_H_

#include <stdint.h>

#include "api/units/time_delta.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "system_wrappers/include/ntp_time.h"

namespace webrtc {

inline uint32_t CompactNtp(NtpTime ntp) {
  return (ntp.seconds() << 16) | (ntp.fractions() >> 16);
}

uint32_t SaturatedToCompactNtp(TimeDelta delta);

inline constexpr int64_t ToNtpUnits(TimeDelta delta) {

  return (rtc::saturated_cast<int32_t>(delta.us()) * (int64_t{1} << 32)) /
         1'000'000;
}

TimeDelta CompactNtpRttToTimeDelta(uint32_t compact_ntp_interval);

}
#endif
