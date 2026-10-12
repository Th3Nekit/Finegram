/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>

#include "modules/desktop_capture/mac/screen_capturer_mac.h"

#include "modules/desktop_capture/mac/desktop_frame_provider.h"
#include "modules/desktop_capture/mac/window_list_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"
#include "sdk/objc/helpers/scoped_cftyperef.h"

namespace webrtc {

namespace {

DesktopRect ScaleAndRoundCGRect(const CGRect& rect, float scale) {
  return DesktopRect::MakeLTRB(static_cast<int>(floor(rect.origin.x * scale)),
                               static_cast<int>(floor(rect.origin.y * scale)),
                               static_cast<int>(ceil((rect.origin.x + rect.size.width) * scale)),
                               static_cast<int>(ceil((rect.origin.y + rect.size.height) * scale)));
}

void CopyRect(const uint8_t* src_plane,
              int src_plane_stride,
              uint8_t* dest_plane,
              int dest_plane_stride,
              int bytes_per_pixel,
              const DesktopRect& rect) {

  const int src_y_offset = src_plane_stride * rect.top();
  const int dest_y_offset = dest_plane_stride * rect.top();
  const int x_offset = bytes_per_pixel * rect.left();
  src_plane += src_y_offset + x_offset;
  dest_plane += dest_y_offset + x_offset;

  const int bytes_per_line = bytes_per_pixel * rect.width();
  const int height = rect.height();
  for (int i = 0; i < height; ++i) {
    memcpy(dest_plane, src_plane, bytes_per_line);
    src_plane += src_plane_stride;
    dest_plane += dest_plane_stride;
  }
}

CFArrayRef CreateWindowListWithExclusion(CGWindowID window_to_exclude) {
  if (!window_to_exclude) return nullptr;

  CFArrayRef all_windows =
      CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
  if (!all_windows) return nullptr;

  CFMutableArrayRef returned_array =
      CFArrayCreateMutable(nullptr, CFArrayGetCount(all_windows), nullptr);

  bool found = false;
  for (CFIndex i = 0; i < CFArrayGetCount(all_windows); ++i) {
    CFDictionaryRef window =
        reinterpret_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(all_windows, i));

    CGWindowID id = GetWindowId(window);
    if (id == window_to_exclude) {
      found = true;
      continue;
    }
    CFArrayAppendValue(returned_array, reinterpret_cast<void*>(id));
  }
  CFRelease(all_windows);

  if (!found) {
    CFRelease(returned_array);
    returned_array = nullptr;
  }
  return returned_array;
}

DesktopRect GetExcludedWindowPixelBounds(CGWindowID window, float dip_to_pixel_scale) {

  static const int kBorderEffectSize = 20;
  CGRect rect;
  CGWindowID ids[1];
  ids[0] = window;

  CFArrayRef window_id_array =
      CFArrayCreate(nullptr, reinterpret_cast<const void**>(&ids), 1, nullptr);
  CFArrayRef window_array = CGWindowListCreateDescriptionFromArray(window_id_array);

  if (CFArrayGetCount(window_array) > 0) {
    CFDictionaryRef win =
        reinterpret_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(window_array, 0));
    CFDictionaryRef bounds_ref =
        reinterpret_cast<CFDictionaryRef>(CFDictionaryGetValue(win, kCGWindowBounds));
    CGRectMakeWithDictionaryRepresentation(bounds_ref, &rect);
  }

  CFRelease(window_id_array);
  CFRelease(window_array);

  rect.origin.x -= kBorderEffectSize;
  rect.origin.y -= kBorderEffectSize;
  rect.size.width += kBorderEffectSize * 2;
  rect.size.height += kBorderEffectSize * 2;

  return ScaleAndRoundCGRect(rect, dip_to_pixel_scale);
}

rtc::ScopedCFTypeRef<CGImageRef> CreateExcludedWindowRegionImage(const DesktopRect& pixel_bounds,
                                                                 float dip_to_pixel_scale,
                                                                 CFArrayRef window_list) {
  CGRect window_bounds;

  window_bounds.origin.x = pixel_bounds.left() / dip_to_pixel_scale;
  window_bounds.origin.y = pixel_bounds.top() / dip_to_pixel_scale;
  window_bounds.size.width = pixel_bounds.width();
  window_bounds.size.height = pixel_bounds.height();

  return rtc::ScopedCFTypeRef<CGImageRef>(
      CGWindowListCreateImageFromArray(window_bounds, window_list, kCGWindowImageDefault));
}

}

ScreenCapturerMac::ScreenCapturerMac(
    rtc::scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor,
    bool detect_updated_region,
    bool allow_iosurface)
    : detect_updated_region_(detect_updated_region),
      desktop_config_monitor_(desktop_config_monitor),
      desktop_frame_provider_(allow_iosurface) {
  RTC_LOG(LS_INFO) << "Allow IOSurface: " << allow_iosurface;
  thread_checker_.Detach();
}

ScreenCapturerMac::~ScreenCapturerMac() {
  RTC_DCHECK(thread_checker_.IsCurrent());
  ReleaseBuffers();
  UnregisterRefreshAndMoveHandlers();
}

bool ScreenCapturerMac::Init() {
  TRACE_EVENT0("webrtc", "ScreenCapturerMac::Init");
  desktop_config_ = desktop_config_monitor_->desktop_configuration();
  return true;
}

void ScreenCapturerMac::ReleaseBuffers() {

  queue_.Reset();
}

void ScreenCapturerMac::Start(Callback* callback) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);
  TRACE_EVENT_INSTANT1(
      "webrtc", "ScreenCapturermac::Start", "target display id ", current_display_);

  callback_ = callback;

  if (!RegisterRefreshAndMoveHandlers()) {
    RTC_LOG(LS_ERROR) << "Failed to register refresh and move handlers.";
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }
  ScreenConfigurationChanged();
}

void ScreenCapturerMac::CaptureFrame() {
  RTC_DCHECK(thread_checker_.IsCurrent());
  TRACE_EVENT0("webrtc", "creenCapturerMac::CaptureFrame");
  int64_t capture_start_time_nanos = rtc::TimeNanos();

  queue_.MoveToNextFrame();
  if (queue_.current_frame() && queue_.current_frame()->IsShared()) {
    RTC_DLOG(LS_WARNING) << "Overwriting frame that is still shared.";
  }

  MacDesktopConfiguration new_config = desktop_config_monitor_->desktop_configuration();
  if (!desktop_config_.Equals(new_config)) {
    desktop_config_ = new_config;

    UnregisterRefreshAndMoveHandlers();
    if (!RegisterRefreshAndMoveHandlers()) {
      RTC_LOG(LS_ERROR) << "Failed to register refresh and move handlers.";
      callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
      return;
    }
    ScreenConfigurationChanged();
  }

  if (UAZoomEnabled()) {
    helper_.InvalidateScreen(screen_pixel_bounds_.size());
  }

  DesktopRegion region;
  helper_.TakeInvalidRegion(&region);

  if (!queue_.current_frame()) queue_.ReplaceCurrentFrame(SharedDesktopFrame::Wrap(CreateFrame()));

  DesktopFrame* current_frame = queue_.current_frame();

  if (!CgBlit(*current_frame, region)) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }
  std::unique_ptr<DesktopFrame> new_frame = queue_.current_frame()->Share();
  if (detect_updated_region_) {
    *new_frame->mutable_updated_region() = region;
  } else {
    new_frame->mutable_updated_region()->AddRect(DesktopRect::MakeSize(new_frame->size()));
  }

  if (current_display_) {
    const MacDisplayConfiguration* config =
        desktop_config_.FindDisplayConfigurationById(current_display_);
    if (config) {
      new_frame->set_top_left(
          config->bounds.top_left().subtract(desktop_config_.bounds.top_left()));
    }
  }

  helper_.set_size_most_recent(new_frame->size());

  new_frame->set_capture_time_ms((rtc::TimeNanos() - capture_start_time_nanos) /
                                 rtc::kNumNanosecsPerMillisec);
  callback_->OnCaptureResult(Result::SUCCESS, std::move(new_frame));
}

void ScreenCapturerMac::SetExcludedWindow(WindowId window) {
  excluded_window_ = window;
}

bool ScreenCapturerMac::GetSourceList(SourceList* screens) {
  RTC_DCHECK(screens->size() == 0);

  for (MacDisplayConfigurations::iterator it = desktop_config_.displays.begin();
       it != desktop_config_.displays.end();
       ++it) {
    screens->push_back({it->id, std::string()});
  }
  return true;
}

bool ScreenCapturerMac::SelectSource(SourceId id) {
  if (id == kFullDesktopScreenId) {
    current_display_ = 0;
  } else {
    const MacDisplayConfiguration* config =
        desktop_config_.FindDisplayConfigurationById(static_cast<CGDirectDisplayID>(id));
    if (!config) return false;
    current_display_ = config->id;
  }

  ScreenConfigurationChanged();
  return true;
}

bool ScreenCapturerMac::CgBlit(const DesktopFrame& frame, const DesktopRegion& region) {

  if (queue_.previous_frame() && !region.Equals(DesktopRegion(screen_pixel_bounds_))) {
    memcpy(frame.data(), queue_.previous_frame()->data(), frame.stride() * frame.size().height());
  }

  MacDisplayConfigurations displays_to_capture;
  if (current_display_) {

    const MacDisplayConfiguration* config =
        desktop_config_.FindDisplayConfigurationById(current_display_);
    if (config) {
      displays_to_capture.push_back(*config);
    } else {
      RTC_LOG(LS_ERROR) << "The selected screen cannot be found for capturing.";
      return false;
    }
  } else {

    displays_to_capture = desktop_config_.displays;
  }

  CFArrayRef window_list = CreateWindowListWithExclusion(excluded_window_);

  for (size_t i = 0; i < displays_to_capture.size(); ++i) {
    const MacDisplayConfiguration& display_config = displays_to_capture[i];

    if (i > 0 && display_config.dip_to_pixel_scale != displays_to_capture[0].dip_to_pixel_scale) {
      continue;
    }

    DesktopRect display_bounds = display_config.pixel_bounds;
    display_bounds.Translate(-screen_pixel_bounds_.left(), -screen_pixel_bounds_.top());

    DesktopRegion copy_region = region;
    copy_region.IntersectWith(display_bounds);
    if (copy_region.is_empty()) continue;

    copy_region.Translate(-display_bounds.left(), -display_bounds.top());

    DesktopRect excluded_window_bounds;
    rtc::ScopedCFTypeRef<CGImageRef> excluded_image;
    if (excluded_window_ && window_list) {

      excluded_window_bounds =
          GetExcludedWindowPixelBounds(excluded_window_, display_config.dip_to_pixel_scale);
      excluded_window_bounds.IntersectWith(display_config.pixel_bounds);

      if (!excluded_window_bounds.is_empty()) {
        excluded_image = CreateExcludedWindowRegionImage(
            excluded_window_bounds, display_config.dip_to_pixel_scale, window_list);
      }
    }

    std::unique_ptr<DesktopFrame> frame_source =
        desktop_frame_provider_.TakeLatestFrameForDisplay(display_config.id);
    if (!frame_source) {
      continue;
    }

    const uint8_t* display_base_address = frame_source->data();
    int src_bytes_per_row = frame_source->stride();
    RTC_DCHECK(display_base_address);

    copy_region.IntersectWith(frame_source->rect());

    uint8_t* out_ptr = frame.GetFrameDataAtPos(display_bounds.top_left());
    for (DesktopRegion::Iterator it(copy_region); !it.IsAtEnd(); it.Advance()) {
      CopyRect(display_base_address,
               src_bytes_per_row,
               out_ptr,
               frame.stride(),
               DesktopFrame::kBytesPerPixel,
               it.rect());
    }

    if (excluded_image) {
      CGDataProviderRef provider = CGImageGetDataProvider(excluded_image.get());
      rtc::ScopedCFTypeRef<CFDataRef> excluded_image_data(CGDataProviderCopyData(provider));
      RTC_DCHECK(excluded_image_data);
      display_base_address = CFDataGetBytePtr(excluded_image_data.get());
      src_bytes_per_row = CGImageGetBytesPerRow(excluded_image.get());

      DesktopRect window_bounds_relative_to_desktop(excluded_window_bounds);
      window_bounds_relative_to_desktop.Translate(-screen_pixel_bounds_.left(),
                                                  -screen_pixel_bounds_.top());

      DesktopRect rect_to_copy = DesktopRect::MakeSize(excluded_window_bounds.size());
      rect_to_copy.IntersectWith(DesktopRect::MakeWH(CGImageGetWidth(excluded_image.get()),
                                                     CGImageGetHeight(excluded_image.get())));

      if (CGImageGetBitsPerPixel(excluded_image.get()) / 8 == DesktopFrame::kBytesPerPixel) {
        CopyRect(display_base_address,
                 src_bytes_per_row,
                 frame.GetFrameDataAtPos(window_bounds_relative_to_desktop.top_left()),
                 frame.stride(),
                 DesktopFrame::kBytesPerPixel,
                 rect_to_copy);
      }
    }
  }
  if (window_list) CFRelease(window_list);
  return true;
}

void ScreenCapturerMac::ScreenConfigurationChanged() {
  if (current_display_) {
    const MacDisplayConfiguration* config =
        desktop_config_.FindDisplayConfigurationById(current_display_);
    screen_pixel_bounds_ = config ? config->pixel_bounds : DesktopRect();
    dip_to_pixel_scale_ = config ? config->dip_to_pixel_scale : 1.0f;
  } else {
    screen_pixel_bounds_ = desktop_config_.pixel_bounds;
    dip_to_pixel_scale_ = desktop_config_.dip_to_pixel_scale;
  }

  ReleaseBuffers();

  helper_.ClearInvalidRegion();

  helper_.InvalidateScreen(screen_pixel_bounds_.size());

  queue_.Reset();
}

bool ScreenCapturerMac::RegisterRefreshAndMoveHandlers() {
  RTC_DCHECK(thread_checker_.IsCurrent());
  if (!desktop_frame_provider_.allow_iosurface()) {
    return true;
  }

  desktop_config_ = desktop_config_monitor_->desktop_configuration();
  for (const auto& config : desktop_config_.displays) {
    size_t pixel_width = config.pixel_bounds.width();
    size_t pixel_height = config.pixel_bounds.height();
    if (pixel_width == 0 || pixel_height == 0) continue;
    CGDirectDisplayID display_id = config.id;
    DesktopVector display_origin = config.pixel_bounds.top_left();

    CGDisplayStreamFrameAvailableHandler handler = ^(CGDisplayStreamFrameStatus status,
                                                     uint64_t display_time,
                                                     IOSurfaceRef frame_surface,
                                                     CGDisplayStreamUpdateRef updateRef) {
      RTC_DCHECK(thread_checker_.IsCurrent());
      if (status == kCGDisplayStreamFrameStatusStopped) return;

      if (status != kCGDisplayStreamFrameStatusFrameComplete) return;

      size_t count = 0;
      const CGRect* rects =
          CGDisplayStreamUpdateGetRects(updateRef, kCGDisplayStreamUpdateDirtyRects, &count);
      if (count != 0) {

        ScreenRefresh(display_id, count, rects, display_origin, frame_surface);
      }
    };

    rtc::ScopedCFTypeRef<CFDictionaryRef> properties_dict(
        CFDictionaryCreate(kCFAllocatorDefault,
                           (const void*[]){kCGDisplayStreamShowCursor},
                           (const void*[]){kCFBooleanFalse},
                           1,
                           &kCFTypeDictionaryKeyCallBacks,
                           &kCFTypeDictionaryValueCallBacks));

    CGDisplayStreamRef display_stream = CGDisplayStreamCreate(
        display_id, pixel_width, pixel_height, 'BGRA', properties_dict.get(), handler);

    if (display_stream) {
      CGError error = CGDisplayStreamStart(display_stream);
      if (error != kCGErrorSuccess) return false;

      CFRunLoopSourceRef source = CGDisplayStreamGetRunLoopSource(display_stream);
      CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
      display_streams_.push_back(display_stream);
    }
  }

  return true;
}

void ScreenCapturerMac::UnregisterRefreshAndMoveHandlers() {
  RTC_DCHECK(thread_checker_.IsCurrent());

  for (CGDisplayStreamRef stream : display_streams_) {
    CFRunLoopSourceRef source = CGDisplayStreamGetRunLoopSource(stream);
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
    CGDisplayStreamStop(stream);
    CFRelease(stream);
  }
  display_streams_.clear();

  desktop_frame_provider_.Release();
}

void ScreenCapturerMac::ScreenRefresh(CGDirectDisplayID display_id,
                                      CGRectCount count,
                                      const CGRect* rect_array,
                                      DesktopVector display_origin,
                                      IOSurfaceRef io_surface) {
  if (screen_pixel_bounds_.is_empty()) ScreenConfigurationChanged();

  DesktopVector translate_vector;
  if (!current_display_) translate_vector = display_origin;

  DesktopRegion region;
  for (CGRectCount i = 0; i < count; ++i) {

    DesktopRect rect = DesktopRect::MakeXYWH(rect_array[i].origin.x,
                                             rect_array[i].origin.y,
                                             rect_array[i].size.width,
                                             rect_array[i].size.height);

    rect.Translate(translate_vector);

    region.AddRect(rect);
  }

  desktop_frame_provider_.InvalidateIOSurface(
      display_id, rtc::ScopedCFTypeRef<IOSurfaceRef>(io_surface, rtc::RetainPolicy::RETAIN));
  helper_.InvalidateRegion(region);
}

std::unique_ptr<DesktopFrame> ScreenCapturerMac::CreateFrame() {
  std::unique_ptr<DesktopFrame> frame(new BasicDesktopFrame(screen_pixel_bounds_.size()));
  frame->set_dpi(
      DesktopVector(kStandardDPI * dip_to_pixel_scale_, kStandardDPI * dip_to_pixel_scale_));
  return frame;
}

}
