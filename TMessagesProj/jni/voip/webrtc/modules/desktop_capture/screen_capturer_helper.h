/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_HELPER_H_
#define MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_HELPER_H_

#include <memory>

#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/desktop_region.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class ScreenCapturerHelper {
 public:
  ScreenCapturerHelper() = default;
  ~ScreenCapturerHelper() = default;

  ScreenCapturerHelper(const ScreenCapturerHelper&) = delete;
  ScreenCapturerHelper& operator=(const ScreenCapturerHelper&) = delete;

  void ClearInvalidRegion();

  void InvalidateRegion(const DesktopRegion& invalid_region);

  void InvalidateScreen(const DesktopSize& size);

  void TakeInvalidRegion(DesktopRegion* invalid_region);

  const DesktopSize& size_most_recent() const;
  void set_size_most_recent(const DesktopSize& size);

  void SetLogGridSize(int log_grid_size);

  static void ExpandToGrid(const DesktopRegion& region,
                           int log_grid_size,
                           DesktopRegion* result);

 private:

  DesktopRegion invalid_region_ RTC_GUARDED_BY(invalid_region_mutex_);

  Mutex invalid_region_mutex_;

  DesktopSize size_most_recent_;

  int log_grid_size_ = 0;
};

}

#endif
