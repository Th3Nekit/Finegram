/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_EVENT_H_
#define RTC_BASE_EVENT_H_

#include "api/units/time_delta.h"

#if defined(WEBRTC_WIN)
#include <windows.h>
#elif defined(WEBRTC_POSIX)
#include <pthread.h>
#else
#error "Must define either WEBRTC_WIN or WEBRTC_POSIX."
#endif

#include "rtc_base/synchronization/yield_policy.h"

namespace rtc {

#if RTC_DCHECK_IS_ON
#define RTC_DISALLOW_WAIT() ScopedDisallowWait disallow_wait_##__LINE__
#else
#define RTC_DISALLOW_WAIT()
#endif

class Event {
 public:

  static constexpr webrtc::TimeDelta kForever =
      webrtc::TimeDelta::PlusInfinity();

  Event();
  Event(bool manual_reset, bool initially_signaled);
  Event(const Event&) = delete;
  Event& operator=(const Event&) = delete;
  ~Event();

  void Set();
  void Reset();

  bool Wait(webrtc::TimeDelta give_up_after, webrtc::TimeDelta warn_after);

  bool Wait(webrtc::TimeDelta give_up_after) {
    return Wait(give_up_after, give_up_after.IsPlusInfinity()
                                   ? webrtc::TimeDelta::Seconds(3)
                                   : kForever);
  }

 private:
#if defined(WEBRTC_WIN)
  HANDLE event_handle_;
#elif defined(WEBRTC_POSIX)
  pthread_mutex_t event_mutex_;
  pthread_cond_t event_cond_;
  const bool is_manual_reset_;
  bool event_status_;
#endif
};

class ScopedAllowBaseSyncPrimitives {
 public:
  ScopedAllowBaseSyncPrimitives() {}
  ~ScopedAllowBaseSyncPrimitives() {}
};

class ScopedAllowBaseSyncPrimitivesForTesting {
 public:
  ScopedAllowBaseSyncPrimitivesForTesting() {}
  ~ScopedAllowBaseSyncPrimitivesForTesting() {}
};

#if RTC_DCHECK_IS_ON
class ScopedDisallowWait {
 public:
  ScopedDisallowWait() = default;

 private:
  class DisallowYieldHandler : public YieldInterface {
   public:
    void YieldExecution() override { RTC_DCHECK_NOTREACHED(); }
  } handler_;
  rtc::ScopedYieldPolicy policy{&handler_};
};
#endif

}

#endif
