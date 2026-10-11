/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_LOCATION_H_
#define API_LOCATION_H_

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT Location {
 public:
  static Location Current() { return Location(); }
};

}

#endif
