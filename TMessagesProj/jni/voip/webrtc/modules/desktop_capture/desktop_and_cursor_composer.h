/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_DESKTOP_AND_CURSOR_COMPOSER_H_
#define MODULES_DESKTOP_CAPTURE_DESKTOP_AND_CURSOR_COMPOSER_H_

#include <memory>
#if defined(WEBRTC_USE_GIO)
#include "modules/desktop_capture/desktop_capture_metadata.h"
#endif
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/mouse_cursor.h"
#include "modules/desktop_capture/mouse_cursor_monitor.h"
#include "modules/desktop_capture/shared_memory.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT DesktopAndCursorComposer
    : public DesktopCapturer,
      public DesktopCapturer::Callback,
      public MouseCursorMonitor::Callback {
 public:

  DesktopAndCursorComposer(std::unique_ptr<DesktopCapturer> desktop_capturer,
                           const DesktopCaptureOptions& options);

  ~DesktopAndCursorComposer() override;

  DesktopAndCursorComposer(const DesktopAndCursorComposer&) = delete;
  DesktopAndCursorComposer& operator=(const DesktopAndCursorComposer&) = delete;

  static std::unique_ptr<DesktopAndCursorComposer>
  CreateWithoutMouseCursorMonitor(
      std::unique_ptr<DesktopCapturer> desktop_capturer);

  void Start(DesktopCapturer::Callback* callback) override;
  void SetSharedMemoryFactory(
      std::unique_ptr<SharedMemoryFactory> shared_memory_factory) override;
  void CaptureFrame() override;
  void SetExcludedWindow(WindowId window) override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  bool FocusOnSelectedSource() override;
  bool IsOccluded(const DesktopVector& pos) override;
  void SetMaxFrameRate(uint32_t max_frame_rate) override;
#if defined(WEBRTC_USE_GIO)
  DesktopCaptureMetadata GetMetadata() override;
#endif

  void OnMouseCursor(MouseCursor* cursor) override;
  void OnMouseCursorPosition(const DesktopVector& position) override;

 private:

  friend class DesktopAndCursorComposerTest;

  DesktopAndCursorComposer(DesktopCapturer* desktop_capturer,
                           MouseCursorMonitor* mouse_monitor);

  void OnFrameCaptureStart() override;
  void OnCaptureResult(DesktopCapturer::Result result,
                       std::unique_ptr<DesktopFrame> frame) override;

  const std::unique_ptr<DesktopCapturer> desktop_capturer_;
  const std::unique_ptr<MouseCursorMonitor> mouse_monitor_;

  DesktopCapturer::Callback* callback_;

  std::unique_ptr<MouseCursor> cursor_;
  DesktopVector cursor_position_;
  DesktopRect previous_cursor_rect_;
  bool cursor_changed_ = false;
};

}

#endif
