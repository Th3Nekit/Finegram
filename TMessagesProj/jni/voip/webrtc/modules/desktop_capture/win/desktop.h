/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_DESKTOP_H_
#define MODULES_DESKTOP_CAPTURE_WIN_DESKTOP_H_

#include <windows.h>

#include <string>

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT Desktop {
 public:
  ~Desktop();

  Desktop(const Desktop&) = delete;
  Desktop& operator=(const Desktop&) = delete;

  bool GetName(std::wstring* desktop_name_out) const;

  bool IsSame(const Desktop& other) const;

  bool SetThreadDesktop() const;

  static Desktop* GetDesktop(const wchar_t* desktop_name);

  static Desktop* GetInputDesktop();

  static Desktop* GetThreadDesktop();

 private:
  Desktop(HDESK desktop, bool own);

  HDESK desktop_;

  bool own_;
};

}

#endif
