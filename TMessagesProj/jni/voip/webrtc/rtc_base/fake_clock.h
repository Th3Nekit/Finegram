/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_FAKE_CLOCK_H_
#define RTC_BASE_FAKE_CLOCK_H_

#include <stdint.h>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"

namespace rtc {

class FakeClock : public ClockInterface {
 public:
  FakeClock() = default;
  FakeClock(const FakeClock&) = delete;
  FakeClock& operator=(const FakeClock&) = delete;
  ~FakeClock() override = default;

  int64_t TimeNanos() const override;

  void SetTime(webrtc::Timestamp new_time);

  void AdvanceTime(webrtc::TimeDelta delta);

 private:
  mutable webrtc::Mutex lock_;
  int64_t time_ns_ RTC_GUARDED_BY(lock_) = 0;
};

class ThreadProcessingFakeClock : public ClockInterface {
 public:
  int64_t TimeNanos() const override { return clock_.TimeNanos(); }
  void SetTime(webrtc::Timestamp time);
  void AdvanceTime(webrtc::TimeDelta delta);

 private:
  FakeClock clock_;
};

class ScopedBaseFakeClock : public FakeClock {
 public:
  ScopedBaseFakeClock();
  ~ScopedBaseFakeClock() override;

 private:
  ClockInterface* prev_clock_;
};

class ScopedFakeClock : public ThreadProcessingFakeClock {
 public:
  ScopedFakeClock();
  ~ScopedFakeClock() override;

 private:
  ClockInterface* prev_clock_;
};

}

#endif
