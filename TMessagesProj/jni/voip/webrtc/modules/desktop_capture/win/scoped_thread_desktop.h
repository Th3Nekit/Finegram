/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_SCOPED_THREAD_DESKTOP_H_
#define MODULES_DESKTOP_CAPTURE_WIN_SCOPED_THREAD_DESKTOP_H_

#include <windows.h>

#include <memory>

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class Desktop;

class RTC_EXPORT ScopedThreadDesktop {
 public:
  ScopedThreadDesktop();
  ~ScopedThreadDesktop();

  ScopedThreadDesktop(const ScopedThreadDesktop&) = delete;
  ScopedThreadDesktop& operator=(const ScopedThreadDesktop&) = delete;

  bool IsSame(const Desktop& desktop);

  void Revert();

  bool SetThreadDesktop(Desktop* desktop);

 private:

  std::unique_ptr<Desktop> assigned_;

  std::unique_ptr<Desktop> initial_;
};

}

#endif
