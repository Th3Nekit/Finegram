/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * WebRTC's wrapper to libyuv.
 */

#ifndef COMMON_VIDEO_LIBYUV_INCLUDE_WEBRTC_LIBYUV_H_
#define COMMON_VIDEO_LIBYUV_INCLUDE_WEBRTC_LIBYUV_H_

#include <stdint.h>
#include <stdio.h>

#include <vector>

#include "api/scoped_refptr.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

enum class VideoType {
  kUnknown,
  kI420,
  kIYUV,
  kRGB24,
  kBGR24,
  kARGB,
  kABGR,
  kRGB565,
  kYUY2,
  kYV12,
  kUYVY,
  kMJPEG,
  kBGRA,
  kNV12,
};

const double kPerfectPSNR = 48.0f;

size_t CalcBufferSize(VideoType type, int width, int height);

int ExtractBuffer(const rtc::scoped_refptr<I420BufferInterface>& input_frame,
                  size_t size,
                  uint8_t* buffer);
int ExtractBuffer(const VideoFrame& input_frame, size_t size, uint8_t* buffer);

int ConvertFromI420(const VideoFrame& src_frame,
                    VideoType dst_video_type,
                    int dst_sample_size,
                    uint8_t* dst_frame);

rtc::scoped_refptr<I420BufferInterface> ScaleVideoFrameBuffer(
    const I420BufferInterface& source,
    int dst_width,
    int dst_height);

double I420SSE(const I420BufferInterface& ref_buffer,
               const I420BufferInterface& test_buffer);

double I420PSNR(const VideoFrame* ref_frame, const VideoFrame* test_frame);
double I420PSNR(const I420BufferInterface& ref_buffer,
                const I420BufferInterface& test_buffer);

double I420WeightedPSNR(const I420BufferInterface& ref_buffer,
                        const I420BufferInterface& test_buffer);

double I420SSIM(const VideoFrame* ref_frame, const VideoFrame* test_frame);
double I420SSIM(const I420BufferInterface& ref_buffer,
                const I420BufferInterface& test_buffer);

void NV12Scale(uint8_t* tmp_buffer,
               const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_uv,
               int src_stride_uv,
               int src_width,
               int src_height,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_uv,
               int dst_stride_uv,
               int dst_width,
               int dst_height);

class RTC_EXPORT NV12ToI420Scaler {
 public:
  NV12ToI420Scaler();
  ~NV12ToI420Scaler();
  void NV12ToI420Scale(const uint8_t* src_y,
                       int src_stride_y,
                       const uint8_t* src_uv,
                       int src_stride_uv,
                       int src_width,
                       int src_height,
                       uint8_t* dst_y,
                       int dst_stride_y,
                       uint8_t* dst_u,
                       int dst_stride_u,
                       uint8_t* dst_v,
                       int dst_stride_v,
                       int dst_width,
                       int dst_height);

 private:
  std::vector<uint8_t> tmp_uv_planes_;
};

int ConvertVideoType(VideoType video_type);

}

#endif
