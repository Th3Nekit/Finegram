/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_DXGI_ADAPTER_DUPLICATOR_H_
#define MODULES_DESKTOP_CAPTURE_WIN_DXGI_ADAPTER_DUPLICATOR_H_

#include <wrl/client.h>

#include <vector>

#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "modules/desktop_capture/win/d3d_device.h"
#include "modules/desktop_capture/win/dxgi_context.h"
#include "modules/desktop_capture/win/dxgi_output_duplicator.h"

namespace webrtc {

class DxgiAdapterDuplicator {
 public:
  using Context = DxgiAdapterContext;

  explicit DxgiAdapterDuplicator(const D3dDevice& device);

  DxgiAdapterDuplicator(DxgiAdapterDuplicator&& other);

  ~DxgiAdapterDuplicator();

  bool Initialize();

  bool Duplicate(Context* context, SharedDesktopFrame* target);

  bool DuplicateMonitor(Context* context,
                        int monitor_id,
                        SharedDesktopFrame* target);

  DesktopRect desktop_rect() const { return desktop_rect_; }

  DesktopRect ScreenRect(int id) const;

  const std::string& GetDeviceName(int id) const;

  int screen_count() const;

  void Setup(Context* context);

  void Unregister(const Context* const context);

  int64_t GetNumFramesCaptured() const;

  void TranslateRect(const DesktopVector& position);

 private:
  bool DoInitialize();

  const D3dDevice device_;
  std::vector<DxgiOutputDuplicator> duplicators_;
  DesktopRect desktop_rect_;
};

}

#endif
