/*
 *  Copyright 2005 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TIME_UTILS_H_
#define RTC_BASE_TIME_UTILS_H_

#include <stdint.h>
#include <time.h>

#include "rtc_base/checks.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/system_time.h"

namespace rtc {

static const int64_t kNumMillisecsPerSec = INT64_C(1000);
static const int64_t kNumMicrosecsPerSec = INT64_C(1000000);
static const int64_t kNumNanosecsPerSec = INT64_C(1000000000);

static const int64_t kNumMicrosecsPerMillisec =
    kNumMicrosecsPerSec / kNumMillisecsPerSec;
static const int64_t kNumNanosecsPerMillisec =
    kNumNanosecsPerSec / kNumMillisecsPerSec;
static const int64_t kNumNanosecsPerMicrosec =
    kNumNanosecsPerSec / kNumMicrosecsPerSec;

constexpr int64_t kNtpJan1970Millisecs = 2'208'988'800 * kNumMillisecsPerSec;

class ClockInterface {
 public:
  virtual ~ClockInterface() {}
  virtual int64_t TimeNanos() const = 0;
};

RTC_EXPORT ClockInterface* SetClockForTesting(ClockInterface* clock);

RTC_EXPORT ClockInterface* GetClockForTesting();

#if defined(WINUWP)

void SyncWithNtp(int64_t time_from_ntp_server_ms);

int64_t WinUwpSystemTimeNanos();
#endif

int64_t SystemTimeMillis();

uint32_t Time32();

RTC_EXPORT int64_t TimeMillis();

inline int64_t Time() {
  return TimeMillis();
}

RTC_EXPORT int64_t TimeMicros();

RTC_EXPORT int64_t TimeNanos();

int64_t TimeAfter(int64_t elapsed);

int64_t TimeDiff(int64_t later, int64_t earlier);
int32_t TimeDiff32(uint32_t later, uint32_t earlier);

inline int64_t TimeSince(int64_t earlier) {
  return TimeMillis() - earlier;
}

inline int64_t TimeUntil(int64_t later) {
  return later - TimeMillis();
}

int64_t TmToSeconds(const tm& tm);

int64_t TimeUTCMicros();

int64_t TimeUTCMillis();

}

#endif
