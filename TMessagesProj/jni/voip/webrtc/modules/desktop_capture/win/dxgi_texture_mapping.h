/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_DXGI_TEXTURE_MAPPING_H_
#define MODULES_DESKTOP_CAPTURE_WIN_DXGI_TEXTURE_MAPPING_H_

#include <d3d11.h>
#include <dxgi1_2.h>

#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/win/dxgi_texture.h"

namespace webrtc {

class DxgiTextureMapping : public DxgiTexture {
 public:

  explicit DxgiTextureMapping(IDXGIOutputDuplication* duplication);

  ~DxgiTextureMapping() override;

 protected:
  bool CopyFromTexture(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                       ID3D11Texture2D* texture) override;

  bool DoRelease() override;

 private:
  IDXGIOutputDuplication* const duplication_;
};

}

#endif
