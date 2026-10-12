/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/race_checker.h"

namespace rtc {

RaceChecker::RaceChecker() {}

bool RaceChecker::Acquire() const {
  const PlatformThreadRef current_thread = CurrentThreadRef();

  const int current_access_count = access_count_;
  access_count_ = access_count_ + 1;
  if (current_access_count == 0)
    accessing_thread_ = current_thread;

  const PlatformThreadRef accessing_thread = accessing_thread_;
  return IsThreadRefEqual(accessing_thread, current_thread);
}

void RaceChecker::Release() const {
  access_count_ = access_count_ - 1;
}

namespace internal {
RaceCheckerScope::RaceCheckerScope(const RaceChecker* race_checker)
    : race_checker_(race_checker), race_check_ok_(race_checker->Acquire()) {}

bool RaceCheckerScope::RaceDetected() const {
  return !race_check_ok_;
}

RaceCheckerScope::~RaceCheckerScope() {
  race_checker_->Release();
}

}
}
