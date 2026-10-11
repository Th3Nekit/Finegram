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

#ifndef MODULES_VIDEO_CODING_CODECS_H264_INCLUDE_H264_H_
#define MODULES_VIDEO_CODING_CODECS_H264_INCLUDE_H264_H_

#include <memory>
#include <string>
#include <vector>

#include "api/video_codecs/h264_profile_level_id.h"
#include "api/video_codecs/scalability_mode.h"
#include "media/base/codec.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct SdpVideoFormat;

RTC_EXPORT SdpVideoFormat
CreateH264Format(H264Profile profile,
                 H264Level level,
                 const std::string& packetization_mode,
                 bool add_scalability_modes = false);

RTC_EXPORT void DisableRtcUseH264();

std::vector<SdpVideoFormat> SupportedH264Codecs(
    bool add_scalability_modes = false);

std::vector<SdpVideoFormat> SupportedH264DecoderCodecs();

class RTC_EXPORT H264Encoder : public VideoEncoder {
 public:
  static std::unique_ptr<H264Encoder> Create(const cricket::VideoCodec& codec);
  static std::unique_ptr<H264Encoder> Create();

  static bool IsSupported();
  static bool SupportsScalabilityMode(ScalabilityMode scalability_mode);

  ~H264Encoder() override {}
};

class RTC_EXPORT H264Decoder : public VideoDecoder {
 public:
  static std::unique_ptr<H264Decoder> Create();
  static bool IsSupported();

  ~H264Decoder() override {}
};

}

#endif
