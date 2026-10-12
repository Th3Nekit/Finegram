/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYNCHRONIZATION_MUTEX_CRITICAL_SECTION_H_
#define RTC_BASE_SYNCHRONIZATION_MUTEX_CRITICAL_SECTION_H_

#if defined(WEBRTC_WIN)

#include <winsock2.h>
#include <windows.h>
#include <sal.h>

#include "absl/base/attributes.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RTC_LOCKABLE MutexImpl final {
 public:
  MutexImpl() { InitializeCriticalSection(&critical_section_); }
  MutexImpl(const MutexImpl&) = delete;
  MutexImpl& operator=(const MutexImpl&) = delete;
  ~MutexImpl() { DeleteCriticalSection(&critical_section_); }

  void Lock() RTC_EXCLUSIVE_LOCK_FUNCTION() {
    EnterCriticalSection(&critical_section_);
  }
  ABSL_MUST_USE_RESULT bool TryLock() RTC_EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    return TryEnterCriticalSection(&critical_section_) != FALSE;
  }
  void AssertHeld() const RTC_ASSERT_EXCLUSIVE_LOCK() {}
  void Unlock() RTC_UNLOCK_FUNCTION() {
    LeaveCriticalSection(&critical_section_);
  }

 private:
  CRITICAL_SECTION critical_section_;
};

}

#endif
#endif
