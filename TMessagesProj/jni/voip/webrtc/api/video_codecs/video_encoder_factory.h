/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_H_
#define API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/units/data_rate.h"
#include "api/video/render_resolution.h"
#include "api/video_codecs/sdp_video_format.h"

namespace webrtc {

class VideoEncoder;

class VideoEncoderFactory {
 public:
  struct CodecSupport {
    bool is_supported = false;
    bool is_power_efficient = false;
  };

  class EncoderSelectorInterface {
   public:
    virtual ~EncoderSelectorInterface() {}

    virtual void OnCurrentEncoder(const SdpVideoFormat& format) = 0;

    virtual absl::optional<SdpVideoFormat> OnAvailableBitrate(
        const DataRate& rate) = 0;

    virtual absl::optional<SdpVideoFormat> OnResolutionChange(
        const RenderResolution& resolution) {
      return absl::nullopt;
    }

    virtual absl::optional<SdpVideoFormat> OnEncoderBroken() = 0;
  };

  virtual std::vector<SdpVideoFormat> GetSupportedFormats() const = 0;

  virtual std::vector<SdpVideoFormat> GetImplementations() const {
    return GetSupportedFormats();
  }

  virtual CodecSupport QueryCodecSupport(
      const SdpVideoFormat& format,
      absl::optional<std::string> scalability_mode) const {

    CodecSupport codec_support;
    if (!scalability_mode) {
      codec_support.is_supported = format.IsCodecInList(GetSupportedFormats());
    }
    return codec_support;
  }

  virtual std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) = 0;

  virtual std::unique_ptr<EncoderSelectorInterface> GetEncoderSelector() const {
    return nullptr;
  }

  virtual ~VideoEncoderFactory() {}
};

}

#endif
