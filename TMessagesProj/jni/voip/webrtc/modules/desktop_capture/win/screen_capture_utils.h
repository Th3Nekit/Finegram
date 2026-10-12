/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURE_UTILS_H_
#define MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURE_UTILS_H_

#if defined(WEBRTC_WIN)

#define WEBRTC_DECLARE_HANDLE(name) \
  struct name##__;                  \
  typedef struct name##__* name
WEBRTC_DECLARE_HANDLE(HMONITOR);
#undef WEBRTC_DECLARE_HANDLE
#endif

#include <string>
#include <vector>

#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

bool HasActiveDisplay();

bool GetScreenList(DesktopCapturer::SourceList* screens,
                   std::vector<std::string>* device_names = nullptr);

bool GetHmonitorFromDeviceIndex(DesktopCapturer::SourceId device_index,
                                HMONITOR* hmonitor);

bool IsMonitorValid(HMONITOR monitor);

DesktopRect GetMonitorRect(HMONITOR monitor);

DesktopVector GetDpiForMonitor(HMONITOR monitor);

bool IsScreenValid(DesktopCapturer::SourceId screen, std::wstring* device_key);

DesktopRect GetFullscreenRect();

RTC_EXPORT DesktopRect GetScreenRect(DesktopCapturer::SourceId screen,
                                     const std::wstring& device_key);

}

#endif
