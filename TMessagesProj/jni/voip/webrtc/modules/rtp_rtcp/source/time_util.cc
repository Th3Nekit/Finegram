/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/time_util.h"

#include <algorithm>

#include "rtc_base/checks.h"
#include "rtc_base/numerics/divide_round.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

uint32_t SaturatedToCompactNtp(TimeDelta delta) {
  constexpr uint32_t kMaxCompactNtp = 0xFFFFFFFF;
  constexpr int kCompactNtpInSecond = 0x10000;
  if (delta <= TimeDelta::Zero())
    return 0;
  if (delta.us() >=
      kMaxCompactNtp * rtc::kNumMicrosecsPerSec / kCompactNtpInSecond)
    return kMaxCompactNtp;

  return DivideRoundToNearest(delta.us() * kCompactNtpInSecond,
                              rtc::kNumMicrosecsPerSec);
}

TimeDelta CompactNtpRttToTimeDelta(uint32_t compact_ntp_interval) {
  static constexpr TimeDelta kMinRtt = TimeDelta::Millis(1);

  if (compact_ntp_interval > 0x80000000)
    return kMinRtt;

  int64_t value = static_cast<int64_t>(compact_ntp_interval);

  int64_t us = DivideRoundToNearest(value * rtc::kNumMicrosecsPerSec, 1 << 16);

  return std::max(TimeDelta::Micros(us), kMinRtt);
}
}
