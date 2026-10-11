/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_NV12_BUFFER_H_
#define API_VIDEO_NV12_BUFFER_H_

#include <memory>
#include <utility>

#include "api/scoped_refptr.h"
#include "api/video/video_frame_buffer.h"
#include "rtc_base/memory/aligned_malloc.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT NV12Buffer : public NV12BufferInterface {
 public:
  static rtc::scoped_refptr<NV12Buffer> Create(int width, int height);
  static rtc::scoped_refptr<NV12Buffer> Create(int width,
                                               int height,
                                               int stride_y,
                                               int stride_uv);
  static rtc::scoped_refptr<NV12Buffer> Copy(
      const I420BufferInterface& i420_buffer);

  rtc::scoped_refptr<I420BufferInterface> ToI420() override;

  int width() const override;
  int height() const override;

  int StrideY() const override;
  int StrideUV() const override;

  const uint8_t* DataY() const override;
  const uint8_t* DataUV() const override;

  uint8_t* MutableDataY();
  uint8_t* MutableDataUV();

  void InitializeData();

  void CropAndScaleFrom(const NV12BufferInterface& src,
                        int offset_x,
                        int offset_y,
                        int crop_width,
                        int crop_height);

 protected:
  NV12Buffer(int width, int height);
  NV12Buffer(int width, int height, int stride_y, int stride_uv);

  ~NV12Buffer() override;

 private:
  size_t UVOffset() const;

  const int width_;
  const int height_;
  const int stride_y_;
  const int stride_uv_;
  const std::unique_ptr<uint8_t, AlignedFreeDeleter> data_;
};

}

#endif
