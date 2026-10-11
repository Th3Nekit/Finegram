/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_RGBA_COLOR_H_
#define MODULES_DESKTOP_CAPTURE_RGBA_COLOR_H_

#include <stdint.h>

#include "modules/desktop_capture/desktop_frame.h"

namespace webrtc {

struct RgbaColor final {

  RgbaColor(uint8_t blue, uint8_t green, uint8_t red, uint8_t alpha);

  RgbaColor(uint8_t blue, uint8_t green, uint8_t red);

  explicit RgbaColor(const uint8_t* bgra);

  explicit RgbaColor(uint32_t bgra);

  bool operator==(const RgbaColor& right) const;

  bool operator!=(const RgbaColor& right) const;

  uint32_t ToUInt32() const;

  uint8_t blue;
  uint8_t green;
  uint8_t red;
  uint8_t alpha;
};
static_assert(
    DesktopFrame::kBytesPerPixel == sizeof(RgbaColor),
    "A pixel in DesktopFrame should be safe to be represented by a RgbaColor");

}

#endif
