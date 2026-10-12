/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_WINDOW_CAPTURE_UTILS_H_
#define MODULES_DESKTOP_CAPTURE_WIN_WINDOW_CAPTURE_UTILS_H_

#include <shlobj.h>
#include <windows.h>
#include <wrl/client.h>

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_geometry.h"

namespace webrtc {

bool GetWindowRect(HWND window, DesktopRect* result);

bool GetCroppedWindowRect(HWND window,
                          bool avoid_cropping_border,
                          DesktopRect* cropped_rect,
                          DesktopRect* original_rect);

bool GetWindowContentRect(HWND window, DesktopRect* result);

int GetWindowRegionTypeWithBoundary(HWND window, DesktopRect* result);

bool GetDcSize(HDC hdc, DesktopSize* size);

bool IsWindowMaximized(HWND window, bool* result);

bool IsWindowValidAndVisible(HWND window);

bool IsWindowResponding(HWND window);

enum GetWindowListFlags {
  kNone = 0x00,
  kIgnoreUntitled = 1 << 0,
  kIgnoreUnresponsive = 1 << 1,
  kIgnoreCurrentProcessWindows = 1 << 2,
};

bool GetWindowList(int flags,
                   DesktopCapturer::SourceList* windows,
                   LONG ex_style_filters = 0);

typedef HRESULT(WINAPI* DwmIsCompositionEnabledFunc)(BOOL* enabled);
typedef HRESULT(WINAPI* DwmGetWindowAttributeFunc)(HWND hwnd,
                                                   DWORD flag,
                                                   PVOID result_ptr,
                                                   DWORD result_size);
class WindowCaptureHelperWin {
 public:
  WindowCaptureHelperWin();
  ~WindowCaptureHelperWin();

  WindowCaptureHelperWin(const WindowCaptureHelperWin&) = delete;
  WindowCaptureHelperWin& operator=(const WindowCaptureHelperWin&) = delete;

  bool IsAeroEnabled();
  bool IsWindowChromeNotification(HWND hwnd);
  bool AreWindowsOverlapping(HWND hwnd,
                             HWND selected_hwnd,
                             const DesktopRect& selected_window_rect);
  bool IsWindowOnCurrentDesktop(HWND hwnd);
  bool IsWindowVisibleOnCurrentDesktop(HWND hwnd);
  bool IsWindowCloaked(HWND hwnd);

  bool EnumerateCapturableWindows(DesktopCapturer::SourceList* results,
                                  bool enumerate_current_process_windows,
                                  LONG ex_style_filters = 0);

 private:
  HMODULE dwmapi_library_ = nullptr;
  DwmIsCompositionEnabledFunc func_ = nullptr;
  DwmGetWindowAttributeFunc dwm_get_window_attribute_func_ = nullptr;

  Microsoft::WRL::ComPtr<IVirtualDesktopManager> virtual_desktop_manager_;
};

}

#endif
