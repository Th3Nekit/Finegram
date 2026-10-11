/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_ENCODER_SETTINGS_H_
#define CALL_ADAPTATION_ENCODER_SETTINGS_H_

#include "absl/types/optional.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "video/config/video_encoder_config.h"

namespace webrtc {

class EncoderSettings {
 public:
  EncoderSettings(VideoEncoder::EncoderInfo encoder_info,
                  VideoEncoderConfig encoder_config,
                  VideoCodec video_codec);
  EncoderSettings(const EncoderSettings& other);
  EncoderSettings& operator=(const EncoderSettings& other);

  const VideoEncoder::EncoderInfo& encoder_info() const;

  const VideoEncoderConfig& encoder_config() const;

  const VideoCodec& video_codec() const;

 private:
  VideoEncoder::EncoderInfo encoder_info_;
  VideoEncoderConfig encoder_config_;
  VideoCodec video_codec_;
};

VideoCodecType GetVideoCodecTypeOrGeneric(
    const absl::optional<EncoderSettings>& settings);

}

#endif
