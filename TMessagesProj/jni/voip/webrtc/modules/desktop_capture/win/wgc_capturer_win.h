/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURER_WIN_H_
#define MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURER_WIN_H_

#include <DispatcherQueue.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <map>
#include <memory>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/win/screen_capture_utils.h"
#include "modules/desktop_capture/win/wgc_capture_session.h"
#include "modules/desktop_capture/win/wgc_capture_source.h"
#include "modules/desktop_capture/win/window_capture_utils.h"

namespace webrtc {

bool IsWgcSupported(CaptureType capture_type);

class SourceEnumerator {
 public:
  virtual ~SourceEnumerator() = default;

  virtual bool FindAllSources(DesktopCapturer::SourceList* sources) = 0;
};

class WindowEnumerator final : public SourceEnumerator {
 public:
  explicit WindowEnumerator(bool enumerate_current_process_windows)
      : enumerate_current_process_windows_(enumerate_current_process_windows) {}

  WindowEnumerator(const WindowEnumerator&) = delete;
  WindowEnumerator& operator=(const WindowEnumerator&) = delete;

  ~WindowEnumerator() override = default;

  bool FindAllSources(DesktopCapturer::SourceList* sources) override {

    return window_capture_helper_.EnumerateCapturableWindows(
        sources, enumerate_current_process_windows_, WS_EX_TOOLWINDOW);
  }

 private:
  WindowCaptureHelperWin window_capture_helper_;
  bool enumerate_current_process_windows_;
};

class ScreenEnumerator final : public SourceEnumerator {
 public:
  ScreenEnumerator() = default;

  ScreenEnumerator(const ScreenEnumerator&) = delete;
  ScreenEnumerator& operator=(const ScreenEnumerator&) = delete;

  ~ScreenEnumerator() override = default;

  bool FindAllSources(DesktopCapturer::SourceList* sources) override {
    return webrtc::GetScreenList(sources);
  }
};

class WgcCapturerWin : public DesktopCapturer {
 public:
  WgcCapturerWin(const DesktopCaptureOptions& options,
                 std::unique_ptr<WgcCaptureSourceFactory> source_factory,
                 std::unique_ptr<SourceEnumerator> source_enumerator,
                 bool allow_delayed_capturable_check);

  WgcCapturerWin(const WgcCapturerWin&) = delete;
  WgcCapturerWin& operator=(const WgcCapturerWin&) = delete;

  ~WgcCapturerWin() override;

  static std::unique_ptr<DesktopCapturer> CreateRawWindowCapturer(
      const DesktopCaptureOptions& options,
      bool allow_delayed_capturable_check = false);

  static std::unique_ptr<DesktopCapturer> CreateRawScreenCapturer(
      const DesktopCaptureOptions& options);

  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  bool FocusOnSelectedSource() override;
  void Start(Callback* callback) override;
  void CaptureFrame() override;

  bool IsSourceBeingCaptured(SourceId id);

 private:
  typedef HRESULT(WINAPI* CreateDispatcherQueueControllerFunc)(
      DispatcherQueueOptions,
      ABI::Windows::System::IDispatcherQueueController**);

  DesktopCaptureOptions options_;

  bool dispatcher_queue_created_ = false;

  HMODULE core_messaging_library_ = NULL;
  CreateDispatcherQueueControllerFunc create_dispatcher_queue_controller_func_ =
      nullptr;

  std::unique_ptr<WgcCaptureSourceFactory> source_factory_;

  std::unique_ptr<SourceEnumerator> source_enumerator_;

  std::unique_ptr<WgcCaptureSource> capture_source_;

  std::map<SourceId, WgcCaptureSession> ongoing_captures_;

  Callback* callback_ = nullptr;

  bool allow_delayed_capturable_check_ = false;

  Microsoft::WRL::ComPtr<::ID3D11Device> d3d11_device_;
};

}

#endif
