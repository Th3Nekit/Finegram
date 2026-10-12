/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_MULTIPLEX_INCLUDE_AUGMENTED_VIDEO_FRAME_BUFFER_H_
#define MODULES_VIDEO_CODING_CODECS_MULTIPLEX_INCLUDE_AUGMENTED_VIDEO_FRAME_BUFFER_H_

#include <cstdint>
#include <memory>

#include "api/scoped_refptr.h"
#include "api/video/video_frame_buffer.h"

namespace webrtc {
class AugmentedVideoFrameBuffer : public VideoFrameBuffer {
 public:
  AugmentedVideoFrameBuffer(
      const rtc::scoped_refptr<VideoFrameBuffer>& video_frame_buffer,
      std::unique_ptr<uint8_t[]> augmenting_data,
      uint16_t augmenting_data_size);

  rtc::scoped_refptr<VideoFrameBuffer> GetVideoFrameBuffer() const;

  uint8_t* GetAugmentingData() const;

  uint16_t GetAugmentingDataSize() const;

  Type type() const final;

  int width() const final;

  int height() const final;

  rtc::scoped_refptr<I420BufferInterface> ToI420() final;

  const I420BufferInterface* GetI420() const final;

 private:
  uint16_t augmenting_data_size_;
  std::unique_ptr<uint8_t[]> augmenting_data_;
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer_;
};
}

#endif
