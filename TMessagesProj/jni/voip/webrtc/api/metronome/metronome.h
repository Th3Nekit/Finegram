/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_METRONOME_METRONOME_H_
#define API_METRONOME_METRONOME_H_

#include "absl/functional/any_invocable.h"
#include "api/units/time_delta.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT Metronome {
 public:
  virtual ~Metronome() = default;

  virtual void RequestCallOnNextTick(absl::AnyInvocable<void() &&> callback) {}

  virtual TimeDelta TickPeriod() const = 0;
};

}

#endif
