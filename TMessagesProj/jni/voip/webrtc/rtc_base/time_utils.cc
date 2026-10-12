/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdint.h>

#if defined(WEBRTC_POSIX)
#include <sys/time.h>
#endif

#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/system_time.h"
#include "rtc_base/time_utils.h"
#if defined(WEBRTC_WIN)
#include "rtc_base/win32.h"
#endif
#if defined(WEBRTC_WIN)
#include <minwinbase.h>
#endif

namespace rtc {

#if defined(WEBRTC_WIN) || defined(WINUWP)

static constexpr uint64_t kFileTimeToUnixTimeEpochOffset =
    116444736000000000ULL;
static constexpr uint64_t kFileTimeToMicroSeconds = 10LL;
#endif

ClockInterface* g_clock = nullptr;

ClockInterface* SetClockForTesting(ClockInterface* clock) {
  ClockInterface* prev = g_clock;
  g_clock = clock;
  return prev;
}

ClockInterface* GetClockForTesting() {
  return g_clock;
}

#if defined(WINUWP)

namespace {

class TimeHelper final {
 public:
  TimeHelper(const TimeHelper&) = delete;

  static void SyncWithNtp(int64_t ntp_server_time_ms) {
    auto& singleton = Singleton();
    TIME_ZONE_INFORMATION time_zone;
    GetTimeZoneInformation(&time_zone);
    int64_t time_zone_bias_ns =
        rtc::dchecked_cast<int64_t>(time_zone.Bias) * 60 * 1000 * 1000 * 1000;
    singleton.app_start_time_ns_ =
        (ntp_server_time_ms - kNTPTimeToUnixTimeEpochOffset) * 1000000 -
        time_zone_bias_ns;
    singleton.UpdateReferenceTime();
  }

  static int64_t TicksNs() {
    auto& singleton = Singleton();
    int64_t result = 0;
    LARGE_INTEGER qpcnt;
    QueryPerformanceCounter(&qpcnt);
    result = rtc::dchecked_cast<int64_t>(
        (rtc::dchecked_cast<uint64_t>(qpcnt.QuadPart) * 100000 /
         rtc::dchecked_cast<uint64_t>(singleton.os_ticks_per_second_)) *
        10000);
    result = singleton.app_start_time_ns_ + result -
             singleton.time_since_os_start_ns_;
    return result;
  }

 private:
  TimeHelper() {
    TIME_ZONE_INFORMATION time_zone;
    GetTimeZoneInformation(&time_zone);
    int64_t time_zone_bias_ns =
        rtc::dchecked_cast<int64_t>(time_zone.Bias) * 60 * 1000 * 1000 * 1000;
    FILETIME ft;

    GetSystemTimeAsFileTime(&ft);
    LARGE_INTEGER li;
    li.HighPart = ft.dwHighDateTime;
    li.LowPart = ft.dwLowDateTime;

    app_start_time_ns_ = (li.QuadPart - kFileTimeToUnixTimeEpochOffset) * 100 -
                         time_zone_bias_ns;

    UpdateReferenceTime();
  }

  static TimeHelper& Singleton() {
    static TimeHelper singleton;
    return singleton;
  }

  void UpdateReferenceTime() {
    LARGE_INTEGER qpfreq;
    QueryPerformanceFrequency(&qpfreq);
    os_ticks_per_second_ = rtc::dchecked_cast<int64_t>(qpfreq.QuadPart);

    LARGE_INTEGER qpcnt;
    QueryPerformanceCounter(&qpcnt);
    time_since_os_start_ns_ = rtc::dchecked_cast<int64_t>(
        (rtc::dchecked_cast<uint64_t>(qpcnt.QuadPart) * 100000 /
         rtc::dchecked_cast<uint64_t>(os_ticks_per_second_)) *
        10000);
  }

 private:
  static constexpr uint64_t kNTPTimeToUnixTimeEpochOffset = 2208988800000L;

  int64_t app_start_time_ns_;

  int64_t time_since_os_start_ns_;

  int64_t os_ticks_per_second_;
};

}

void SyncWithNtp(int64_t time_from_ntp_server_ms) {
  TimeHelper::SyncWithNtp(time_from_ntp_server_ms);
}

int64_t WinUwpSystemTimeNanos() {
  return TimeHelper::TicksNs();
}

#endif

int64_t SystemTimeMillis() {
  return static_cast<int64_t>(SystemTimeNanos() / kNumNanosecsPerMillisec);
}

int64_t TimeNanos() {
  if (g_clock) {
    return g_clock->TimeNanos();
  }
  return SystemTimeNanos();
}

uint32_t Time32() {
  return static_cast<uint32_t>(TimeNanos() / kNumNanosecsPerMillisec);
}

int64_t TimeMillis() {
  return TimeNanos() / kNumNanosecsPerMillisec;
}

int64_t TimeMicros() {
  return TimeNanos() / kNumNanosecsPerMicrosec;
}

int64_t TimeAfter(int64_t elapsed) {
  RTC_DCHECK_GE(elapsed, 0);
  return TimeMillis() + elapsed;
}

int32_t TimeDiff32(uint32_t later, uint32_t earlier) {
  return later - earlier;
}

int64_t TimeDiff(int64_t later, int64_t earlier) {
  return later - earlier;
}

int64_t TmToSeconds(const tm& tm) {
  static short int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  static short int cumul_mdays[12] = {0,   31,  59,  90,  120, 151,
                                      181, 212, 243, 273, 304, 334};
  int year = tm.tm_year + 1900;
  int month = tm.tm_mon;
  int day = tm.tm_mday - 1;
  int hour = tm.tm_hour;
  int min = tm.tm_min;
  int sec = tm.tm_sec;

  bool expiry_in_leap_year =
      (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));

  if (year < 1970)
    return -1;
  if (month < 0 || month > 11)
    return -1;
  if (day < 0 || day >= mdays[month] + (expiry_in_leap_year && month == 2 - 1))
    return -1;
  if (hour < 0 || hour > 23)
    return -1;
  if (min < 0 || min > 59)
    return -1;
  if (sec < 0 || sec > 59)
    return -1;

  day += cumul_mdays[month];

  day += ((year / 4 - 1970 / 4) - (year / 100 - 1970 / 100) +
          (year / 400 - 1970 / 400));

  if (expiry_in_leap_year && month <= 2 - 1)
    day -= 1;

  return (((static_cast<int64_t>(year - 1970) * 365 + day) * 24 + hour) * 60 +
          min) *
             60 +
         sec;
}

int64_t TimeUTCMicros() {
  if (g_clock) {
    return g_clock->TimeNanos() / kNumNanosecsPerMicrosec;
  }
#if defined(WEBRTC_POSIX)
  struct timeval time;
  gettimeofday(&time, nullptr);

  return (static_cast<int64_t>(time.tv_sec) * rtc::kNumMicrosecsPerSec +
          time.tv_usec);
#elif defined(WEBRTC_WIN)
  FILETIME ft;

  GetSystemTimeAsFileTime(&ft);
  LARGE_INTEGER li;
  li.HighPart = ft.dwHighDateTime;
  li.LowPart = ft.dwLowDateTime;
  return (li.QuadPart - kFileTimeToUnixTimeEpochOffset) /
         kFileTimeToMicroSeconds;
#endif
}

int64_t TimeUTCMillis() {
  return TimeUTCMicros() / kNumMicrosecsPerMillisec;
}

}
