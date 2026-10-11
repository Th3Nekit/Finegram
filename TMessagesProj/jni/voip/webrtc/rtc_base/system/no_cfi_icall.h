/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYSTEM_NO_CFI_ICALL_H_
#define RTC_BASE_SYSTEM_NO_CFI_ICALL_H_

#include "rtc_base/sanitizer.h"

#if !defined(WEBRTC_CHROMIUM_BUILD)
#if !defined(DISABLE_CFI_ICALL)
#if defined(WEBRTC_WIN)

#define DISABLE_CFI_ICALL RTC_NO_SANITIZE("cfi-icall") __declspec(guard(nocf))
#else
#define DISABLE_CFI_ICALL RTC_NO_SANITIZE("cfi-icall")
#endif
#endif
#if !defined(DISABLE_CFI_ICALL)
#define DISABLE_CFI_ICALL
#endif
#endif

#endif
