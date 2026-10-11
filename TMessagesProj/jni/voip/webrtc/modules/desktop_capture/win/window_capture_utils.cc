/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/window_capture_utils.h"

#include <dwmapi.h>

#include <algorithm>

#include "modules/desktop_capture/win/scoped_gdi_object.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/win/windows_version.h"

namespace webrtc {

namespace {

struct GetWindowListParams {
  GetWindowListParams(int flags,
                      LONG ex_style_filters,
                      DesktopCapturer::SourceList* result)
      : ignore_untitled(flags & GetWindowListFlags::kIgnoreUntitled),
        ignore_unresponsive(flags & GetWindowListFlags::kIgnoreUnresponsive),
        ignore_current_process_windows(
            flags & GetWindowListFlags::kIgnoreCurrentProcessWindows),
        ex_style_filters(ex_style_filters),
        result(result) {}
  const bool ignore_untitled;
  const bool ignore_unresponsive;
  const bool ignore_current_process_windows;
  const LONG ex_style_filters;
  DesktopCapturer::SourceList* const result;
};

bool IsWindowOwnedByCurrentProcess(HWND hwnd) {
  DWORD process_id;
  GetWindowThreadProcessId(hwnd, &process_id);
  return process_id == GetCurrentProcessId();
}

BOOL CALLBACK GetWindowListHandler(HWND hwnd, LPARAM param) {
  GetWindowListParams* params = reinterpret_cast<GetWindowListParams*>(param);
  DesktopCapturer::SourceList* list = params->result;

  if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) {
    return TRUE;
  }

  HWND owner = GetWindow(hwnd, GW_OWNER);
  LONG exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
  if (owner && !(exstyle & WS_EX_APPWINDOW)) {
    return TRUE;
  }

  if (exstyle & params->ex_style_filters) {
    return TRUE;
  }

  if (params->ignore_unresponsive && !IsWindowResponding(hwnd)) {
    return TRUE;
  }

  DesktopCapturer::Source window;
  window.id = reinterpret_cast<WindowId>(hwnd);

  bool owned_by_current_process = IsWindowOwnedByCurrentProcess(hwnd);
  if (owned_by_current_process && params->ignore_current_process_windows) {
    return TRUE;
  }

  if (!owned_by_current_process || IsWindowResponding(hwnd)) {
    const size_t kTitleLength = 500;
    WCHAR window_title[kTitleLength] = L"";
    if (GetWindowTextLength(hwnd) != 0 &&
        GetWindowTextW(hwnd, window_title, kTitleLength) > 0) {
      window.title = rtc::ToUtf8(window_title);
    }
  }

  if (params->ignore_untitled && window.title.empty())
    return TRUE;

  const size_t kMaxClassNameLength = 256;
  WCHAR class_name[kMaxClassNameLength] = L"";
  const int class_name_length =
      GetClassNameW(hwnd, class_name, kMaxClassNameLength);
  if (class_name_length < 1)
    return TRUE;

  if (wcscmp(class_name, L"Progman") == 0)
    return TRUE;

  if (wcscmp(class_name, L"Button") == 0)
    return TRUE;

  list->push_back(window);

  return TRUE;
}

}

const wchar_t kChromeWindowClassPrefix[] = L"Chrome_WidgetWin_";

const int kHiddenTaskbarMarginOnScreen = 2;

bool GetWindowRect(HWND window, DesktopRect* result) {
  RECT rect;
  if (!::GetWindowRect(window, &rect)) {
    return false;
  }
  *result = DesktopRect::MakeLTRB(rect.left, rect.top, rect.right, rect.bottom);
  return true;
}

bool GetCroppedWindowRect(HWND window,
                          bool avoid_cropping_border,
                          DesktopRect* cropped_rect,
                          DesktopRect* original_rect) {
  DesktopRect window_rect;
  if (!GetWindowRect(window, &window_rect)) {
    return false;
  }

  if (original_rect) {
    *original_rect = window_rect;
  }
  *cropped_rect = window_rect;

  bool is_maximized = false;
  if (!IsWindowMaximized(window, &is_maximized)) {
    return false;
  }

  if (rtc::rtc_win::GetVersion() >= rtc::rtc_win::Version::VERSION_WIN8 ||
      is_maximized) {

    LONG style = GetWindowLong(window, GWL_STYLE);
    if (style & WS_THICKFRAME || style & DS_MODALFRAME) {
      int width = GetSystemMetrics(SM_CXSIZEFRAME);
      int bottom_height = GetSystemMetrics(SM_CYSIZEFRAME);
      const int visible_border_height = GetSystemMetrics(SM_CYBORDER);
      int top_height = visible_border_height;

      if (avoid_cropping_border) {
        width = std::max(0, width - GetSystemMetrics(SM_CXBORDER));
        bottom_height = std::max(0, bottom_height - visible_border_height);
        top_height = 0;
      }
      cropped_rect->Extend(-width, -top_height, -width, -bottom_height);
    }
  }

  return true;
}

bool GetWindowContentRect(HWND window, DesktopRect* result) {
  if (!GetWindowRect(window, result)) {
    return false;
  }

  RECT rect;
  if (!::GetClientRect(window, &rect)) {
    return false;
  }

  const int width = rect.right - rect.left;

  if (width > 0 && width < result->width()) {

    const int shrink = ((width - result->width()) / 2);

    result->Extend(shrink, 0, shrink, 0);

    if (result->height() > shrink * 2) {
      result->Extend(0, shrink, 0, shrink);
    }
    RTC_DCHECK(!result->is_empty());
  }

  return true;
}

int GetWindowRegionTypeWithBoundary(HWND window, DesktopRect* result) {
  win::ScopedGDIObject<HRGN, win::DeleteObjectTraits<HRGN>> scoped_hrgn(
      CreateRectRgn(0, 0, 0, 0));
  const int region_type = GetWindowRgn(window, scoped_hrgn.Get());

  if (region_type == SIMPLEREGION) {
    RECT rect;
    GetRgnBox(scoped_hrgn.Get(), &rect);
    *result =
        DesktopRect::MakeLTRB(rect.left, rect.top, rect.right, rect.bottom);
  }
  return region_type;
}

bool GetDcSize(HDC hdc, DesktopSize* size) {
  win::ScopedGDIObject<HGDIOBJ, win::DeleteObjectTraits<HGDIOBJ>> scoped_hgdi(
      GetCurrentObject(hdc, OBJ_BITMAP));
  BITMAP bitmap;
  memset(&bitmap, 0, sizeof(BITMAP));
  if (GetObject(scoped_hgdi.Get(), sizeof(BITMAP), &bitmap) == 0) {
    return false;
  }
  size->set(bitmap.bmWidth, bitmap.bmHeight);
  return true;
}

bool IsWindowMaximized(HWND window, bool* result) {
  WINDOWPLACEMENT placement;
  memset(&placement, 0, sizeof(WINDOWPLACEMENT));
  placement.length = sizeof(WINDOWPLACEMENT);
  if (!::GetWindowPlacement(window, &placement)) {
    return false;
  }

  *result = (placement.showCmd == SW_SHOWMAXIMIZED);
  return true;
}

bool IsWindowValidAndVisible(HWND window) {
  return IsWindow(window) && IsWindowVisible(window) && !IsIconic(window);
}

bool IsWindowResponding(HWND window) {

  const UINT uTimeoutMs = 50;
  return SendMessageTimeout(window, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, uTimeoutMs,
                            nullptr);
}

bool GetWindowList(int flags,
                   DesktopCapturer::SourceList* windows,
                   LONG ex_style_filters) {
  GetWindowListParams params(flags, ex_style_filters, windows);
  return ::EnumWindows(&GetWindowListHandler,
                       reinterpret_cast<LPARAM>(&params)) != 0;
}

WindowCaptureHelperWin::WindowCaptureHelperWin() {

  dwmapi_library_ = LoadLibraryW(L"dwmapi.dll");
  if (dwmapi_library_) {
    func_ = reinterpret_cast<DwmIsCompositionEnabledFunc>(
        GetProcAddress(dwmapi_library_, "DwmIsCompositionEnabled"));
    dwm_get_window_attribute_func_ =
        reinterpret_cast<DwmGetWindowAttributeFunc>(
            GetProcAddress(dwmapi_library_, "DwmGetWindowAttribute"));
  }

  if (rtc::rtc_win::GetVersion() >= rtc::rtc_win::Version::VERSION_WIN10) {
    if (FAILED(::CoCreateInstance(__uuidof(VirtualDesktopManager), nullptr,
                                  CLSCTX_ALL,
                                  IID_PPV_ARGS(&virtual_desktop_manager_)))) {
      RTC_LOG(LS_WARNING) << "Fail to create instance of VirtualDesktopManager";
    }
  }
}

WindowCaptureHelperWin::~WindowCaptureHelperWin() {
  if (dwmapi_library_) {
    FreeLibrary(dwmapi_library_);
  }
}

bool WindowCaptureHelperWin::IsAeroEnabled() {
  BOOL result = FALSE;
  if (func_) {
    func_(&result);
  }
  return result != FALSE;
}

bool WindowCaptureHelperWin::IsWindowChromeNotification(HWND hwnd) {
  const size_t kTitleLength = 32;
  WCHAR window_title[kTitleLength];
  GetWindowTextW(hwnd, window_title, kTitleLength);
  if (wcsnlen_s(window_title, kTitleLength) != 0) {
    return false;
  }

  const size_t kClassLength = 256;
  WCHAR class_name[kClassLength];
  const int class_name_length = GetClassNameW(hwnd, class_name, kClassLength);
  if (class_name_length < 1 ||
      wcsncmp(class_name, kChromeWindowClassPrefix,
              wcsnlen_s(kChromeWindowClassPrefix, kClassLength)) != 0) {
    return false;
  }

  const LONG exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
  if ((exstyle & WS_EX_NOACTIVATE) && (exstyle & WS_EX_TOOLWINDOW) &&
      (exstyle & WS_EX_TOPMOST)) {
    return true;
  }

  return false;
}

bool WindowCaptureHelperWin::AreWindowsOverlapping(
    HWND hwnd,
    HWND selected_hwnd,
    const DesktopRect& selected_window_rect) {
  DesktopRect content_rect;
  if (!GetWindowContentRect(hwnd, &content_rect)) {

    return true;
  }
  content_rect.IntersectWith(selected_window_rect);

  if (content_rect.is_empty()) {
    return false;
  }

  bool is_maximized = false;
  IsWindowMaximized(selected_hwnd, &is_maximized);
  bool overlaps_hidden_horizontal_taskbar =
      selected_window_rect.width() == content_rect.width() &&
      content_rect.height() == kHiddenTaskbarMarginOnScreen;
  bool overlaps_hidden_vertical_taskbar =
      selected_window_rect.height() == content_rect.height() &&
      content_rect.width() == kHiddenTaskbarMarginOnScreen;
  if (is_maximized && (overlaps_hidden_horizontal_taskbar ||
                       overlaps_hidden_vertical_taskbar)) {
    return false;
  }

  return true;
}

bool WindowCaptureHelperWin::IsWindowOnCurrentDesktop(HWND hwnd) {

  if (virtual_desktop_manager_) {
    BOOL on_current_desktop;
    if (SUCCEEDED(virtual_desktop_manager_->IsWindowOnCurrentVirtualDesktop(
            hwnd, &on_current_desktop)) &&
        !on_current_desktop) {
      return false;
    }
  }
  return true;
}

bool WindowCaptureHelperWin::IsWindowVisibleOnCurrentDesktop(HWND hwnd) {
  return IsWindowValidAndVisible(hwnd) && IsWindowOnCurrentDesktop(hwnd) &&
         !IsWindowCloaked(hwnd);
}

bool WindowCaptureHelperWin::IsWindowCloaked(HWND hwnd) {
  if (!dwm_get_window_attribute_func_) {

    return false;
  }

  int res = 0;
  if (dwm_get_window_attribute_func_(hwnd, DWMWA_CLOAKED, &res, sizeof(res)) !=
      S_OK) {

    return false;
  }

  return res != 0;
}

bool WindowCaptureHelperWin::EnumerateCapturableWindows(
    DesktopCapturer::SourceList* results,
    bool enumerate_current_process_windows,
    LONG ex_style_filters) {
  int flags = (GetWindowListFlags::kIgnoreUntitled |
               GetWindowListFlags::kIgnoreUnresponsive);
  if (!enumerate_current_process_windows) {
    flags |= GetWindowListFlags::kIgnoreCurrentProcessWindows;
  }

  if (!webrtc::GetWindowList(flags, results, ex_style_filters)) {
    return false;
  }

  for (auto it = results->begin(); it != results->end();) {
    if (!IsWindowVisibleOnCurrentDesktop(reinterpret_cast<HWND>(it->id))) {
      it = results->erase(it);
    } else {
      ++it;
    }
  }

  return true;
}

}
