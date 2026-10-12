/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_H_
#define MODULES_DESKTOP_CAPTURE_MAC_SCREEN_CAPTURER_MAC_H_

#include <CoreGraphics/CoreGraphics.h>

#include <memory>
#include <vector>

#include "api/sequence_checker.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/mac/desktop_configuration.h"
#include "modules/desktop_capture/mac/desktop_configuration_monitor.h"
#include "modules/desktop_capture/mac/desktop_frame_provider.h"
#include "modules/desktop_capture/screen_capture_frame_queue.h"
#include "modules/desktop_capture/screen_capturer_helper.h"
#include "modules/desktop_capture/shared_desktop_frame.h"

namespace webrtc {

class DisplayStreamManager;

class ScreenCapturerMac final : public DesktopCapturer {
 public:
  ScreenCapturerMac(rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor,
                    bool detect_updated_region,
                    bool allow_iosurface);
  ~ScreenCapturerMac() override;

  ScreenCapturerMac(const ScreenCapturerMac&) = delete;
  ScreenCapturerMac& operator=(const ScreenCapturerMac&) = delete;

  bool Init();

  void Start(Callback* callback) override;
  void CaptureFrame() override;
  void SetExcludedWindow(WindowId window) override;
  bool GetSourceList(SourceList* screens) override;
  bool SelectSource(SourceId id) override;

 private:

  bool CgBlit(const DesktopFrame& frame, const DesktopRegion& region);

  void ScreenConfigurationChanged();

  bool RegisterRefreshAndMoveHandlers();
  void UnregisterRefreshAndMoveHandlers();

  void ScreenRefresh(CGDirectDisplayID display_id,
                     CGRectCount count,
                     const CGRect* rect_array,
                     DesktopVector display_origin,
                     IOSurfaceRef io_surface);
  void ReleaseBuffers();

  std::unique_ptr<DesktopFrame> CreateFrame();

  const bool detect_updated_region_;

  Callback* callback_ = nullptr;

  ScreenCaptureFrameQueue<SharedDesktopFrame> queue_;

  MacDesktopConfiguration desktop_config_;

  CGDirectDisplayID current_display_ = 0;

  DesktopRect screen_pixel_bounds_;

  float dip_to_pixel_scale_ = 1.0f;

  ScreenCapturerHelper helper_;

  DesktopRegion last_invalid_region_;

  rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor_;

  CGWindowID excluded_window_ = 0;

  std::vector<CGDisplayStreamRef> display_streams_;

  DesktopFrameProvider desktop_frame_provider_;

  SequenceChecker thread_checker_;
};

}

#endif
