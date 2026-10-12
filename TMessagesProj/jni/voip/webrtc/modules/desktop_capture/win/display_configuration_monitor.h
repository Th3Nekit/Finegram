/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_DISPLAY_CONFIGURATION_MONITOR_H_
#define MODULES_DESKTOP_CAPTURE_WIN_DISPLAY_CONFIGURATION_MONITOR_H_

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "rtc_base/containers/flat_map.h"

namespace webrtc {

class DisplayConfigurationMonitor {
 public:

  bool IsChanged(DesktopCapturer::SourceId source_id);

  void Reset();

 private:
  DesktopVector GetDpiForSourceId(DesktopCapturer::SourceId source_id);

  DesktopRect rect_;

  flat_map<DesktopCapturer::SourceId, DesktopVector> source_dpis_;

  bool initialized_ = false;
};

}

#endif
