/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_WIN)
#include <windows.h>
#else
#include <string.h>
#endif

#include "rtc_base/checks.h"
#include "rtc_base/zero_memory.h"

namespace rtc {

void ExplicitZeroMemory(void* ptr, size_t len) {
  RTC_DCHECK(ptr || !len);
#if defined(WEBRTC_WIN)
  SecureZeroMemory(ptr, len);
#else
  memset(ptr, 0, len);
#if !defined(__pnacl__)

  __asm__ __volatile__("" : : "r"(ptr) : "memory");
#endif
#endif
}

}
