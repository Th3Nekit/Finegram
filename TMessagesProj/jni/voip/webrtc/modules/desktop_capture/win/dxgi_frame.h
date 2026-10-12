/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_DXGI_FRAME_H_
#define MODULES_DESKTOP_CAPTURE_WIN_DXGI_FRAME_H_

#include <memory>
#include <vector>

#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/resolution_tracker.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "modules/desktop_capture/shared_memory.h"
#include "modules/desktop_capture/win/dxgi_context.h"

namespace webrtc {

class DxgiDuplicatorController;

class DxgiFrame final {
 public:
  using Context = DxgiFrameContext;

  explicit DxgiFrame(SharedMemoryFactory* factory);
  ~DxgiFrame();

  SharedDesktopFrame* frame() const;

 private:

  friend class DxgiDuplicatorController;

  bool Prepare(DesktopSize size, DesktopCapturer::SourceId source_id);

  Context* context();

  SharedMemoryFactory* const factory_;
  ResolutionTracker resolution_tracker_;
  DesktopCapturer::SourceId source_id_ = kFullDesktopScreenId;
  std::unique_ptr<SharedDesktopFrame> frame_;
  Context context_;
};

}

#endif
