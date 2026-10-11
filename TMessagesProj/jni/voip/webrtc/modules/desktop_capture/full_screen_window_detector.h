/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_FULL_SCREEN_WINDOW_DETECTOR_H_
#define MODULES_DESKTOP_CAPTURE_FULL_SCREEN_WINDOW_DETECTOR_H_

#include <memory>

#include "api/function_view.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/full_screen_application_handler.h"

namespace webrtc {

class FullScreenWindowDetector
    : public rtc::RefCountedNonVirtual<FullScreenWindowDetector> {
 public:
  using ApplicationHandlerFactory =
      std::function<std::unique_ptr<FullScreenApplicationHandler>(
          DesktopCapturer::SourceId sourceId)>;

  FullScreenWindowDetector(
      ApplicationHandlerFactory application_handler_factory);

  FullScreenWindowDetector(const FullScreenWindowDetector&) = delete;
  FullScreenWindowDetector& operator=(const FullScreenWindowDetector&) = delete;

  DesktopCapturer::SourceId FindFullScreenWindow(
      DesktopCapturer::SourceId original_source_id);

  void UpdateWindowListIfNeeded(
      DesktopCapturer::SourceId original_source_id,
      rtc::FunctionView<bool(DesktopCapturer::SourceList*)> get_sources);

  static rtc::scoped_refptr<FullScreenWindowDetector>
  CreateFullScreenWindowDetector();

 protected:
  std::unique_ptr<FullScreenApplicationHandler> app_handler_;

 private:
  void CreateApplicationHandlerIfNeeded(DesktopCapturer::SourceId source_id);

  ApplicationHandlerFactory application_handler_factory_;

  int64_t last_update_time_ms_;
  DesktopCapturer::SourceId previous_source_id_;

  DesktopCapturer::SourceId no_handler_source_id_;

  DesktopCapturer::SourceList window_list_;
};

}

#endif
