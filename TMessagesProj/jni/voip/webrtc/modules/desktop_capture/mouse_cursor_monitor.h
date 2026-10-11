/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MOUSE_CURSOR_MONITOR_H_
#define MODULES_DESKTOP_CAPTURE_MOUSE_CURSOR_MONITOR_H_

#include <memory>

#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class DesktopCaptureOptions;
class DesktopFrame;
class MouseCursor;

class MouseCursorMonitor {
 public:

  enum CursorState {

    INSIDE,

    OUTSIDE,
  };

  enum Mode {

    SHAPE_ONLY,

    SHAPE_AND_POSITION,
  };

  class Callback {
   public:

    virtual void OnMouseCursor(MouseCursor* cursor) = 0;

    virtual void OnMouseCursorPosition(CursorState state,
                                       const DesktopVector& position) {}

    virtual void OnMouseCursorPosition(const DesktopVector& position) {}

   protected:
    virtual ~Callback() {}
  };

  virtual ~MouseCursorMonitor() {}

  static MouseCursorMonitor* CreateForWindow(
      const DesktopCaptureOptions& options,
      WindowId window);

  static RTC_EXPORT MouseCursorMonitor* CreateForScreen(
      const DesktopCaptureOptions& options,
      ScreenId screen);

  static RTC_EXPORT std::unique_ptr<MouseCursorMonitor> Create(
      const DesktopCaptureOptions& options);

  virtual void Init(Callback* callback, Mode mode) = 0;

  virtual void Capture() = 0;
};

}

#endif
