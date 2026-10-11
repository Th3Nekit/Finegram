/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mac/desktop_configuration.h"

#include <math.h>
#include <algorithm>
#include <Cocoa/Cocoa.h>

#include "rtc_base/checks.h"

#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_7

@interface NSScreen (LionAPI)
- (CGFloat)backingScaleFactor;
- (NSRect)convertRectToBacking:(NSRect)aRect;
@end

#endif

namespace webrtc {

namespace {

DesktopRect NSRectToDesktopRect(const NSRect& ns_rect) {
  return DesktopRect::MakeLTRB(
      static_cast<int>(floor(ns_rect.origin.x)),
      static_cast<int>(floor(ns_rect.origin.y)),
      static_cast<int>(ceil(ns_rect.origin.x + ns_rect.size.width)),
      static_cast<int>(ceil(ns_rect.origin.y + ns_rect.size.height)));
}

void InvertRectYOrigin(const DesktopRect& bounds,
                       DesktopRect* rect) {
  RTC_DCHECK_EQ(bounds.top(), 0);
  *rect = DesktopRect::MakeXYWH(
      rect->left(), bounds.bottom() - rect->bottom(),
      rect->width(), rect->height());
}

MacDisplayConfiguration GetConfigurationForScreen(NSScreen* screen) {
  MacDisplayConfiguration display_config;

  NSDictionary* device_description = [screen deviceDescription];
  display_config.id = static_cast<CGDirectDisplayID>(
      [[device_description objectForKey:@"NSScreenNumber"] intValue]);

  NSRect ns_bounds = [screen frame];
  display_config.bounds = NSRectToDesktopRect(ns_bounds);

  if ([screen respondsToSelector:@selector(backingScaleFactor)] &&
      [screen respondsToSelector:@selector(convertRectToBacking:)]) {
    display_config.dip_to_pixel_scale = [screen backingScaleFactor];
    NSRect ns_pixel_bounds = [screen convertRectToBacking: ns_bounds];
    display_config.pixel_bounds = NSRectToDesktopRect(ns_pixel_bounds);
  } else {
    display_config.pixel_bounds = display_config.bounds;
  }

  display_config.is_builtin = CGDisplayIsBuiltin(display_config.id);

  return display_config;
}

}

MacDisplayConfiguration::MacDisplayConfiguration() = default;
MacDisplayConfiguration::MacDisplayConfiguration(
    const MacDisplayConfiguration& other) = default;
MacDisplayConfiguration::MacDisplayConfiguration(
    MacDisplayConfiguration&& other) = default;
MacDisplayConfiguration::~MacDisplayConfiguration() = default;

MacDisplayConfiguration& MacDisplayConfiguration::operator=(
    const MacDisplayConfiguration& other) = default;
MacDisplayConfiguration& MacDisplayConfiguration::operator=(
    MacDisplayConfiguration&& other) = default;

MacDesktopConfiguration::MacDesktopConfiguration() = default;
MacDesktopConfiguration::MacDesktopConfiguration(
    const MacDesktopConfiguration& other) = default;
MacDesktopConfiguration::MacDesktopConfiguration(
    MacDesktopConfiguration&& other) = default;
MacDesktopConfiguration::~MacDesktopConfiguration() = default;

MacDesktopConfiguration& MacDesktopConfiguration::operator=(
    const MacDesktopConfiguration& other) = default;
MacDesktopConfiguration& MacDesktopConfiguration::operator=(
    MacDesktopConfiguration&& other) = default;

MacDesktopConfiguration MacDesktopConfiguration::GetCurrent(Origin origin) {
  MacDesktopConfiguration desktop_config;

  NSArray* screens = [NSScreen screens];
  RTC_DCHECK(screens);

  for (NSUInteger i = 0; i < [screens count]; ++i) {
    MacDisplayConfiguration display_config =
        GetConfigurationForScreen([screens objectAtIndex: i]);

    if (i == 0)
      desktop_config.dip_to_pixel_scale = display_config.dip_to_pixel_scale;

    if (i > 0 && origin == TopLeftOrigin) {
      InvertRectYOrigin(desktop_config.displays[0].bounds,
                        &display_config.bounds);

      float scaling_factor = display_config.dip_to_pixel_scale /
          desktop_config.displays[0].dip_to_pixel_scale;
      DesktopRect primary_bounds = DesktopRect::MakeLTRB(
          desktop_config.displays[0].pixel_bounds.left() * scaling_factor,
          desktop_config.displays[0].pixel_bounds.top() * scaling_factor,
          desktop_config.displays[0].pixel_bounds.right() * scaling_factor,
          desktop_config.displays[0].pixel_bounds.bottom() * scaling_factor);
      InvertRectYOrigin(primary_bounds, &display_config.pixel_bounds);
    }

    desktop_config.displays.push_back(display_config);

    if (display_config.dip_to_pixel_scale ==
        desktop_config.dip_to_pixel_scale) {
      desktop_config.bounds.UnionWith(display_config.bounds);
      desktop_config.pixel_bounds.UnionWith(display_config.pixel_bounds);
    }
  }

  return desktop_config;
}

bool operator==(const MacDisplayConfiguration& left,
                const MacDisplayConfiguration& right) {
  return left.id == right.id &&
      left.bounds.equals(right.bounds) &&
      left.pixel_bounds.equals(right.pixel_bounds) &&
      left.dip_to_pixel_scale == right.dip_to_pixel_scale;
}

bool MacDesktopConfiguration::Equals(const MacDesktopConfiguration& other) {
  return bounds.equals(other.bounds) &&
      pixel_bounds.equals(other.pixel_bounds) &&
      dip_to_pixel_scale == other.dip_to_pixel_scale &&
      displays == other.displays;
}

const MacDisplayConfiguration*
MacDesktopConfiguration::FindDisplayConfigurationById(
    CGDirectDisplayID id) {
  bool is_builtin = CGDisplayIsBuiltin(id);
  for (MacDisplayConfigurations::const_iterator it = displays.begin();
      it != displays.end(); ++it) {

    if ((is_builtin && it->is_builtin) || (!is_builtin && it->id == id)) return &(*it);
  }
  return NULL;
}

}
