/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_VP8_INCLUDE_VP8_H_
#define MODULES_VIDEO_CODING_CODECS_VP8_INCLUDE_VP8_H_

#include <memory>
#include <vector>

#include "api/environment/environment.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/vp8_frame_buffer_controller.h"
#include "modules/video_coding/include/video_codec_interface.h"

namespace webrtc {

class VP8Encoder {
 public:
  struct Settings {

    std::unique_ptr<Vp8FrameBufferControllerFactory>
        frame_buffer_controller_factory = nullptr;

    std::vector<VideoEncoder::ResolutionBitrateLimits>
        resolution_bitrate_limits = {};
  };

  static std::unique_ptr<VideoEncoder> Create();
  static std::unique_ptr<VideoEncoder> Create(Settings settings);
};

class VP8Decoder {
 public:
  static std::unique_ptr<VideoDecoder> Create();
};

std::unique_ptr<VideoDecoder> CreateVp8Decoder(const Environment& env);

}

#endif
