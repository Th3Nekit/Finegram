/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/win/scoped_com_initializer.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

ScopedCOMInitializer::ScopedCOMInitializer() {
  RTC_DLOG(LS_INFO) << "Single-Threaded Apartment (STA) COM thread";
  Initialize(COINIT_APARTMENTTHREADED);
}

ScopedCOMInitializer::ScopedCOMInitializer(SelectMTA mta) {
  RTC_DLOG(LS_INFO) << "Multi-Threaded Apartment (MTA) COM thread";
  Initialize(COINIT_MULTITHREADED);
}

ScopedCOMInitializer::~ScopedCOMInitializer() {
  if (Succeeded()) {
    CoUninitialize();
  }
}

void ScopedCOMInitializer::Initialize(COINIT init) {

  hr_ = CoInitializeEx(NULL, init);
  RTC_CHECK_NE(RPC_E_CHANGED_MODE, hr_)
      << "Invalid COM thread model change (MTA->STA)";

  if (hr_ == S_OK) {
    RTC_DLOG(LS_INFO)
        << "The COM library was initialized successfully on this thread";
  } else if (hr_ == S_FALSE) {
    RTC_DLOG(LS_WARNING)
        << "The COM library is already initialized on this thread";
  }
}

}
