/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef MODULES_VIDEO_CODING_CODECS_VP9_INCLUDE_VP9_H_
#define MODULES_VIDEO_CODING_CODECS_VP9_INCLUDE_VP9_H_

#include <memory>
#include <vector>

#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/sdp_video_format.h"
#include "media/base/codec.h"
#include "modules/video_coding/include/video_codec_interface.h"

namespace webrtc {

std::vector<SdpVideoFormat> SupportedVP9Codecs(
    bool add_scalability_modes = false);

std::vector<SdpVideoFormat> SupportedVP9DecoderCodecs();

class VP9Encoder : public VideoEncoder {
 public:

  static std::unique_ptr<VP9Encoder> Create();

  static std::unique_ptr<VP9Encoder> Create(const cricket::VideoCodec& codec);
  static bool SupportsScalabilityMode(ScalabilityMode scalability_mode);

  ~VP9Encoder() override {}
};

class VP9Decoder : public VideoDecoder {
 public:
  static std::unique_ptr<VP9Decoder> Create();

  ~VP9Decoder() override {}
};
}

#endif
