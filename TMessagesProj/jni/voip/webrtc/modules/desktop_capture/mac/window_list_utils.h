/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MAC_WINDOW_LIST_UTILS_H_
#define MODULES_DESKTOP_CAPTURE_MAC_WINDOW_LIST_UTILS_H_

#include <ApplicationServices/ApplicationServices.h>

#include <string>

#include "api/function_view.h"
#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/mac/desktop_configuration.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

bool RTC_EXPORT
GetWindowList(rtc::FunctionView<bool(CFDictionaryRef)> on_window,
              bool ignore_minimized,
              bool only_zero_layer);

bool RTC_EXPORT GetWindowList(DesktopCapturer::SourceList* windows,
                              bool ignore_minimized,
                              bool only_zero_layer);

bool IsWindowFullScreen(const MacDesktopConfiguration& desktop_config,
                        CFDictionaryRef window);

bool IsWindowFullScreen(const MacDesktopConfiguration& desktop_config,
                        CGWindowID id);

bool IsWindowOnScreen(CFDictionaryRef window);

bool IsWindowOnScreen(CGWindowID id);

std::string GetWindowTitle(CFDictionaryRef window);

std::string GetWindowTitle(CGWindowID id);

std::string GetWindowOwnerName(CFDictionaryRef window);

std::string GetWindowOwnerName(CGWindowID id);

WindowId GetWindowId(CFDictionaryRef window);

int GetWindowOwnerPid(CFDictionaryRef window);

int GetWindowOwnerPid(CGWindowID id);

float GetScaleFactorAtPosition(const MacDesktopConfiguration& desktop_config,
                               DesktopVector position);

float GetWindowScaleFactor(CGWindowID id, DesktopSize size);

DesktopRect GetWindowBounds(CFDictionaryRef window);

DesktopRect GetWindowBounds(CGWindowID id);

}

#endif
