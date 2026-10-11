/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SHARED_SCREENCAST_STREAM_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SHARED_SCREENCAST_STREAM_H_

#include <memory>

#include "absl/types/optional.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/mouse_cursor.h"
#include "modules/desktop_capture/screen_capture_frame_queue.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class SharedScreenCastStreamPrivate;

class RTC_EXPORT SharedScreenCastStream
    : public rtc::RefCountedNonVirtual<SharedScreenCastStream> {
 public:
  class Observer {
   public:
    virtual void OnCursorPositionChanged() = 0;
    virtual void OnCursorShapeChanged() = 0;
    virtual void OnDesktopFrameChanged() = 0;
    virtual void OnFailedToProcessBuffer() = 0;
    virtual void OnStreamConfigured() = 0;
    virtual void OnFrameRateChanged(uint32_t frame_rate) = 0;

   protected:
    Observer() = default;
    virtual ~Observer() = default;
  };

  static rtc::scoped_refptr<SharedScreenCastStream> CreateDefault();

  bool StartScreenCastStream(uint32_t stream_node_id);
  bool StartScreenCastStream(uint32_t stream_node_id,
                             int fd,
                             uint32_t width = 0,
                             uint32_t height = 0,
                             bool is_cursor_embedded = false,
                             DesktopCapturer::Callback* callback = nullptr);
  void UpdateScreenCastStreamResolution(uint32_t width, uint32_t height);
  void UpdateScreenCastStreamFrameRate(uint32_t frame_rate);
  void SetUseDamageRegion(bool use_damage_region);
  void SetObserver(SharedScreenCastStream::Observer* observer);
  void StopScreenCastStream();

  std::unique_ptr<SharedDesktopFrame> CaptureFrame();

  std::unique_ptr<MouseCursor> CaptureCursor();

  absl::optional<DesktopVector> CaptureCursorPosition();

  ~SharedScreenCastStream();

 protected:
  SharedScreenCastStream();

 private:
  friend class SharedScreenCastStreamPrivate;

  SharedScreenCastStream(const SharedScreenCastStream&) = delete;
  SharedScreenCastStream& operator=(const SharedScreenCastStream&) = delete;

  std::unique_ptr<SharedScreenCastStreamPrivate> private_;
};

}

#endif
