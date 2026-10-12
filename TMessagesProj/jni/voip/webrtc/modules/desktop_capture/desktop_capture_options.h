/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURE_OPTIONS_H_
#define MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURE_OPTIONS_H_

#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

#if defined(WEBRTC_USE_X11)
#include "modules/desktop_capture/linux/x11/shared_x_display.h"
#endif

#if defined(WEBRTC_USE_PIPEWIRE)
#include "modules/desktop_capture/linux/wayland/shared_screencast_stream.h"
#endif

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include "modules/desktop_capture/mac/desktop_configuration_monitor.h"
#endif

#include "modules/desktop_capture/full_screen_window_detector.h"

namespace webrtc {

class RTC_EXPORT DesktopCaptureOptions {
 public:

  static DesktopCaptureOptions CreateDefault();

  DesktopCaptureOptions();
  DesktopCaptureOptions(const DesktopCaptureOptions& options);
  DesktopCaptureOptions(DesktopCaptureOptions&& options);
  ~DesktopCaptureOptions();

  DesktopCaptureOptions& operator=(const DesktopCaptureOptions& options);
  DesktopCaptureOptions& operator=(DesktopCaptureOptions&& options);

#if defined(WEBRTC_USE_X11)
  const rtc::scoped_refptr<SharedXDisplay>& x_display() const {
    return x_display_;
  }
  void set_x_display(rtc::scoped_refptr<SharedXDisplay> x_display) {
    x_display_ = x_display;
  }
#endif

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)

  const rtc::scoped_refptr<DesktopConfigurationMonitor>& configuration_monitor()
      const {
    return configuration_monitor_;
  }

  void set_configuration_monitor(
      rtc::scoped_refptr<DesktopConfigurationMonitor> m) {
    configuration_monitor_ = m;
  }

  bool allow_iosurface() const { return allow_iosurface_; }
  void set_allow_iosurface(bool allow) { allow_iosurface_ = allow; }
#endif

  const rtc::scoped_refptr<FullScreenWindowDetector>&
  full_screen_window_detector() const {
    return full_screen_window_detector_;
  }
  void set_full_screen_window_detector(
      rtc::scoped_refptr<FullScreenWindowDetector> detector) {
    full_screen_window_detector_ = detector;
  }

  bool use_update_notifications() const { return use_update_notifications_; }
  void set_use_update_notifications(bool use_update_notifications) {
    use_update_notifications_ = use_update_notifications;
  }

  bool disable_effects() const { return disable_effects_; }
  void set_disable_effects(bool disable_effects) {
    disable_effects_ = disable_effects;
  }

  bool detect_updated_region() const { return detect_updated_region_; }
  void set_detect_updated_region(bool detect_updated_region) {
    detect_updated_region_ = detect_updated_region;
  }

  bool prefer_cursor_embedded() const { return prefer_cursor_embedded_; }
  void set_prefer_cursor_embedded(bool prefer_cursor_embedded) {
    prefer_cursor_embedded_ = prefer_cursor_embedded;
  }

#if defined(WEBRTC_WIN)

  bool enumerate_current_process_windows() const {
    return enumerate_current_process_windows_;
  }
  void set_enumerate_current_process_windows(
      bool enumerate_current_process_windows) {
    enumerate_current_process_windows_ = enumerate_current_process_windows;
  }

  bool allow_directx_capturer() const { return allow_directx_capturer_; }
  void set_allow_directx_capturer(bool enabled) {
    allow_directx_capturer_ = enabled;
  }

  bool allow_cropping_window_capturer() const {
    return allow_cropping_window_capturer_;
  }
  void set_allow_cropping_window_capturer(bool allow) {
    allow_cropping_window_capturer_ = allow;
  }

#if defined(RTC_ENABLE_WIN_WGC)

  bool allow_wgc_screen_capturer() const { return allow_wgc_screen_capturer_; }
  void set_allow_wgc_screen_capturer(bool allow) {
    allow_wgc_screen_capturer_ = allow;
  }

  bool allow_wgc_window_capturer() const { return allow_wgc_window_capturer_; }
  void set_allow_wgc_window_capturer(bool allow) {
    allow_wgc_window_capturer_ = allow;
  }

  bool allow_wgc_capturer_fallback() const {
    return allow_wgc_capturer_fallback_;
  }
  void set_allow_wgc_capturer_fallback(bool allow) {
    allow_wgc_capturer_fallback_ = allow;
  }

  bool allow_wgc_zero_hertz() const { return allow_wgc_zero_hertz_; }
  void set_allow_wgc_zero_hertz(bool allow) { allow_wgc_zero_hertz_ = allow; }
#endif
#endif

#if defined(WEBRTC_USE_PIPEWIRE)
  bool allow_pipewire() const { return allow_pipewire_; }
  void set_allow_pipewire(bool allow) { allow_pipewire_ = allow; }

  const rtc::scoped_refptr<SharedScreenCastStream>& screencast_stream() const {
    return screencast_stream_;
  }
  void set_screencast_stream(
      rtc::scoped_refptr<SharedScreenCastStream> stream) {
    screencast_stream_ = stream;
  }

  void set_width(uint32_t width) { width_ = width; }
  uint32_t get_width() const { return width_; }

  void set_height(uint32_t height) { height_ = height; }
  uint32_t get_height() const { return height_; }

  void set_pipewire_use_damage_region(bool use_damage_regions) {
    pipewire_use_damage_region_ = use_damage_regions;
  }
  bool pipewire_use_damage_region() const {
    return pipewire_use_damage_region_;
  }
#endif

 private:
#if defined(WEBRTC_USE_X11)
  rtc::scoped_refptr<SharedXDisplay> x_display_;
#endif
#if defined(WEBRTC_USE_PIPEWIRE)

  rtc::scoped_refptr<SharedScreenCastStream> screencast_stream_;
#endif
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  rtc::scoped_refptr<DesktopConfigurationMonitor> configuration_monitor_;
  bool allow_iosurface_ = false;
#endif

  rtc::scoped_refptr<FullScreenWindowDetector> full_screen_window_detector_;

#if defined(WEBRTC_WIN)
  bool enumerate_current_process_windows_ = true;
  bool allow_directx_capturer_ = false;
  bool allow_cropping_window_capturer_ = false;
#if defined(RTC_ENABLE_WIN_WGC)
  bool allow_wgc_screen_capturer_ = false;
  bool allow_wgc_window_capturer_ = false;
  bool allow_wgc_capturer_fallback_ = false;
  bool allow_wgc_zero_hertz_ = false;
#endif
#endif
#if defined(WEBRTC_USE_X11)
  bool use_update_notifications_ = false;
#else
  bool use_update_notifications_ = true;
#endif
  bool disable_effects_ = true;
  bool detect_updated_region_ = false;
  bool prefer_cursor_embedded_ = false;
#if defined(WEBRTC_USE_PIPEWIRE)
  bool allow_pipewire_ = false;
  bool pipewire_use_damage_region_ = true;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
#endif
};

}

#endif
