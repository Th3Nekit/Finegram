/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MAC_DESKTOP_CONFIGURATION_H_
#define MODULES_DESKTOP_CAPTURE_MAC_DESKTOP_CONFIGURATION_H_

#include <ApplicationServices/ApplicationServices.h>

#include <vector>

#include "modules/desktop_capture/desktop_geometry.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct MacDisplayConfiguration {
  MacDisplayConfiguration();
  MacDisplayConfiguration(const MacDisplayConfiguration& other);
  MacDisplayConfiguration(MacDisplayConfiguration&& other);
  ~MacDisplayConfiguration();

  MacDisplayConfiguration& operator=(const MacDisplayConfiguration& other);
  MacDisplayConfiguration& operator=(MacDisplayConfiguration&& other);

  CGDirectDisplayID id = 0;

  DesktopRect bounds;

  DesktopRect pixel_bounds;

  float dip_to_pixel_scale = 1.0f;

  bool is_builtin;
};

typedef std::vector<MacDisplayConfiguration> MacDisplayConfigurations;

struct RTC_EXPORT MacDesktopConfiguration {

  enum Origin { BottomLeftOrigin, TopLeftOrigin };

  MacDesktopConfiguration();
  MacDesktopConfiguration(const MacDesktopConfiguration& other);
  MacDesktopConfiguration(MacDesktopConfiguration&& other);
  ~MacDesktopConfiguration();

  MacDesktopConfiguration& operator=(const MacDesktopConfiguration& other);
  MacDesktopConfiguration& operator=(MacDesktopConfiguration&& other);

  static MacDesktopConfiguration GetCurrent(Origin origin);

  bool Equals(const MacDesktopConfiguration& other);

  const MacDisplayConfiguration* FindDisplayConfigurationById(
      CGDirectDisplayID id);

  DesktopRect bounds;

  DesktopRect pixel_bounds;

  float dip_to_pixel_scale = 1.0f;

  MacDisplayConfigurations displays;
};

}

#endif
