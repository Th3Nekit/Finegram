/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_DECODER_H_
#define API_VIDEO_CODECS_VIDEO_DECODER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/video/encoded_image.h"
#include "api/video/render_resolution.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT DecodedImageCallback {
 public:
  virtual ~DecodedImageCallback() {}

  virtual int32_t Decoded(VideoFrame& decodedImage) = 0;

  virtual int32_t Decoded(VideoFrame& decodedImage, int64_t decode_time_ms);

  virtual void Decoded(VideoFrame& decodedImage,
                       absl::optional<int32_t> decode_time_ms,
                       absl::optional<uint8_t> qp);
};

class RTC_EXPORT VideoDecoder {
 public:
  struct DecoderInfo {

    std::string implementation_name;

    bool is_hardware_accelerated = false;

    std::string ToString() const;
    bool operator==(const DecoderInfo& rhs) const;
    bool operator!=(const DecoderInfo& rhs) const { return !(*this == rhs); }
  };

  class Settings {
   public:
    Settings() = default;
    Settings(const Settings&) = default;
    Settings& operator=(const Settings&) = default;
    ~Settings() = default;

    absl::optional<int> buffer_pool_size() const;
    void set_buffer_pool_size(absl::optional<int> value);

    RenderResolution max_render_resolution() const;
    void set_max_render_resolution(RenderResolution value);

    int number_of_cores() const { return number_of_cores_; }
    void set_number_of_cores(int value);

    VideoCodecType codec_type() const { return codec_type_; }
    void set_codec_type(VideoCodecType value) { codec_type_ = value; }

   private:
    absl::optional<int> buffer_pool_size_;
    RenderResolution max_resolution_;
    int number_of_cores_ = 1;
    VideoCodecType codec_type_ = kVideoCodecGeneric;
  };

  virtual ~VideoDecoder() = default;

  virtual bool Configure(const Settings& settings) = 0;

  virtual int32_t Decode(const EncodedImage& input_image,
                         int64_t render_time_ms) {
    return Decode(input_image,                   false, render_time_ms);
  }

  virtual int32_t Decode(const EncodedImage& input_image,
                         bool missing_frames,
                         int64_t render_time_ms) {
    return Decode(input_image, render_time_ms);
  }

  virtual int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) = 0;

  virtual int32_t Release() = 0;

  virtual DecoderInfo GetDecoderInfo() const;

  virtual const char* ImplementationName() const;
};

inline absl::optional<int> VideoDecoder::Settings::buffer_pool_size() const {
  return buffer_pool_size_;
}

inline void VideoDecoder::Settings::set_buffer_pool_size(
    absl::optional<int> value) {
  buffer_pool_size_ = value;
}

inline RenderResolution VideoDecoder::Settings::max_render_resolution() const {
  return max_resolution_;
}

inline void VideoDecoder::Settings::set_max_render_resolution(
    RenderResolution value) {
  max_resolution_ = value;
}

}

#endif
