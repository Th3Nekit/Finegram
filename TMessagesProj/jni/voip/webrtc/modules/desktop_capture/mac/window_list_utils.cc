/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mac/window_list_utils.h"

#include <ApplicationServices/ApplicationServices.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <utility>

#include "rtc_base/checks.h"

static_assert(static_cast<webrtc::WindowId>(kCGNullWindowID) ==
                  webrtc::kNullWindowId,
              "kNullWindowId needs to equal to kCGNullWindowID.");

namespace webrtc {

namespace {

const CFStringRef kStatusIndicator = CFSTR("StatusIndicator");
const CFStringRef kStatusIndicatorOwnerName = CFSTR("Window Server");

bool ToUtf8(const CFStringRef str16, std::string* str8) {
  size_t maxlen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str16),
                                                    kCFStringEncodingUTF8) +
                  1;
  std::unique_ptr<char[]> buffer(new char[maxlen]);
  if (!buffer ||
      !CFStringGetCString(str16, buffer.get(), maxlen, kCFStringEncodingUTF8)) {
    return false;
  }
  str8->assign(buffer.get());
  return true;
}

bool GetWindowRef(CGWindowID id,
                  rtc::FunctionView<void(CFDictionaryRef)> on_window) {
  RTC_DCHECK(on_window);

  CFArrayRef window_id_array =
      CFArrayCreate(NULL, reinterpret_cast<const void**>(&id), 1, NULL);
  CFArrayRef window_array =
      CGWindowListCreateDescriptionFromArray(window_id_array);

  bool result = false;

  if (window_array && CFArrayGetCount(window_array)) {
    on_window(reinterpret_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(window_array, 0)));
    result = true;
  }

  if (window_array) {
    CFRelease(window_array);
  }
  CFRelease(window_id_array);
  return result;
}

}

bool GetWindowList(rtc::FunctionView<bool(CFDictionaryRef)> on_window,
                   bool ignore_minimized,
                   bool only_zero_layer) {
  RTC_DCHECK(on_window);

  CFArrayRef window_array = CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
      kCGNullWindowID);
  if (!window_array)
    return false;

  MacDesktopConfiguration desktop_config = MacDesktopConfiguration::GetCurrent(
      MacDesktopConfiguration::TopLeftOrigin);

  CFIndex count = CFArrayGetCount(window_array);
  for (CFIndex i = 0; i < count; i++) {
    CFDictionaryRef window = reinterpret_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(window_array, i));
    if (!window) {
      continue;
    }

    CFNumberRef window_id = reinterpret_cast<CFNumberRef>(
        CFDictionaryGetValue(window, kCGWindowNumber));
    if (!window_id) {
      continue;
    }

    CFNumberRef window_layer = reinterpret_cast<CFNumberRef>(
        CFDictionaryGetValue(window, kCGWindowLayer));
    if (!window_layer) {
      continue;
    }

    int layer;
    if (!CFNumberGetValue(window_layer, kCFNumberIntType, &layer)) {
      continue;
    }
    if (only_zero_layer && layer != 0) {
      continue;
    }

    if (ignore_minimized && !IsWindowOnScreen(window) &&
        !IsWindowFullScreen(desktop_config, window)) {
      continue;
    }

    CFStringRef window_title = reinterpret_cast<CFStringRef>(
        CFDictionaryGetValue(window, kCGWindowName));
    if (!window_title && !IsWindowOnScreen(window) &&
        !IsWindowFullScreen(desktop_config, window)) {
      continue;
    }

    CFStringRef window_owner_name = reinterpret_cast<CFStringRef>(
        CFDictionaryGetValue(window, kCGWindowOwnerName));

    if (window_title && CFEqual(window_title, kStatusIndicator) &&
        window_owner_name &&
        CFEqual(window_owner_name, kStatusIndicatorOwnerName)) {
      continue;
    }

    if (!on_window(window)) {
      break;
    }
  }

  CFRelease(window_array);
  return true;
}

bool GetWindowList(DesktopCapturer::SourceList* windows,
                   bool ignore_minimized,
                   bool only_zero_layer) {

  std::list<DesktopCapturer::Source> sources;
  std::map<int, std::list<DesktopCapturer::Source>::const_iterator> pid_itr_map;
  const bool ret = GetWindowList(
      [&sources, &pid_itr_map](CFDictionaryRef window) {
        WindowId window_id = GetWindowId(window);
        if (window_id != kNullWindowId) {
          const std::string title = GetWindowTitle(window);
          const int pid = GetWindowOwnerPid(window);

          std::map<int,
                   std::list<DesktopCapturer::Source>::const_iterator>::iterator
              itr = pid_itr_map.find(pid);

          if (title.empty()) {
            std::string owner_name = GetWindowOwnerName(window);

            if (!owner_name.empty() && (itr == pid_itr_map.end())) {
              sources.push_back(DesktopCapturer::Source{window_id, owner_name});
              RTC_DCHECK(!sources.empty());

              std::list<DesktopCapturer::Source>::const_iterator last_source =
                  --sources.end();
              pid_itr_map.insert(
                  std::pair<int,
                            std::list<DesktopCapturer::Source>::const_iterator>(
                      pid, last_source));
            }
          } else {
            sources.push_back(DesktopCapturer::Source{window_id, title});

            if (itr != pid_itr_map.end() && (itr->second != sources.end())) {
              sources.erase(itr->second);

              itr->second = sources.end();
            }
          }
        }
        return true;
      },
      ignore_minimized, only_zero_layer);

  if (!ret)
    return false;

  RTC_DCHECK(windows);
  windows->reserve(windows->size() + sources.size());
  std::copy(std::begin(sources), std::end(sources),
            std::back_inserter(*windows));

  return true;
}

bool IsWindowFullScreen(const MacDesktopConfiguration& desktop_config,
                        CFDictionaryRef window) {
  bool fullscreen = false;
  CFDictionaryRef bounds_ref = reinterpret_cast<CFDictionaryRef>(
      CFDictionaryGetValue(window, kCGWindowBounds));

  CGRect bounds;
  if (bounds_ref &&
      CGRectMakeWithDictionaryRepresentation(bounds_ref, &bounds)) {
    for (MacDisplayConfigurations::const_iterator it =
             desktop_config.displays.begin();
         it != desktop_config.displays.end(); it++) {
      if (it->bounds.equals(
              DesktopRect::MakeXYWH(bounds.origin.x, bounds.origin.y,
                                    bounds.size.width, bounds.size.height))) {
        fullscreen = true;
        break;
      }
    }
  }

  return fullscreen;
}

bool IsWindowFullScreen(const MacDesktopConfiguration& desktop_config,
                        CGWindowID id) {
  bool fullscreen = false;
  GetWindowRef(id, [&](CFDictionaryRef window) {
    fullscreen = IsWindowFullScreen(desktop_config, window);
  });
  return fullscreen;
}

bool IsWindowOnScreen(CFDictionaryRef window) {
  CFBooleanRef on_screen = reinterpret_cast<CFBooleanRef>(
      CFDictionaryGetValue(window, kCGWindowIsOnscreen));
  return on_screen != NULL && CFBooleanGetValue(on_screen);
}

bool IsWindowOnScreen(CGWindowID id) {
  bool on_screen;
  if (GetWindowRef(id, [&on_screen](CFDictionaryRef window) {
        on_screen = IsWindowOnScreen(window);
      })) {
    return on_screen;
  }
  return false;
}

std::string GetWindowTitle(CFDictionaryRef window) {
  CFStringRef title = reinterpret_cast<CFStringRef>(
      CFDictionaryGetValue(window, kCGWindowName));
  std::string result;
  if (title && ToUtf8(title, &result)) {
    return result;
  }

  return std::string();
}

std::string GetWindowTitle(CGWindowID id) {
  std::string title;
  if (GetWindowRef(id, [&title](CFDictionaryRef window) {
        title = GetWindowTitle(window);
      })) {
    return title;
  }
  return std::string();
}

std::string GetWindowOwnerName(CFDictionaryRef window) {
  CFStringRef owner_name = reinterpret_cast<CFStringRef>(
      CFDictionaryGetValue(window, kCGWindowOwnerName));
  std::string result;
  if (owner_name && ToUtf8(owner_name, &result)) {
    return result;
  }
  return std::string();
}

std::string GetWindowOwnerName(CGWindowID id) {
  std::string owner_name;
  if (GetWindowRef(id, [&owner_name](CFDictionaryRef window) {
        owner_name = GetWindowOwnerName(window);
      })) {
    return owner_name;
  }
  return std::string();
}

WindowId GetWindowId(CFDictionaryRef window) {
  CFNumberRef window_id = reinterpret_cast<CFNumberRef>(
      CFDictionaryGetValue(window, kCGWindowNumber));
  if (!window_id) {
    return kNullWindowId;
  }

  CGWindowID id;
  if (!CFNumberGetValue(window_id, kCFNumberIntType, &id)) {
    return kNullWindowId;
  }

  return id;
}

int GetWindowOwnerPid(CFDictionaryRef window) {
  CFNumberRef window_pid = reinterpret_cast<CFNumberRef>(
      CFDictionaryGetValue(window, kCGWindowOwnerPID));
  if (!window_pid) {
    return 0;
  }

  int pid;
  if (!CFNumberGetValue(window_pid, kCFNumberIntType, &pid)) {
    return 0;
  }

  return pid;
}

int GetWindowOwnerPid(CGWindowID id) {
  int pid;
  if (GetWindowRef(id, [&pid](CFDictionaryRef window) {
        pid = GetWindowOwnerPid(window);
      })) {
    return pid;
  }
  return 0;
}

float GetScaleFactorAtPosition(const MacDesktopConfiguration& desktop_config,
                               DesktopVector position) {

  for (auto it = desktop_config.displays.begin();
       it != desktop_config.displays.end(); ++it) {
    if (it->bounds.Contains(position)) {
      return it->dip_to_pixel_scale;
    }
  }
  return 1;
}

float GetWindowScaleFactor(CGWindowID id, DesktopSize size) {
  DesktopRect window_bounds = GetWindowBounds(id);
  float scale = 1.0f;

  if (!window_bounds.is_empty() && !size.is_empty()) {
    float scale_x = size.width() / window_bounds.width();
    float scale_y = size.height() / window_bounds.height();

    if ((std::fabs(scale_x - scale_y) <=
         std::numeric_limits<float>::epsilon() * std::max(scale_x, scale_y)) &&
        scale_x > 0.0f) {
      scale = scale_x;
    }
  }

  return scale;
}

DesktopRect GetWindowBounds(CFDictionaryRef window) {
  CFDictionaryRef window_bounds = reinterpret_cast<CFDictionaryRef>(
      CFDictionaryGetValue(window, kCGWindowBounds));
  if (!window_bounds) {
    return DesktopRect();
  }

  CGRect gc_window_rect;
  if (!CGRectMakeWithDictionaryRepresentation(window_bounds, &gc_window_rect)) {
    return DesktopRect();
  }

  return DesktopRect::MakeXYWH(gc_window_rect.origin.x, gc_window_rect.origin.y,
                               gc_window_rect.size.width,
                               gc_window_rect.size.height);
}

DesktopRect GetWindowBounds(CGWindowID id) {
  DesktopRect result;
  if (GetWindowRef(id, [&result](CFDictionaryRef window) {
        result = GetWindowBounds(window);
      })) {
    return result;
  }
  return DesktopRect();
}

}
