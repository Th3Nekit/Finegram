/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/cropping_window_capturer.h"
#include "modules/desktop_capture/desktop_capturer_differ_wrapper.h"
#include "modules/desktop_capture/win/screen_capture_utils.h"
#include "modules/desktop_capture/win/selected_window_context.h"
#include "modules/desktop_capture/win/window_capture_utils.h"
#include "rtc_base/logging.h"
#include "rtc_base/trace_event.h"
#include "rtc_base/win/windows_version.h"

namespace webrtc {

namespace {

struct TopWindowVerifierContext : public SelectedWindowContext {
  TopWindowVerifierContext(HWND selected_window,
                           HWND excluded_window,
                           DesktopRect selected_window_rect,
                           WindowCaptureHelperWin* window_capture_helper)
      : SelectedWindowContext(selected_window,
                              selected_window_rect,
                              window_capture_helper),
        excluded_window(excluded_window) {
    RTC_DCHECK_NE(selected_window, excluded_window);
  }

  bool IsTopWindow() {
    if (!IsSelectedWindowValid()) {
      return false;
    }

    int num_retries = 0;
    while (true) {
      HWND hwnd = nullptr;
      while ((hwnd = FindWindowEx(nullptr, hwnd, nullptr, nullptr))) {
        if (hwnd == selected_window()) {

          return true;
        }

        if (hwnd == excluded_window) {
          continue;
        }

        if (!window_capture_helper()->IsWindowVisibleOnCurrentDesktop(hwnd)) {
          continue;
        }

        if (window_capture_helper()->IsWindowChromeNotification(hwnd)) {
          continue;
        }

        if (IsWindowOwnedBySelectedWindow(hwnd)) {
          continue;
        }

        if (IsWindowOverlappingSelectedWindow(hwnd)) {

          return false;
        }
      }

      DWORD lastError = GetLastError();
      if (lastError == ERROR_SUCCESS) {

        RTC_LOG(LS_WARNING) << "Failed to find selected window (only expected "
                               "if it was closed)";
        RTC_DCHECK(!IsWindow(selected_window()));
        return false;
      } else if (lastError == ERROR_INVALID_WINDOW_HANDLE) {

        if (++num_retries <= 10) {
          RTC_LOG(LS_WARNING) << "Enumeration failed due to race with a window "
                                 "closing; retrying - retry #"
                              << num_retries;
          continue;
        } else {
          RTC_LOG(LS_ERROR)
              << "Exhausted retry allowance around window enumeration failures "
                 "due to races with windows closing";
        }
      }

      RTC_LOG(LS_ERROR) << "Failed to enumerate windows: " << lastError;
      RTC_DCHECK_NOTREACHED();
      return false;
    }
  }

  const HWND excluded_window;
};

class CroppingWindowCapturerWin : public CroppingWindowCapturer {
 public:
  explicit CroppingWindowCapturerWin(const DesktopCaptureOptions& options)
      : CroppingWindowCapturer(options),
        enumerate_current_process_windows_(
            options.enumerate_current_process_windows()),
        full_screen_window_detector_(options.full_screen_window_detector()) {}

  void CaptureFrame() override;

 private:
  bool ShouldUseScreenCapturer() override;
  DesktopRect GetWindowRectInVirtualScreen() override;

  WindowId GetWindowToCapture() const;

  DesktopRect window_region_rect_;

  WindowCaptureHelperWin window_capture_helper_;

  bool enumerate_current_process_windows_;

  rtc::scoped_refptr<FullScreenWindowDetector> full_screen_window_detector_;

  mutable bool fullscreen_usage_logged_ = false;
};

void CroppingWindowCapturerWin::CaptureFrame() {
  DesktopCapturer* win_capturer = window_capturer();
  if (win_capturer) {

    if (full_screen_window_detector_) {
      full_screen_window_detector_->UpdateWindowListIfNeeded(
          selected_window(), [this](DesktopCapturer::SourceList* sources) {

            SourceList result;
            int window_list_flags =
                enumerate_current_process_windows_
                    ? GetWindowListFlags::kNone
                    : GetWindowListFlags::kIgnoreCurrentProcessWindows;

            if (!webrtc::GetWindowList(window_list_flags, &result))
              return false;

            auto it = std::remove_if(
                result.begin(), result.end(), [this](const auto& source) {
                  HWND hwnd = reinterpret_cast<HWND>(source.id);
                  return !window_capture_helper_
                              .IsWindowVisibleOnCurrentDesktop(hwnd);
                });
            result.erase(it, result.end());

            sources->swap(result);
            return true;
          });
    }
    win_capturer->SelectSource(GetWindowToCapture());
  }

  CroppingWindowCapturer::CaptureFrame();
}

bool CroppingWindowCapturerWin::ShouldUseScreenCapturer() {
  if (rtc::rtc_win::GetVersion() < rtc::rtc_win::Version::VERSION_WIN8 &&
      window_capture_helper_.IsAeroEnabled()) {
    return false;
  }

  const HWND selected = reinterpret_cast<HWND>(GetWindowToCapture());

  if (!window_capture_helper_.IsWindowVisibleOnCurrentDesktop(selected)) {
    return false;
  }

  const LONG window_ex_style = GetWindowLong(selected, GWL_EXSTYLE);
  if (window_ex_style & WS_EX_LAYERED) {
    COLORREF color_ref_key = 0;
    BYTE alpha = 0;
    DWORD flags = 0;

    if (!GetLayeredWindowAttributes(selected, &color_ref_key, &alpha, &flags))
      return false;

    if ((flags & LWA_COLORKEY) || ((flags & LWA_ALPHA) && (alpha < 255))) {
      return false;
    }
  }

  if (!GetWindowRect(selected, &window_region_rect_)) {
    return false;
  }

  DesktopRect content_rect;
  if (!GetWindowContentRect(selected, &content_rect)) {
    return false;
  }

  DesktopRect region_rect;

  const int region_type =
      GetWindowRegionTypeWithBoundary(selected, &region_rect);

  if (region_type == COMPLEXREGION || region_type == NULLREGION) {
    return false;
  }

  if (region_type == SIMPLEREGION) {

    region_rect.Translate(window_region_rect_.left(),
                          window_region_rect_.top());

    window_region_rect_.IntersectWith(region_rect);
    content_rect.IntersectWith(region_rect);
  }

  if (!GetFullscreenRect().ContainsRect(content_rect)) {
    return false;
  }

  TopWindowVerifierContext context(selected,
                                   reinterpret_cast<HWND>(excluded_window()),
                                   content_rect, &window_capture_helper_);
  return context.IsTopWindow();
}

DesktopRect CroppingWindowCapturerWin::GetWindowRectInVirtualScreen() {
  TRACE_EVENT0("webrtc",
               "CroppingWindowCapturerWin::GetWindowRectInVirtualScreen");
  DesktopRect window_rect;
  HWND hwnd = reinterpret_cast<HWND>(GetWindowToCapture());
  if (!GetCroppedWindowRect(hwnd,                           false, &window_rect,
                                              nullptr)) {
    RTC_LOG(LS_WARNING) << "Failed to get window info: " << GetLastError();
    return window_rect;
  }
  window_rect.IntersectWith(window_region_rect_);

  DesktopRect screen_rect(GetFullscreenRect());
  window_rect.IntersectWith(screen_rect);
  window_rect.Translate(-screen_rect.left(), -screen_rect.top());
  return window_rect;
}

WindowId CroppingWindowCapturerWin::GetWindowToCapture() const {
  const auto selected_source = selected_window();
  const auto full_screen_source =
      full_screen_window_detector_
          ? full_screen_window_detector_->FindFullScreenWindow(selected_source)
          : 0;
  if (full_screen_source && full_screen_source != selected_source &&
      !fullscreen_usage_logged_) {
    fullscreen_usage_logged_ = true;
    LogDesktopCapturerFullscreenDetectorUsage();
  }
  return full_screen_source ? full_screen_source : selected_source;
}

}

std::unique_ptr<DesktopCapturer> CroppingWindowCapturer::CreateCapturer(
    const DesktopCaptureOptions& options) {
  std::unique_ptr<DesktopCapturer> capturer(
      new CroppingWindowCapturerWin(options));
  if (capturer && options.detect_updated_region()) {
    capturer.reset(new DesktopCapturerDifferWrapper(std::move(capturer)));
  }

  return capturer;
}

}
