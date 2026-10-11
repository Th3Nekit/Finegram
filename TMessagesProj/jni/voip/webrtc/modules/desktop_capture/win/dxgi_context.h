/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_DXGI_CONTEXT_H_
#define MODULES_DESKTOP_CAPTURE_WIN_DXGI_CONTEXT_H_

#include <vector>

#include "modules/desktop_capture/desktop_region.h"

namespace webrtc {

struct DxgiOutputContext final {

  DesktopRegion updated_region;
};

struct DxgiAdapterContext final {
  DxgiAdapterContext();
  DxgiAdapterContext(const DxgiAdapterContext& other);
  ~DxgiAdapterContext();

  std::vector<DxgiOutputContext> contexts;
};

struct DxgiFrameContext final {
 public:
  DxgiFrameContext();

  ~DxgiFrameContext();

  void Reset();

  int controller_id = 0;

  std::vector<DxgiAdapterContext> contexts;
};

}

#endif
