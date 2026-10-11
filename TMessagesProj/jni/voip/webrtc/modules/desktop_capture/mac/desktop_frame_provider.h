/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MAC_DESKTOP_FRAME_PROVIDER_H_
#define MODULES_DESKTOP_CAPTURE_MAC_DESKTOP_FRAME_PROVIDER_H_

#include <CoreGraphics/CoreGraphics.h>
#include <IOSurface/IOSurface.h>

#include <map>
#include <memory>

#include "api/sequence_checker.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "sdk/objc/helpers/scoped_cftyperef.h"

namespace webrtc {

class DesktopFrameProvider {
 public:
  explicit DesktopFrameProvider(bool allow_iosurface);
  ~DesktopFrameProvider();

  DesktopFrameProvider(const DesktopFrameProvider&) = delete;
  DesktopFrameProvider& operator=(const DesktopFrameProvider&) = delete;

  std::unique_ptr<DesktopFrame> TakeLatestFrameForDisplay(
      CGDirectDisplayID display_id);

  void InvalidateIOSurface(CGDirectDisplayID display_id,
                           rtc::ScopedCFTypeRef<IOSurfaceRef> io_surface);

  void Release();

  bool allow_iosurface() const { return allow_iosurface_; }

 private:
  SequenceChecker thread_checker_;
  const bool allow_iosurface_;

  std::map<CGDirectDisplayID, std::unique_ptr<SharedDesktopFrame>> io_surfaces_;
};

}

#endif
