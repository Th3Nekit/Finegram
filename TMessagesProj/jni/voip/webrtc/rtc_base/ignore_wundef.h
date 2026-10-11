/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_IGNORE_WUNDEF_H_
#define RTC_BASE_IGNORE_WUNDEF_H_

#ifdef __clang__
#define RTC_PUSH_IGNORING_WUNDEF() \
  _Pragma("clang diagnostic push") \
      _Pragma("clang diagnostic ignored \"-Wundef\"")
#define RTC_POP_IGNORING_WUNDEF() _Pragma("clang diagnostic pop")
#else
#define RTC_PUSH_IGNORING_WUNDEF()
#define RTC_POP_IGNORING_WUNDEF()
#endif

#endif
