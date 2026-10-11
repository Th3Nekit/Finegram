/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_X11_SCREEN_CAPTURER_X11_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_X11_SCREEN_CAPTURER_X11_H_

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrandr.h>

#include <memory>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/linux/x11/shared_x_display.h"
#include "modules/desktop_capture/linux/x11/x_atom_cache.h"
#include "modules/desktop_capture/linux/x11/x_server_pixel_buffer.h"
#include "modules/desktop_capture/screen_capture_frame_queue.h"
#include "modules/desktop_capture/screen_capturer_helper.h"
#include "modules/desktop_capture/shared_desktop_frame.h"

namespace webrtc {

class ScreenCapturerX11 : public DesktopCapturer,
                          public SharedXDisplay::XEventHandler {
 public:
  ScreenCapturerX11();
  ~ScreenCapturerX11() override;

  ScreenCapturerX11(const ScreenCapturerX11&) = delete;
  ScreenCapturerX11& operator=(const ScreenCapturerX11&) = delete;

  static std::unique_ptr<DesktopCapturer> CreateRawScreenCapturer(
      const DesktopCaptureOptions& options);

  bool Init(const DesktopCaptureOptions& options);

  void Start(Callback* delegate) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

 private:
  Display* display() { return options_.x_display()->display(); }

  bool HandleXEvent(const XEvent& event) override;

  void InitXDamage();
  void InitXrandr();
  void UpdateMonitors();

  std::unique_ptr<DesktopFrame> CaptureScreen();

  void ScreenConfigurationChanged();

  void SynchronizeFrame();

  void DeinitXlib();

  DesktopCaptureOptions options_;

  Callback* callback_ = nullptr;

  GC gc_ = nullptr;
  Window root_window_ = BadValue;

  bool use_randr_ = false;
  int randr_event_base_ = 0;
  XRRMonitorInfo* monitors_ = nullptr;
  int num_monitors_ = 0;
  DesktopRect selected_monitor_rect_;

  Atom selected_monitor_name_ = 0;
  typedef XRRMonitorInfo* (*get_monitors_func)(Display*, Window, Bool, int*);
  typedef void (*free_monitors_func)(XRRMonitorInfo*);
  get_monitors_func get_monitors_ = nullptr;
  free_monitors_func free_monitors_ = nullptr;

  bool has_xfixes_ = false;
  int xfixes_event_base_ = -1;
  int xfixes_error_base_ = -1;

  bool use_damage_ = false;
  Damage damage_handle_ = 0;
  int damage_event_base_ = -1;
  int damage_error_base_ = -1;
  XserverRegion damage_region_ = 0;

  XServerPixelBuffer x_server_pixel_buffer_;

  ScreenCapturerHelper helper_;

  ScreenCaptureFrameQueue<SharedDesktopFrame> queue_;

  DesktopRegion last_invalid_region_;

  std::unique_ptr<XAtomCache> atom_cache_;
};

}

#endif
