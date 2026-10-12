/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURER_H_
#define MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "modules/desktop_capture/delegated_source_list_controller.h"
#if defined(WEBRTC_USE_GIO)
#include "modules/desktop_capture/desktop_capture_metadata.h"
#endif
#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/shared_memory.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

void RTC_EXPORT LogDesktopCapturerFullscreenDetectorUsage();

class DesktopCaptureOptions;
class DesktopFrame;

class RTC_EXPORT DesktopCapturer {
 public:
  enum class Result {

    SUCCESS,

    ERROR_TEMPORARY,

    ERROR_PERMANENT,

    MAX_VALUE = ERROR_PERMANENT
  };

  class Callback {
   public:

    virtual void OnFrameCaptureStart() {}

    virtual void OnCaptureResult(Result result,
                                 std::unique_ptr<DesktopFrame> frame) = 0;

   protected:
    virtual ~Callback() {}
  };

#if defined(CHROMEOS)
  typedef int64_t SourceId;
#else
  typedef intptr_t SourceId;
#endif

  static_assert(std::is_same<SourceId, ScreenId>::value,
                "SourceId should be a same type as ScreenId.");

  struct Source {

    SourceId id;

    std::string title;

#if defined(CHROMEOS)

    WindowId in_process_id = kNullWindowId;
#endif

    int64_t display_id = kInvalidDisplayId;
  };

  typedef std::vector<Source> SourceList;

  virtual ~DesktopCapturer();

  virtual void Start(Callback* callback) = 0;

  virtual void SetMaxFrameRate(uint32_t max_frame_rate) {}

  virtual DelegatedSourceListController* GetDelegatedSourceListController();

  virtual void SetSharedMemoryFactory(
      std::unique_ptr<SharedMemoryFactory> shared_memory_factory);

  virtual void CaptureFrame() = 0;

  virtual void SetExcludedWindow(WindowId window);

  virtual bool GetSourceList(SourceList* sources);

  virtual bool SelectSource(SourceId id);

  virtual bool FocusOnSelectedSource();

  virtual bool IsOccluded(const DesktopVector& pos);

  static std::unique_ptr<DesktopCapturer> CreateWindowCapturer(
      const DesktopCaptureOptions& options);

  static std::unique_ptr<DesktopCapturer> CreateScreenCapturer(
      const DesktopCaptureOptions& options);

  static std::unique_ptr<DesktopCapturer> CreateGenericCapturer(
      const DesktopCaptureOptions& options);

#if defined(WEBRTC_USE_PIPEWIRE) || defined(WEBRTC_USE_X11)
  static bool IsRunningUnderWayland();

  virtual void UpdateResolution(uint32_t width, uint32_t height) {}
#endif

#if defined(WEBRTC_USE_GIO)

  virtual DesktopCaptureMetadata GetMetadata() { return {}; }
#endif

 protected:

  static std::unique_ptr<DesktopCapturer> CreateRawWindowCapturer(
      const DesktopCaptureOptions& options);

  static std::unique_ptr<DesktopCapturer> CreateRawScreenCapturer(
      const DesktopCaptureOptions& options);
};

}

#endif
