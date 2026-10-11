/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_DEPRECATED_RECURSIVE_CRITICAL_SECTION_H_
#define RTC_BASE_DEPRECATED_RECURSIVE_CRITICAL_SECTION_H_

#include <atomic>

#include "rtc_base/platform_thread_types.h"
#include "rtc_base/thread_annotations.h"

#if defined(WEBRTC_WIN)

#include <winsock2.h>
#include <windows.h>
#include <sal.h>

#endif

#if defined(WEBRTC_POSIX)
#include <pthread.h>
#endif

#define RTC_USE_NATIVE_MUTEX_ON_MAC 1

#if defined(WEBRTC_MAC) && !RTC_USE_NATIVE_MUTEX_ON_MAC
#include <dispatch/dispatch.h>
#endif

namespace rtc {

class RTC_LOCKABLE RecursiveCriticalSection {
 public:
  RecursiveCriticalSection();
  ~RecursiveCriticalSection();

  void Enter() const RTC_EXCLUSIVE_LOCK_FUNCTION();
  bool TryEnter() const RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true);
  void Leave() const RTC_UNLOCK_FUNCTION();

 private:

  bool CurrentThreadIsOwner() const;

#if defined(WEBRTC_WIN)
  mutable CRITICAL_SECTION crit_;
#elif defined(WEBRTC_POSIX)
#if defined(WEBRTC_MAC) && !RTC_USE_NATIVE_MUTEX_ON_MAC

  mutable std::atomic<int> lock_queue_;

  mutable int recursion_;

  mutable dispatch_semaphore_t semaphore_;

  mutable PlatformThreadRef owning_thread_;
#else
  mutable pthread_mutex_t mutex_;
#endif
  mutable PlatformThreadRef thread_;
  mutable int recursion_count_;
#else
#error Unsupported platform.
#endif
};

class RTC_SCOPED_LOCKABLE CritScope {
 public:
  explicit CritScope(const RecursiveCriticalSection* cs)
      RTC_EXCLUSIVE_LOCK_FUNCTION(cs);
  ~CritScope() RTC_UNLOCK_FUNCTION();

  CritScope(const CritScope&) = delete;
  CritScope& operator=(const CritScope&) = delete;

 private:
  const RecursiveCriticalSection* const cs_;
};

}

#endif
