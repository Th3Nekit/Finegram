/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_DXGI_DUPLICATOR_CONTROLLER_H_
#define MODULES_DESKTOP_CAPTURE_WIN_DXGI_DUPLICATOR_CONTROLLER_H_

#include <d3dcommon.h>

#include <atomic>
#include <string>
#include <vector>

#include "api/scoped_refptr.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "modules/desktop_capture/win/d3d_device.h"
#include "modules/desktop_capture/win/display_configuration_monitor.h"
#include "modules/desktop_capture/win/dxgi_adapter_duplicator.h"
#include "modules/desktop_capture/win/dxgi_context.h"
#include "modules/desktop_capture/win/dxgi_frame.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT DxgiDuplicatorController {
 public:
  using Context = DxgiFrameContext;

  struct D3dInfo {

    D3D_FEATURE_LEVEL min_feature_level;
    D3D_FEATURE_LEVEL max_feature_level;

  };

  enum class Result {
    SUCCEEDED = 0,
    UNSUPPORTED_SESSION = 1,
    FRAME_PREPARE_FAILED = 2,
    INITIALIZATION_FAILED = 3,
    DUPLICATION_FAILED = 4,
    INVALID_MONITOR_ID = 5,
    MAX_VALUE = INVALID_MONITOR_ID
  };

  static std::string ResultName(Result result);

  static rtc::scoped_refptr<DxgiDuplicatorController> Instance();

  static bool IsCurrentSessionSupported();

  bool IsSupported();

  bool RetrieveD3dInfo(D3dInfo* info);

  Result Duplicate(DxgiFrame* frame);

  Result DuplicateMonitor(DxgiFrame* frame, int monitor_id);

  DesktopVector system_dpi();

  int ScreenCount();

  bool GetDeviceNames(std::vector<std::string>* output);

 private:

  friend void DxgiFrameContext::Reset();

  friend class webrtc::scoped_refptr<DxgiDuplicatorController>;

  DxgiDuplicatorController();

  ~DxgiDuplicatorController();

  void AddRef();
  void Release();

  Result DoDuplicate(DxgiFrame* frame, int monitor_id);

  void Unload();

  void Unregister(const Context* const context);

  bool Initialize() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool DoInitialize() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void Deinitialize() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool ContextExpired(const Context* const context) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void Setup(Context* context) RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool DoDuplicateUnlocked(Context* context,
                           int monitor_id,
                           SharedDesktopFrame* target)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool DoDuplicateAll(Context* context, SharedDesktopFrame* target)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool DoDuplicateOne(Context* context,
                      int monitor_id,
                      SharedDesktopFrame* target)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  int64_t GetNumFramesCaptured() const RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  DesktopSize desktop_size() const RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  DesktopRect ScreenRect(int id) const RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  int ScreenCountUnlocked() const RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void GetDeviceNamesUnlocked(std::vector<std::string>* output) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  DesktopSize SelectedDesktopSize(int monitor_id) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool EnsureFrameCaptured(Context* context, SharedDesktopFrame* target)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void TranslateRect() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  std::atomic_int refcount_;

  Mutex mutex_;

  int identity_ RTC_GUARDED_BY(mutex_) = 0;
  DesktopRect desktop_rect_ RTC_GUARDED_BY(mutex_);
  DesktopVector system_dpi_ RTC_GUARDED_BY(mutex_);
  std::vector<DxgiAdapterDuplicator> duplicators_ RTC_GUARDED_BY(mutex_);
  D3dInfo d3d_info_ RTC_GUARDED_BY(mutex_);
  DisplayConfigurationMonitor display_configuration_monitor_
      RTC_GUARDED_BY(mutex_);

  uint32_t succeeded_duplications_ RTC_GUARDED_BY(mutex_) = 0;
};

}

#endif
