/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_DXGI_OUTPUT_DUPLICATOR_H_
#define MODULES_DESKTOP_CAPTURE_WIN_DXGI_OUTPUT_DUPLICATOR_H_

#include <comdef.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <vector>

#include "modules/desktop_capture/desktop_frame_rotation.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "modules/desktop_capture/win/d3d_device.h"
#include "modules/desktop_capture/win/dxgi_context.h"
#include "modules/desktop_capture/win/dxgi_texture.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class DxgiOutputDuplicator {
 public:
  using Context = DxgiOutputContext;

  DxgiOutputDuplicator(const D3dDevice& device,
                       const Microsoft::WRL::ComPtr<IDXGIOutput1>& output,
                       const DXGI_OUTPUT_DESC& desc);

  DxgiOutputDuplicator(DxgiOutputDuplicator&& other);

  ~DxgiOutputDuplicator();

  bool Initialize();

  bool Duplicate(Context* context,
                 DesktopVector offset,
                 SharedDesktopFrame* target);

  DesktopRect desktop_rect() const { return desktop_rect_; }

  const std::string& device_name() const { return device_name_; }

  void Setup(Context* context);

  void Unregister(const Context* const context);

  int64_t num_frames_captured() const;

  void TranslateRect(const DesktopVector& position);

 private:

  void DetectUpdatedRegion(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                           DesktopRegion* updated_region);

  bool DoDetectUpdatedRegion(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                             DesktopRegion* updated_region);

  bool ContainsMouseCursor(const DXGI_OUTDUPL_FRAME_INFO& frame_info);

  bool ReleaseFrame();

  bool DuplicateOutput();

  DesktopRect GetTranslatedDesktopRect(DesktopVector offset) const;

  DesktopRect GetUntranslatedDesktopRect() const;

  void SpreadContextChange(const Context* const context);

  DesktopSize desktop_size() const;

  const D3dDevice device_;
  const Microsoft::WRL::ComPtr<IDXGIOutput1> output_;
  const std::string device_name_;
  DesktopRect desktop_rect_;
  Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication_;
  DXGI_OUTDUPL_DESC desc_;
  std::vector<uint8_t> metadata_;
  std::unique_ptr<DxgiTexture> texture_;
  Rotation rotation_;
  DesktopSize unrotated_size_;

  std::vector<Context*> contexts_;

  std::unique_ptr<SharedDesktopFrame> last_frame_;
  DesktopVector last_frame_offset_;

  int64_t num_frames_captured_ = 0;
};

}

#endif
