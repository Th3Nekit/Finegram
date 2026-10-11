/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_FALLBACK_DESKTOP_CAPTURER_WRAPPER_H_
#define MODULES_DESKTOP_CAPTURE_FALLBACK_DESKTOP_CAPTURER_WRAPPER_H_

#include <memory>

#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/shared_memory.h"

namespace webrtc {

class FallbackDesktopCapturerWrapper final : public DesktopCapturer,
                                             public DesktopCapturer::Callback {
 public:
  FallbackDesktopCapturerWrapper(
      std::unique_ptr<DesktopCapturer> main_capturer,
      std::unique_ptr<DesktopCapturer> secondary_capturer);
  ~FallbackDesktopCapturerWrapper() override;

  void Start(DesktopCapturer::Callback* callback) override;
  void SetSharedMemoryFactory(
      std::unique_ptr<SharedMemoryFactory> shared_memory_factory) override;
  void CaptureFrame() override;
  void SetExcludedWindow(WindowId window) override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  bool FocusOnSelectedSource() override;
  bool IsOccluded(const DesktopVector& pos) override;

 private:

  void OnCaptureResult(Result result,
                       std::unique_ptr<DesktopFrame> frame) override;

  const std::unique_ptr<DesktopCapturer> main_capturer_;
  const std::unique_ptr<DesktopCapturer> secondary_capturer_;
  std::unique_ptr<SharedMemoryFactory> shared_memory_factory_;
  bool main_capturer_permanent_error_ = false;
  DesktopCapturer::Callback* callback_ = nullptr;
};

}

#endif
