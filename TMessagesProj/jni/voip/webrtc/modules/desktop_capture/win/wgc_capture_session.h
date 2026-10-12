/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURE_SESSION_H_
#define MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURE_SESSION_H_

#include <d3d11.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.h>
#include <wrl/client.h>

#include <memory>

#include "api/sequence_checker.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/screen_capture_frame_queue.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "modules/desktop_capture/win/wgc_capture_source.h"
#include "rtc_base/event.h"

namespace webrtc {

class WgcCaptureSession final {
 public:
  WgcCaptureSession(
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
      Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::Capture::IGraphicsCaptureItem> item,
      ABI::Windows::Graphics::SizeInt32 size);

  WgcCaptureSession(const WgcCaptureSession&) = delete;
  WgcCaptureSession& operator=(const WgcCaptureSession&) = delete;

  ~WgcCaptureSession();

  HRESULT StartCapture(const DesktopCaptureOptions& options);

  bool GetFrame(std::unique_ptr<DesktopFrame>* output_frame,
                bool source_should_be_capturable);

  bool IsCaptureStarted() const {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return is_capture_started_;
  }

  static constexpr int kNumBuffers = 2;

 private:

  HRESULT CreateMappedTexture(
      Microsoft::WRL::ComPtr<ID3D11Texture2D> src_texture,
      UINT width = 0,
      UINT height = 0);

  HRESULT OnItemClosed(
      ABI::Windows::Graphics::Capture::IGraphicsCaptureItem* sender,
      IInspectable* event_args);

  void EnsureFrame();

  HRESULT ProcessFrame();

  void RemoveEventHandler();

  bool FrameContentCanBeCompared();

  bool allow_zero_hertz() const { return allow_zero_hertz_; }

  std::unique_ptr<EventRegistrationToken> item_closed_token_;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;

  Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>
      item_;

  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>
      direct3d_device_;

  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePool>
      frame_pool_;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> mapped_texture_;

  ABI::Windows::Graphics::SizeInt32 size_;

  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Capture::IGraphicsCaptureSession>
      session_;

  ScreenCaptureFrameQueue<SharedDesktopFrame> queue_;

  bool item_closed_ = false;
  bool is_capture_started_ = false;

  bool allow_zero_hertz_ = false;

  DesktopRegion damage_region_;

  SequenceChecker sequence_checker_;
};

}

#endif
