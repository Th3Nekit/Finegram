/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef MODULES_VIDEO_CODING_CODECS_H264_H264_DECODER_IMPL_H_
#define MODULES_VIDEO_CODING_CODECS_H264_H264_DECODER_IMPL_H_

#ifdef WEBRTC_USE_H264

#if defined(WEBRTC_WIN) && !defined(__clang__)
#error "See: bugs.webrtc.org/9213#c13."
#endif

#include <memory>

#include "modules/video_coding/codecs/h264/include/h264.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

#include "common_video/h264/h264_bitstream_parser.h"
#include "common_video/include/video_frame_buffer_pool.h"

namespace webrtc {

struct AVCodecContextDeleter {
  void operator()(AVCodecContext* ptr) const { avcodec_free_context(&ptr); }
};
struct AVFrameDeleter {
  void operator()(AVFrame* ptr) const { av_frame_free(&ptr); }
};

class H264DecoderImpl : public H264Decoder {
 public:
  H264DecoderImpl();
  ~H264DecoderImpl() override;

  bool Configure(const Settings& settings) override;
  int32_t Release() override;

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;

  int32_t Decode(const EncodedImage& input_image,
                 bool                   ,
                 int64_t render_time_ms = -1) override;

  const char* ImplementationName() const override;

 private:

  static int AVGetBuffer2(AVCodecContext* context,
                          AVFrame* av_frame,
                          int flags);

  static void AVFreeBuffer2(void* opaque, uint8_t* data);

  bool IsInitialized() const;

  void ReportInit();
  void ReportError();

  VideoFrameBufferPool ffmpeg_buffer_pool_;
  std::unique_ptr<AVCodecContext, AVCodecContextDeleter> av_context_;
  std::unique_ptr<AVFrame, AVFrameDeleter> av_frame_;

  DecodedImageCallback* decoded_image_callback_;

  bool has_reported_init_;
  bool has_reported_error_;

  webrtc::H264BitstreamParser h264_bitstream_parser_;
};

}

#endif

#endif
