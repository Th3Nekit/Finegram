/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_X11_WINDOW_LIST_UTILS_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_X11_WINDOW_LIST_UTILS_H_

#include <X11/X.h>
#include <X11/Xlib.h>
#include <stdint.h>

#include "api/function_view.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/linux/x11/x_atom_cache.h"

namespace webrtc {

bool GetWindowList(XAtomCache* cache,
                   rtc::FunctionView<bool(::Window)> on_window);

int32_t GetWindowState(XAtomCache* cache, ::Window window);

bool GetWindowRect(::Display* display,
                   ::Window window,
                   DesktopRect* rect,
                   XWindowAttributes* attributes = nullptr);

template <typename T>
DesktopRect DesktopRectFromXAttributes(const T& attributes) {
  return DesktopRect::MakeXYWH(attributes.x, attributes.y, attributes.width,
                               attributes.height);
}

}

#endif
