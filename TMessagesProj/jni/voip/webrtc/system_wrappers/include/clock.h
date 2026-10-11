/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SYSTEM_WRAPPERS_INCLUDE_CLOCK_H_
#define SYSTEM_WRAPPERS_INCLUDE_CLOCK_H_

#include <stdint.h>

#include <atomic>
#include <memory>

#include "api/units/timestamp.h"
#include "rtc_base/system/rtc_export.h"
#include "system_wrappers/include/ntp_time.h"

namespace webrtc {

const uint32_t kNtpJan1970 = 2208988800UL;

const double kMagicNtpFractionalUnit = 4.294967296E+9;

class RTC_EXPORT Clock {
 public:
  virtual ~Clock() {}

  virtual Timestamp CurrentTime() = 0;
  int64_t TimeInMilliseconds() { return CurrentTime().ms(); }
  int64_t TimeInMicroseconds() { return CurrentTime().us(); }

  NtpTime CurrentNtpTime() { return ConvertTimestampToNtpTime(CurrentTime()); }
  int64_t CurrentNtpInMilliseconds() { return CurrentNtpTime().ToMs(); }

  virtual NtpTime ConvertTimestampToNtpTime(Timestamp timestamp) = 0;
  int64_t ConvertTimestampToNtpTimeInMilliseconds(int64_t timestamp_ms) {
    return ConvertTimestampToNtpTime(Timestamp::Millis(timestamp_ms)).ToMs();
  }

  static Clock* GetRealTimeClock();
};

class SimulatedClock : public Clock {
 public:

  explicit SimulatedClock(int64_t initial_time_us);
  explicit SimulatedClock(Timestamp initial_time);
  ~SimulatedClock() override;

  Timestamp CurrentTime() override;

  NtpTime ConvertTimestampToNtpTime(Timestamp timestamp) override;

  void AdvanceTimeMilliseconds(int64_t milliseconds);
  void AdvanceTimeMicroseconds(int64_t microseconds);
  void AdvanceTime(TimeDelta delta);

 private:

  std::atomic<int64_t> time_us_;
};

}

#endif
