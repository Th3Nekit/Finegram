/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_DXGI_TEXTURE_STAGING_H_
#define MODULES_DESKTOP_CAPTURE_WIN_DXGI_TEXTURE_STAGING_H_

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/win/d3d_device.h"
#include "modules/desktop_capture/win/dxgi_texture.h"

namespace webrtc {

class DxgiTextureStaging : public DxgiTexture {
 public:

  explicit DxgiTextureStaging(const D3dDevice& device);

  ~DxgiTextureStaging() override;

 protected:

  bool CopyFromTexture(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                       ID3D11Texture2D* texture) override;

  bool DoRelease() override;

 private:

  bool InitializeStage(ID3D11Texture2D* texture);

  void AssertStageAndSurfaceAreSameObject();

  const DesktopRect desktop_rect_;
  const D3dDevice device_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> stage_;
  Microsoft::WRL::ComPtr<IDXGISurface> surface_;
};

}

#endif
