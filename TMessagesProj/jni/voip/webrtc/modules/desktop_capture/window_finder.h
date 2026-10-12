/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WINDOW_FINDER_H_
#define MODULES_DESKTOP_CAPTURE_WINDOW_FINDER_H_

#include <memory>

#include "api/scoped_refptr.h"
#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/desktop_geometry.h"

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include "modules/desktop_capture/mac/desktop_configuration_monitor.h"
#endif

namespace webrtc {

#if defined(WEBRTC_USE_X11)
class XAtomCache;
#endif

class WindowFinder {
 public:
  WindowFinder() = default;
  virtual ~WindowFinder() = default;

  virtual WindowId GetWindowUnderPoint(DesktopVector point) = 0;

  struct Options final {
    Options();
    ~Options();
    Options(const Options& other);
    Options(Options&& other);

#if defined(WEBRTC_USE_X11)
    XAtomCache* cache = nullptr;
#endif
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
    rtc::scoped_refptr<DesktopConfigurationMonitor> configuration_monitor;
#endif
  };

  static std::unique_ptr<WindowFinder> Create(const Options& options);
};

}

#endif
