/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_SCREEN_DRAWER_H_
#define MODULES_DESKTOP_CAPTURE_SCREEN_DRAWER_H_

#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/rgba_color.h"

namespace webrtc {

class ScreenDrawerLock {
 public:
  virtual ~ScreenDrawerLock();

  static std::unique_ptr<ScreenDrawerLock> Create();

 protected:
  ScreenDrawerLock();
};

class ScreenDrawer {
 public:

  static std::unique_ptr<ScreenDrawer> Create();

  ScreenDrawer();
  virtual ~ScreenDrawer();

  virtual DesktopRect DrawableRegion() = 0;

  virtual void DrawRectangle(DesktopRect rect, RgbaColor color) = 0;

  virtual void Clear() = 0;

  virtual void WaitForPendingDraws() = 0;

  virtual bool MayDrawIncompleteShapes() = 0;

  virtual WindowId window_id() const = 0;
};

}

#endif
