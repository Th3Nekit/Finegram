/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_I210_BUFFER_H_
#define API_VIDEO_I210_BUFFER_H_

#include <stdint.h>

#include <memory>

#include "api/scoped_refptr.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "rtc_base/memory/aligned_malloc.h"

namespace webrtc {

class I210Buffer : public I210BufferInterface {
 public:

  static rtc::scoped_refptr<I210Buffer> Create(int width, int height);

  static rtc::scoped_refptr<I210Buffer> Copy(const I210BufferInterface& buffer);

  static rtc::scoped_refptr<I210Buffer> Copy(const I420BufferInterface& buffer);

  static rtc::scoped_refptr<I210Buffer> Rotate(const I210BufferInterface& src,
                                               VideoRotation rotation);

  rtc::scoped_refptr<I420BufferInterface> ToI420() override;

  int width() const override;
  int height() const override;
  const uint16_t* DataY() const override;
  const uint16_t* DataU() const override;
  const uint16_t* DataV() const override;
  int StrideY() const override;
  int StrideU() const override;
  int StrideV() const override;

  uint16_t* MutableDataY();
  uint16_t* MutableDataU();
  uint16_t* MutableDataV();

  void CropAndScaleFrom(const I210BufferInterface& src,
                        int offset_x,
                        int offset_y,
                        int crop_width,
                        int crop_height);

  void ScaleFrom(const I210BufferInterface& src);

 protected:
  I210Buffer(int width, int height, int stride_y, int stride_u, int stride_v);
  ~I210Buffer() override;

 private:
  const int width_;
  const int height_;
  const int stride_y_;
  const int stride_u_;
  const int stride_v_;
  const std::unique_ptr<uint16_t, AlignedFreeDeleter> data_;
};

}

#endif
