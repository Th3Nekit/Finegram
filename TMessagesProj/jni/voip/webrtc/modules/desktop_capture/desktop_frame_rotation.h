/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_DESKTOP_FRAME_ROTATION_H_
#define MODULES_DESKTOP_CAPTURE_DESKTOP_FRAME_ROTATION_H_

#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"

namespace webrtc {

enum class Rotation {
  CLOCK_WISE_0,
  CLOCK_WISE_90,
  CLOCK_WISE_180,
  CLOCK_WISE_270,
};

void RotateDesktopFrame(const DesktopFrame& source,
                        const DesktopRect& source_rect,
                        const Rotation& rotation,
                        const DesktopVector& target_offset,
                        DesktopFrame* target);

Rotation ReverseRotation(Rotation rotation);

DesktopSize RotateSize(DesktopSize size, Rotation rotation);

DesktopRect RotateRect(DesktopRect rect, DesktopSize size, Rotation rotation);

}

#endif
