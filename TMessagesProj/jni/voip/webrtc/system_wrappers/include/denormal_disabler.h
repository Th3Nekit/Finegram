/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SYSTEM_WRAPPERS_INCLUDE_DENORMAL_DISABLER_H_
#define SYSTEM_WRAPPERS_INCLUDE_DENORMAL_DISABLER_H_

#include "rtc_base/system/arch.h"

namespace webrtc {

class DenormalDisabler {
 public:

  DenormalDisabler();

  explicit DenormalDisabler(bool enabled);
  DenormalDisabler(const DenormalDisabler&) = delete;
  DenormalDisabler& operator=(const DenormalDisabler&) = delete;

  ~DenormalDisabler();

  static bool IsSupported();

 private:
  const int status_word_;
  const bool disabling_activated_;
};

}

#endif
