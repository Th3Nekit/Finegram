/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_DIRECTX_H_
#define MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_DIRECTX_H_

#include <d3dcommon.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "api/scoped_refptr.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/screen_capture_frame_queue.h"
#include "modules/desktop_capture/win/dxgi_duplicator_controller.h"
#include "modules/desktop_capture/win/dxgi_frame.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT ScreenCapturerWinDirectx : public DesktopCapturer {
 public:
  using D3dInfo = DxgiDuplicatorController::D3dInfo;

  static bool IsSupported();

  static bool RetrieveD3dInfo(D3dInfo* info);

  static bool IsCurrentSessionSupported();

  static bool GetScreenListFromDeviceNames(
      const std::vector<std::string>& device_names,
      DesktopCapturer::SourceList* screens);

  static int GetIndexFromScreenId(ScreenId id,
                                  const std::vector<std::string>& device_names);

  ScreenCapturerWinDirectx();
  explicit ScreenCapturerWinDirectx(const DesktopCaptureOptions& options);

  ~ScreenCapturerWinDirectx() override;

  ScreenCapturerWinDirectx(const ScreenCapturerWinDirectx&) = delete;
  ScreenCapturerWinDirectx& operator=(const ScreenCapturerWinDirectx&) = delete;

  void Start(Callback* callback) override;
  void SetSharedMemoryFactory(
      std::unique_ptr<SharedMemoryFactory> shared_memory_factory) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

 private:
  const rtc::scoped_refptr<DxgiDuplicatorController> controller_;
  DesktopCaptureOptions options_;

  std::unordered_map<SourceId, ScreenCaptureFrameQueue<DxgiFrame>>
      frame_queue_map_;
  std::unique_ptr<SharedMemoryFactory> shared_memory_factory_;
  Callback* callback_ = nullptr;
  SourceId current_screen_id_ = kFullDesktopScreenId;
};

}

#endif
