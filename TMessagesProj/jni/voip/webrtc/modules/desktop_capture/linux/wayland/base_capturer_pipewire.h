/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_BASE_CAPTURER_PIPEWIRE_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_BASE_CAPTURER_PIPEWIRE_H_

#include "modules/desktop_capture/delegated_source_list_controller.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/linux/wayland/screen_capture_portal_interface.h"
#include "modules/desktop_capture/linux/wayland/screencast_portal.h"
#include "modules/desktop_capture/linux/wayland/shared_screencast_stream.h"
#include "modules/portal/portal_request_response.h"
#include "modules/portal/xdg_desktop_portal_utils.h"
#include "modules/portal/xdg_session_details.h"

namespace webrtc {

class RTC_EXPORT BaseCapturerPipeWire
    : public DesktopCapturer,
      public DelegatedSourceListController,
      public ScreenCastPortal::PortalNotifier {
 public:

  static bool IsSupported();

  BaseCapturerPipeWire(const DesktopCaptureOptions& options, CaptureType type);
  BaseCapturerPipeWire(
      const DesktopCaptureOptions& options,
      std::unique_ptr<xdg_portal::ScreenCapturePortalInterface> portal);
  ~BaseCapturerPipeWire() override;

  BaseCapturerPipeWire(const BaseCapturerPipeWire&) = delete;
  BaseCapturerPipeWire& operator=(const BaseCapturerPipeWire&) = delete;

  void Start(Callback* delegate) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  DelegatedSourceListController* GetDelegatedSourceListController() override;
  void SetMaxFrameRate(uint32_t max_frame_rate) override;

  void Observe(Observer* observer) override;
  void EnsureVisible() override;
  void EnsureHidden() override;

  void OnScreenCastRequestResult(xdg_portal::RequestResponse result,
                                 uint32_t stream_node_id,
                                 int fd) override;
  void OnScreenCastSessionClosed() override;
  void UpdateResolution(uint32_t width, uint32_t height) override;

  xdg_portal::SessionDetails GetSessionDetails();

  void SendFramesImmediately(bool send_frames_immediately);

 private:
  ScreenCastPortal* GetScreenCastPortal();

  DesktopCaptureOptions options_ = {};
  Callback* callback_ = nullptr;
  bool send_frames_immediately_ = false;
  bool capturer_failed_ = false;
  bool is_screencast_portal_ = false;
  bool is_portal_open_ = false;

  Observer* delegated_source_list_observer_ = nullptr;

  SourceId selected_source_id_ = 0;

  SourceId source_id_ = 0;
  std::unique_ptr<xdg_portal::ScreenCapturePortalInterface> portal_;
};

}

#endif
