/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_MAGNIFIER_H_
#define MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_MAGNIFIER_H_

#include <magnification.h>
#include <wincodec.h>
#include <windows.h>

#include <memory>

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/screen_capture_frame_queue.h"
#include "modules/desktop_capture/screen_capturer_helper.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "modules/desktop_capture/win/scoped_thread_desktop.h"

namespace webrtc {

class DesktopFrame;
class DesktopRect;

class ScreenCapturerWinMagnifier : public DesktopCapturer {
 public:
  ScreenCapturerWinMagnifier();
  ~ScreenCapturerWinMagnifier() override;

  ScreenCapturerWinMagnifier(const ScreenCapturerWinMagnifier&) = delete;
  ScreenCapturerWinMagnifier& operator=(const ScreenCapturerWinMagnifier&) =
      delete;

  void Start(Callback* callback) override;
  void SetSharedMemoryFactory(
      std::unique_ptr<SharedMemoryFactory> shared_memory_factory) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* screens) override;
  bool SelectSource(SourceId id) override;
  void SetExcludedWindow(WindowId window) override;

 private:
  typedef BOOL(WINAPI* MagImageScalingCallback)(HWND hwnd,
                                                void* srcdata,
                                                MAGIMAGEHEADER srcheader,
                                                void* destdata,
                                                MAGIMAGEHEADER destheader,
                                                RECT unclipped,
                                                RECT clipped,
                                                HRGN dirty);
  typedef BOOL(WINAPI* MagInitializeFunc)(void);
  typedef BOOL(WINAPI* MagUninitializeFunc)(void);
  typedef BOOL(WINAPI* MagSetWindowSourceFunc)(HWND hwnd, RECT rect);
  typedef BOOL(WINAPI* MagSetWindowFilterListFunc)(HWND hwnd,
                                                   DWORD dwFilterMode,
                                                   int count,
                                                   HWND* pHWND);
  typedef BOOL(WINAPI* MagSetImageScalingCallbackFunc)(
      HWND hwnd,
      MagImageScalingCallback callback);

  static BOOL WINAPI OnMagImageScalingCallback(HWND hwnd,
                                               void* srcdata,
                                               MAGIMAGEHEADER srcheader,
                                               void* destdata,
                                               MAGIMAGEHEADER destheader,
                                               RECT unclipped,
                                               RECT clipped,
                                               HRGN dirty);

  bool CaptureImage(const DesktopRect& rect);

  bool InitializeMagnifier();

  void OnCaptured(void* data, const MAGIMAGEHEADER& header);

  void CreateCurrentFrameIfNecessary(const DesktopSize& size);

  Callback* callback_ = nullptr;
  std::unique_ptr<SharedMemoryFactory> shared_memory_factory_;
  ScreenId current_screen_id_ = kFullDesktopScreenId;
  std::wstring current_device_key_;
  HWND excluded_window_ = NULL;

  ScreenCaptureFrameQueue<SharedDesktopFrame> queue_;

  ScopedThreadDesktop desktop_;

  HDC desktop_dc_ = NULL;

  HMODULE mag_lib_handle_ = NULL;
  MagInitializeFunc mag_initialize_func_ = nullptr;
  MagUninitializeFunc mag_uninitialize_func_ = nullptr;
  MagSetWindowSourceFunc set_window_source_func_ = nullptr;
  MagSetWindowFilterListFunc set_window_filter_list_func_ = nullptr;
  MagSetImageScalingCallbackFunc set_image_scaling_callback_func_ = nullptr;

  HWND host_window_ = NULL;

  HWND magnifier_window_ = NULL;

  bool magnifier_initialized_ = false;

  bool magnifier_capture_succeeded_ = true;
};

}

#endif
