/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/window_capturer_win_gdi.h"

#include <cmath>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "modules/desktop_capture/cropped_desktop_frame.h"
#include "modules/desktop_capture/desktop_capture_metrics_helper.h"
#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame_win.h"
#include "modules/desktop_capture/win/screen_capture_utils.h"
#include "modules/desktop_capture/win/selected_window_context.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"
#include "rtc_base/win/windows_version.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

struct OwnedWindowCollectorContext : public SelectedWindowContext {
  OwnedWindowCollectorContext(HWND selected_window,
                              DesktopRect selected_window_rect,
                              WindowCaptureHelperWin* window_capture_helper,
                              std::vector<HWND>* owned_windows)
      : SelectedWindowContext(selected_window,
                              selected_window_rect,
                              window_capture_helper),
        owned_windows(owned_windows) {}

  std::vector<HWND>* owned_windows;
};

BOOL CALLBACK OwnedWindowCollector(HWND hwnd, LPARAM param) {
  OwnedWindowCollectorContext* context =
      reinterpret_cast<OwnedWindowCollectorContext*>(param);
  if (hwnd == context->selected_window()) {

    return FALSE;
  }

  if (!(GetWindowLong(hwnd, GWL_STYLE) & WS_POPUP) ||
      !context->window_capture_helper()->IsWindowVisibleOnCurrentDesktop(
          hwnd)) {
    return TRUE;
  }

  if (context->IsWindowOwnedBySelectedWindow(hwnd) &&
      context->IsWindowOverlappingSelectedWindow(hwnd)) {

    if (GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT) {
      const WCHAR kSysShadow[] = L"SysShadow";
      const size_t kClassLength = arraysize(kSysShadow);
      WCHAR class_name[kClassLength];
      const int class_name_length =
          GetClassNameW(hwnd, class_name, kClassLength);
      if (class_name_length == kClassLength - 1 &&
          wcscmp(class_name, kSysShadow) == 0) {
        return TRUE;
      }
    }

    context->owned_windows->push_back(hwnd);
  }

  return TRUE;
}

WindowCapturerWinGdi::WindowCapturerWinGdi(
    bool enumerate_current_process_windows)
    : enumerate_current_process_windows_(enumerate_current_process_windows) {}
WindowCapturerWinGdi::~WindowCapturerWinGdi() {}

bool WindowCapturerWinGdi::GetSourceList(SourceList* sources) {
  if (!window_capture_helper_.EnumerateCapturableWindows(
          sources, enumerate_current_process_windows_))
    return false;

  std::map<HWND, DesktopSize> new_map;
  for (const auto& item : *sources) {
    HWND hwnd = reinterpret_cast<HWND>(item.id);
    new_map[hwnd] = window_size_map_[hwnd];
  }
  window_size_map_.swap(new_map);

  return true;
}

bool WindowCapturerWinGdi::SelectSource(SourceId id) {
  HWND window = reinterpret_cast<HWND>(id);
  if (!IsWindowValidAndVisible(window))
    return false;

  window_ = window;

  previous_size_ = window_size_map_[window];
  return true;
}

bool WindowCapturerWinGdi::FocusOnSelectedSource() {
  if (!window_)
    return false;

  if (!IsWindowValidAndVisible(window_))
    return false;

  return BringWindowToTop(window_) && SetForegroundWindow(window_);
}

bool WindowCapturerWinGdi::IsOccluded(const DesktopVector& pos) {
  DesktopVector sys_pos = pos.add(GetFullscreenRect().top_left());
  HWND hwnd =
      reinterpret_cast<HWND>(window_finder_.GetWindowUnderPoint(sys_pos));

  return hwnd != window_ &&
         std::find(owned_windows_.begin(), owned_windows_.end(), hwnd) ==
             owned_windows_.end();
}

void WindowCapturerWinGdi::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);
  RecordCapturerImpl(DesktopCapturerId::kWindowCapturerWinGdi);

  callback_ = callback;
}

void WindowCapturerWinGdi::CaptureFrame() {
  RTC_DCHECK(callback_);
  int64_t capture_start_time_nanos = rtc::TimeNanos();

  CaptureResults results = CaptureFrame(                          true);
  if (!results.frame) {

    results.result = results.result == Result::SUCCESS ? Result::ERROR_TEMPORARY
                                                       : results.result;
    callback_->OnCaptureResult(results.result, nullptr);
    return;
  }

  int capture_time_ms = (rtc::TimeNanos() - capture_start_time_nanos) /
                        rtc::kNumNanosecsPerMillisec;
  RTC_HISTOGRAM_COUNTS_1000(
      "WebRTC.DesktopCapture.Win.WindowGdiCapturerFrameTime", capture_time_ms);
  results.frame->set_capture_time_ms(capture_time_ms);
  results.frame->set_capturer_id(DesktopCapturerId::kWindowCapturerWinGdi);
  callback_->OnCaptureResult(results.result, std::move(results.frame));
}

WindowCapturerWinGdi::CaptureResults WindowCapturerWinGdi::CaptureFrame(
    bool capture_owned_windows) {
  TRACE_EVENT0("webrtc", "WindowCapturerWinGdi::CaptureFrame");

  if (!window_) {
    RTC_LOG(LS_ERROR) << "Window hasn't been selected: " << GetLastError();
    return {Result::ERROR_PERMANENT, nullptr};
  }

  if (!IsWindow(window_)) {
    RTC_LOG(LS_ERROR) << "Target window has been closed.";
    return {Result::ERROR_PERMANENT, nullptr};
  }

  const bool avoid_cropping_border = !capture_owned_windows;
  DesktopRect cropped_rect;
  DesktopRect original_rect;

  if (!GetCroppedWindowRect(window_, avoid_cropping_border, &cropped_rect,
                            &original_rect)) {
    RTC_LOG(LS_WARNING) << "Failed to get drawable window area: "
                        << GetLastError();
    return {Result::ERROR_TEMPORARY, nullptr};
  }

  if (original_rect.is_empty() ||
      !window_capture_helper_.IsWindowVisibleOnCurrentDesktop(window_)) {
    std::unique_ptr<DesktopFrame> frame(
        new BasicDesktopFrame(DesktopSize(1, 1)));

    previous_size_ = frame->size();
    window_size_map_[window_] = previous_size_;
    return {Result::SUCCESS, std::move(frame)};
  }

  HDC window_dc = GetWindowDC(window_);
  if (!window_dc) {
    RTC_LOG(LS_WARNING) << "Failed to get window DC: " << GetLastError();
    return {Result::ERROR_TEMPORARY, nullptr};
  }

  DesktopRect unscaled_cropped_rect = cropped_rect;
  double horizontal_scale = 1.0;
  double vertical_scale = 1.0;

  DesktopSize window_dc_size;
  if (GetDcSize(window_dc, &window_dc_size)) {

    horizontal_scale =
        static_cast<double>(window_dc_size.width()) / original_rect.width();
    vertical_scale =
        static_cast<double>(window_dc_size.height()) / original_rect.height();
    original_rect.Scale(horizontal_scale, vertical_scale);
    cropped_rect.Scale(horizontal_scale, vertical_scale);

    int translate_left = static_cast<int>(std::round(
        (cropped_rect.left() - original_rect.left()) * (horizontal_scale - 1)));
    int translate_top = static_cast<int>(std::round(
        (cropped_rect.top() - original_rect.top()) * (vertical_scale - 1)));
    cropped_rect.Translate(translate_left, translate_top);
  }

  std::unique_ptr<DesktopFrameWin> frame(
      DesktopFrameWin::Create(original_rect.size(), nullptr, window_dc));
  if (!frame.get()) {
    RTC_LOG(LS_WARNING) << "Failed to create frame.";
    ReleaseDC(window_, window_dc);
    return {Result::ERROR_TEMPORARY, nullptr};
  }

  HDC mem_dc = CreateCompatibleDC(window_dc);
  HGDIOBJ previous_object = SelectObject(mem_dc, frame->bitmap());
  BOOL result = FALSE;

  if (rtc::rtc_win::GetVersion() >= rtc::rtc_win::Version::VERSION_WIN8) {

    const UINT flags = PW_RENDERFULLCONTENT;
    result = PrintWindow(window_, mem_dc, flags);
  }

  if (!result && (!window_capture_helper_.IsAeroEnabled() ||
                  !previous_size_.equals(frame->size()))) {
    result = PrintWindow(window_, mem_dc, 0);
  }

  if (!result) {
    result = BitBlt(mem_dc, 0, 0, frame->size().width(), frame->size().height(),
                    window_dc, 0, 0, SRCCOPY);
  }

  SelectObject(mem_dc, previous_object);
  DeleteDC(mem_dc);
  ReleaseDC(window_, window_dc);

  previous_size_ = frame->size();
  window_size_map_[window_] = previous_size_;

  frame->mutable_updated_region()->SetRect(
      DesktopRect::MakeSize(frame->size()));
  frame->set_top_left(
      original_rect.top_left().subtract(GetFullscreenRect().top_left()));

  if (!result) {
    RTC_LOG(LS_ERROR) << "Both PrintWindow() and BitBlt() failed.";
    return {Result::ERROR_TEMPORARY, nullptr};
  }

  cropped_rect.Translate(-original_rect.left(), -original_rect.top());
  std::unique_ptr<DesktopFrame> cropped_frame =
      CreateCroppedDesktopFrame(std::move(frame), cropped_rect);
  RTC_DCHECK(cropped_frame);

  if (capture_owned_windows) {

    owned_windows_.clear();
    OwnedWindowCollectorContext context(window_, unscaled_cropped_rect,
                                        &window_capture_helper_,
                                        &owned_windows_);

    if (context.IsSelectedWindowValid()) {
      EnumWindows(OwnedWindowCollector, reinterpret_cast<LPARAM>(&context));

      if (!owned_windows_.empty()) {
        if (!owned_window_capturer_) {
          owned_window_capturer_ = std::make_unique<WindowCapturerWinGdi>(
              enumerate_current_process_windows_);
        }

        for (auto it = owned_windows_.rbegin(); it != owned_windows_.rend();
             it++) {
          HWND hwnd = *it;
          if (owned_window_capturer_->SelectSource(
                  reinterpret_cast<SourceId>(hwnd))) {
            CaptureResults results = owned_window_capturer_->CaptureFrame(
                                          false);

            if (results.result != DesktopCapturer::Result::SUCCESS) {

              RTC_LOG(LS_INFO) << "Capturing owned window failed (previous "
                                  "error/warning pertained to that)";
            } else {

              cropped_frame->CopyIntersectingPixelsFrom(
                  *results.frame, horizontal_scale, vertical_scale);
            }
          }
        }
      }
    }
  }

  return {Result::SUCCESS, std::move(cropped_frame)};
}

std::unique_ptr<DesktopCapturer> WindowCapturerWinGdi::CreateRawWindowCapturer(
    const DesktopCaptureOptions& options) {
  return std::unique_ptr<DesktopCapturer>(
      new WindowCapturerWinGdi(options.enumerate_current_process_windows()));
}

}
