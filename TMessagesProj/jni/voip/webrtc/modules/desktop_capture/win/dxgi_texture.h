/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_DXGI_TEXTURE_H_
#define MODULES_DESKTOP_CAPTURE_WIN_DXGI_TEXTURE_H_

#include <d3d11.h>
#include <dxgi1_2.h>

#include <memory>

#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"

namespace webrtc {

class DesktopRegion;

class DxgiTexture {
 public:

  DxgiTexture();

  virtual ~DxgiTexture();

  bool CopyFrom(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                IDXGIResource* resource);

  const DesktopSize& desktop_size() const { return desktop_size_; }

  uint8_t* bits() const { return static_cast<uint8_t*>(rect_.pBits); }

  int pitch() const { return static_cast<int>(rect_.Pitch); }

  bool Release();

  const DesktopFrame& AsDesktopFrame();

 protected:
  DXGI_MAPPED_RECT* rect();

  virtual bool CopyFromTexture(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                               ID3D11Texture2D* texture) = 0;

  virtual bool DoRelease() = 0;

 private:
  DXGI_MAPPED_RECT rect_ = {0};
  DesktopSize desktop_size_;
  std::unique_ptr<DesktopFrame> frame_;
};

}

#endif
