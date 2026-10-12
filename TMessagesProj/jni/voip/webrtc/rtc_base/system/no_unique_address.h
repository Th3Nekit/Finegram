/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYSTEM_NO_UNIQUE_ADDRESS_H_
#define RTC_BASE_SYSTEM_NO_UNIQUE_ADDRESS_H_

#ifndef __has_cpp_attribute
#define __has_cpp_attribute(x) 0
#endif
#if __has_cpp_attribute(no_unique_address)

#define RTC_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define RTC_NO_UNIQUE_ADDRESS
#endif

#endif
