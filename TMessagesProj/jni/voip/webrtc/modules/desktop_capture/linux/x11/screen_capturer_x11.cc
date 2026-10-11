/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/x11/screen_capturer_x11.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/damagewire.h>
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>

#include <memory>
#include <utility>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/linux/x11/x_server_pixel_buffer.h"
#include "modules/desktop_capture/screen_capture_frame_queue.h"
#include "modules/desktop_capture/screen_capturer_helper.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/sanitizer.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

ScreenCapturerX11::ScreenCapturerX11() {
  helper_.SetLogGridSize(4);
}

ScreenCapturerX11::~ScreenCapturerX11() {
  options_.x_display()->RemoveEventHandler(ConfigureNotify, this);
  if (use_damage_) {
    options_.x_display()->RemoveEventHandler(damage_event_base_ + XDamageNotify,
                                             this);
  }
  if (use_randr_) {
    options_.x_display()->RemoveEventHandler(
        randr_event_base_ + RRScreenChangeNotify, this);
  }
  DeinitXlib();
}

bool ScreenCapturerX11::Init(const DesktopCaptureOptions& options) {
  TRACE_EVENT0("webrtc", "ScreenCapturerX11::Init");
  options_ = options;

  atom_cache_ = std::make_unique<XAtomCache>(display());

  root_window_ = RootWindow(display(), DefaultScreen(display()));
  if (root_window_ == BadValue) {
    RTC_LOG(LS_ERROR) << "Unable to get the root window";
    DeinitXlib();
    return false;
  }

  gc_ = XCreateGC(display(), root_window_, 0, NULL);
  if (gc_ == NULL) {
    RTC_LOG(LS_ERROR) << "Unable to get graphics context";
    DeinitXlib();
    return false;
  }

  options_.x_display()->AddEventHandler(ConfigureNotify, this);

  if (XFixesQueryExtension(display(), &xfixes_event_base_,
                           &xfixes_error_base_)) {
    has_xfixes_ = true;
  } else {
    RTC_LOG(LS_INFO) << "X server does not support XFixes.";
  }

  XSelectInput(display(), root_window_, StructureNotifyMask);

  if (!x_server_pixel_buffer_.Init(atom_cache_.get(),
                                   DefaultRootWindow(display()))) {
    RTC_LOG(LS_ERROR) << "Failed to initialize pixel buffer.";
    return false;
  }

  if (options_.use_update_notifications()) {
    InitXDamage();
  }

  InitXrandr();

  SelectSource(kFullDesktopScreenId);

  return true;
}

void ScreenCapturerX11::InitXDamage() {

  if (!has_xfixes_) {
    return;
  }

  if (!XDamageQueryExtension(display(), &damage_event_base_,
                             &damage_error_base_)) {
    RTC_LOG(LS_INFO) << "X server does not support XDamage.";
    return;
  }

  damage_handle_ =
      XDamageCreate(display(), root_window_, XDamageReportNonEmpty);
  if (!damage_handle_) {
    RTC_LOG(LS_ERROR) << "Unable to initialize XDamage.";
    return;
  }

  damage_region_ = XFixesCreateRegion(display(), 0, 0);
  if (!damage_region_) {
    XDamageDestroy(display(), damage_handle_);
    RTC_LOG(LS_ERROR) << "Unable to create XFixes region.";
    return;
  }

  options_.x_display()->AddEventHandler(damage_event_base_ + XDamageNotify,
                                        this);

  use_damage_ = true;
  RTC_LOG(LS_INFO) << "Using XDamage extension.";
}

RTC_NO_SANITIZE("cfi-icall")
void ScreenCapturerX11::InitXrandr() {
  int major_version = 0;
  int minor_version = 0;
  int error_base_ignored = 0;
  if (XRRQueryExtension(display(), &randr_event_base_, &error_base_ignored) &&
      XRRQueryVersion(display(), &major_version, &minor_version)) {
    if (major_version > 1 || (major_version == 1 && minor_version >= 5)) {

      get_monitors_ = reinterpret_cast<get_monitors_func>(
          dlsym(RTLD_DEFAULT, "XRRGetMonitors"));
      free_monitors_ = reinterpret_cast<free_monitors_func>(
          dlsym(RTLD_DEFAULT, "XRRFreeMonitors"));
      if (get_monitors_ && free_monitors_) {
        use_randr_ = true;
        RTC_LOG(LS_INFO) << "Using XRandR extension v" << major_version << '.'
                         << minor_version << '.';
        monitors_ =
            get_monitors_(display(), root_window_, true, &num_monitors_);

        XRRSelectInput(display(), root_window_, RRScreenChangeNotifyMask);
        options_.x_display()->AddEventHandler(
            randr_event_base_ + RRScreenChangeNotify, this);
      } else {
        RTC_LOG(LS_ERROR) << "Unable to link XRandR monitor functions.";
      }
    } else {
      RTC_LOG(LS_ERROR) << "XRandR entension is older than v1.5.";
    }
  } else {
    RTC_LOG(LS_ERROR) << "X server does not support XRandR.";
  }
}

RTC_NO_SANITIZE("cfi-icall")
void ScreenCapturerX11::UpdateMonitors() {

  queue_.Reset();

  if (monitors_) {
    free_monitors_(monitors_);
    monitors_ = nullptr;
  }

  monitors_ = get_monitors_(display(), root_window_, true, &num_monitors_);

  if (selected_monitor_name_) {
    if (selected_monitor_name_ == static_cast<Atom>(kFullDesktopScreenId)) {
      selected_monitor_rect_ =
          DesktopRect::MakeSize(x_server_pixel_buffer_.window_size());
      return;
    }

    for (int i = 0; i < num_monitors_; ++i) {
      XRRMonitorInfo& m = monitors_[i];
      if (selected_monitor_name_ == m.name) {
        RTC_LOG(LS_INFO) << "XRandR monitor " << m.name << " rect updated.";
        selected_monitor_rect_ =
            DesktopRect::MakeXYWH(m.x, m.y, m.width, m.height);
        const auto& pixel_buffer_rect = x_server_pixel_buffer_.window_rect();
        if (!pixel_buffer_rect.ContainsRect(selected_monitor_rect_)) {

          RTC_LOG(LS_WARNING)
              << "Cropping selected monitor rect to fit the pixel-buffer.";
          selected_monitor_rect_.IntersectWith(pixel_buffer_rect);
        }
        return;
      }
    }

    RTC_LOG(LS_INFO) << "XRandR selected monitor " << selected_monitor_name_
                     << " lost.";
    selected_monitor_rect_ = DesktopRect::MakeWH(0, 0);
  }
}

void ScreenCapturerX11::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;
}

void ScreenCapturerX11::CaptureFrame() {
  TRACE_EVENT0("webrtc", "ScreenCapturerX11::CaptureFrame");
  int64_t capture_start_time_nanos = rtc::TimeNanos();

  queue_.MoveToNextFrame();
  if (queue_.current_frame() && queue_.current_frame()->IsShared()) {
    RTC_DLOG(LS_WARNING) << "Overwriting frame that is still shared.";
  }

  options_.x_display()->ProcessPendingXEvents();

  if (!x_server_pixel_buffer_.is_initialized()) {

    RTC_LOG(LS_ERROR) << "Pixel buffer is not initialized.";
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  if (!queue_.current_frame()) {
    std::unique_ptr<DesktopFrame> frame(
        new BasicDesktopFrame(selected_monitor_rect_.size()));

    frame->set_top_left(selected_monitor_rect_.top_left());
    queue_.ReplaceCurrentFrame(SharedDesktopFrame::Wrap(std::move(frame)));
  }

  std::unique_ptr<DesktopFrame> result = CaptureScreen();
  if (!result) {
    RTC_LOG(LS_WARNING) << "Temporarily failed to capture screen.";
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  last_invalid_region_ = result->updated_region();
  result->set_capture_time_ms((rtc::TimeNanos() - capture_start_time_nanos) /
                              rtc::kNumNanosecsPerMillisec);
  result->set_capturer_id(DesktopCapturerId::kX11CapturerLinux);
  callback_->OnCaptureResult(Result::SUCCESS, std::move(result));
}

bool ScreenCapturerX11::GetSourceList(SourceList* sources) {
  RTC_DCHECK(sources->size() == 0);
  if (!use_randr_) {
    sources->push_back({});
    return true;
  }

  options_.x_display()->ProcessPendingXEvents();

  for (int i = 0; i < num_monitors_; ++i) {
    XRRMonitorInfo& m = monitors_[i];
    char* monitor_title = XGetAtomName(display(), m.name);

    sources->push_back({static_cast<SourceId>(m.name), monitor_title});
    XFree(monitor_title);
  }

  return true;
}

bool ScreenCapturerX11::SelectSource(SourceId id) {

  queue_.Reset();

  if (!use_randr_ || id == kFullDesktopScreenId) {
    selected_monitor_name_ = kFullDesktopScreenId;
    selected_monitor_rect_ =
        DesktopRect::MakeSize(x_server_pixel_buffer_.window_size());
    return true;
  }

  for (int i = 0; i < num_monitors_; ++i) {
    if (id == static_cast<SourceId>(monitors_[i].name)) {
      RTC_LOG(LS_INFO) << "XRandR selected source: " << id;
      XRRMonitorInfo& m = monitors_[i];
      selected_monitor_name_ = m.name;
      selected_monitor_rect_ =
          DesktopRect::MakeXYWH(m.x, m.y, m.width, m.height);
      const auto& pixel_buffer_rect = x_server_pixel_buffer_.window_rect();
      if (!pixel_buffer_rect.ContainsRect(selected_monitor_rect_)) {
        RTC_LOG(LS_WARNING)
            << "Cropping selected monitor rect to fit the pixel-buffer.";
        selected_monitor_rect_.IntersectWith(pixel_buffer_rect);
      }
      return true;
    }
  }
  return false;
}

bool ScreenCapturerX11::HandleXEvent(const XEvent& event) {
  if (use_damage_ && (event.type == damage_event_base_ + XDamageNotify)) {
    const XDamageNotifyEvent* damage_event =
        reinterpret_cast<const XDamageNotifyEvent*>(&event);
    if (damage_event->damage != damage_handle_)
      return false;
    RTC_DCHECK(damage_event->level == XDamageReportNonEmpty);
    return true;
  } else if (use_randr_ &&
             event.type == randr_event_base_ + RRScreenChangeNotify) {
    XRRUpdateConfiguration(const_cast<XEvent*>(&event));
    UpdateMonitors();
    RTC_LOG(LS_INFO) << "XRandR screen change event received.";
    return false;
  } else if (event.type == ConfigureNotify) {
    ScreenConfigurationChanged();
    return false;
  }
  return false;
}

std::unique_ptr<DesktopFrame> ScreenCapturerX11::CaptureScreen() {
  std::unique_ptr<SharedDesktopFrame> frame = queue_.current_frame()->Share();
  RTC_DCHECK(selected_monitor_rect_.size().equals(frame->size()));
  RTC_DCHECK(selected_monitor_rect_.top_left().equals(frame->top_left()));

  helper_.set_size_most_recent(frame->size());

  if (use_damage_ && queue_.previous_frame())
    SynchronizeFrame();

  DesktopRegion* updated_region = frame->mutable_updated_region();

  x_server_pixel_buffer_.Synchronize();
  if (use_damage_ && queue_.previous_frame()) {

    XDamageSubtract(display(), damage_handle_, None, damage_region_);
    int rects_num = 0;
    XRectangle bounds;
    XRectangle* rects = XFixesFetchRegionAndBounds(display(), damage_region_,
                                                   &rects_num, &bounds);
    for (int i = 0; i < rects_num; ++i) {
      auto damage_rect = DesktopRect::MakeXYWH(rects[i].x, rects[i].y,
                                               rects[i].width, rects[i].height);

      damage_rect.Translate(-frame->top_left());
      updated_region->AddRect(damage_rect);
    }
    XFree(rects);
    helper_.InvalidateRegion(*updated_region);

    helper_.TakeInvalidRegion(updated_region);

    for (DesktopRegion::Iterator it(*updated_region); !it.IsAtEnd();
         it.Advance()) {
      auto rect = it.rect();
      rect.Translate(frame->top_left());
      if (!x_server_pixel_buffer_.CaptureRect(rect, frame.get()))
        return nullptr;
    }
  } else {

    if (!x_server_pixel_buffer_.CaptureRect(selected_monitor_rect_,
                                            frame.get())) {
      return nullptr;
    }
    updated_region->SetRect(DesktopRect::MakeSize(frame->size()));
  }

  return std::move(frame);
}

void ScreenCapturerX11::ScreenConfigurationChanged() {
  TRACE_EVENT0("webrtc", "ScreenCapturerX11::ScreenConfigurationChanged");

  queue_.Reset();

  helper_.ClearInvalidRegion();
  if (!x_server_pixel_buffer_.Init(atom_cache_.get(),
                                   DefaultRootWindow(display()))) {
    RTC_LOG(LS_ERROR) << "Failed to initialize pixel buffer after screen "
                         "configuration change.";
  }

  if (use_randr_) {

    UpdateMonitors();
  } else {
    selected_monitor_rect_ =
        DesktopRect::MakeSize(x_server_pixel_buffer_.window_size());
  }
}

void ScreenCapturerX11::SynchronizeFrame() {

  RTC_DCHECK(queue_.previous_frame());

  DesktopFrame* current = queue_.current_frame();
  DesktopFrame* last = queue_.previous_frame();
  RTC_DCHECK(current != last);
  for (DesktopRegion::Iterator it(last_invalid_region_); !it.IsAtEnd();
       it.Advance()) {
    const DesktopRect& r = it.rect();
    current->CopyPixelsFrom(*last, r.top_left(), r);
  }
}

RTC_NO_SANITIZE("cfi-icall")
void ScreenCapturerX11::DeinitXlib() {
  if (monitors_) {
    free_monitors_(monitors_);
    monitors_ = nullptr;
  }

  if (gc_) {
    XFreeGC(display(), gc_);
    gc_ = nullptr;
  }

  x_server_pixel_buffer_.Release();

  if (display()) {
    if (damage_handle_) {
      XDamageDestroy(display(), damage_handle_);
      damage_handle_ = 0;
    }

    if (damage_region_) {
      XFixesDestroyRegion(display(), damage_region_);
      damage_region_ = 0;
    }
  }
}

std::unique_ptr<DesktopCapturer> ScreenCapturerX11::CreateRawScreenCapturer(
    const DesktopCaptureOptions& options) {
  if (!options.x_display())
    return nullptr;

  std::unique_ptr<ScreenCapturerX11> capturer(new ScreenCapturerX11());
  if (!capturer.get()->Init(options)) {
    return nullptr;
  }

  return std::move(capturer);
}

}
