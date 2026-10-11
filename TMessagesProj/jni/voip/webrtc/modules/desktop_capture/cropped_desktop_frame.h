/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_CROPPED_DESKTOP_FRAME_H_
#define MODULES_DESKTOP_CAPTURE_CROPPED_DESKTOP_FRAME_H_

#include <memory>

#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

std::unique_ptr<DesktopFrame> RTC_EXPORT
CreateCroppedDesktopFrame(std::unique_ptr<DesktopFrame> frame,
                          const DesktopRect& rect);

}

#endif
