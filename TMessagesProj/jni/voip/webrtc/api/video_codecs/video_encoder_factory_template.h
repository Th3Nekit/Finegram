/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_H_
#define API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/array_view.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/video_coding/svc/scalability_mode_util.h"

namespace webrtc {

template <typename... Ts>
class VideoEncoderFactoryTemplate : public VideoEncoderFactory {
 public:
  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    return GetSupportedFormatsInternal<Ts...>();
  }

  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) override {

    absl::optional<SdpVideoFormat> matched =
        FuzzyMatchSdpVideoFormat(GetSupportedFormats(), format);
    return CreateVideoEncoderInternal<Ts...>(matched.value_or(format));
  }

  CodecSupport QueryCodecSupport(
      const SdpVideoFormat& format,
      absl::optional<std::string> scalability_mode) const override {
    return QueryCodecSupportInternal<Ts...>(format, scalability_mode);
  }

 private:
  bool IsFormatInList(
      const SdpVideoFormat& format,
      rtc::ArrayView<const SdpVideoFormat> supported_formats) const {
    return absl::c_any_of(
        supported_formats, [&](const SdpVideoFormat& supported_format) {
          return supported_format.name == format.name &&
                 supported_format.parameters == format.parameters;
        });
  }

  template <typename V>
  bool IsScalabilityModeSupported(
      const absl::optional<std::string>& scalability_mode_string) const {
    if (!scalability_mode_string.has_value()) {
      return true;
    }
    absl::optional<ScalabilityMode> scalability_mode =
        ScalabilityModeFromString(*scalability_mode_string);
    return scalability_mode.has_value() &&
           V::IsScalabilityModeSupported(*scalability_mode);
  }

  template <typename V, typename... Vs>
  std::vector<SdpVideoFormat> GetSupportedFormatsInternal() const {
    auto supported_formats = V::SupportedFormats();

    if constexpr (sizeof...(Vs) > 0) {

      for (const auto& other_format : GetSupportedFormatsInternal<Vs...>()) {
        if (!IsFormatInList(other_format, supported_formats)) {
          supported_formats.push_back(other_format);
        }
      }
    }

    return supported_formats;
  }

  template <typename V, typename... Vs>
  std::unique_ptr<VideoEncoder> CreateVideoEncoderInternal(
      const SdpVideoFormat& format) {
    if (IsFormatInList(format, V::SupportedFormats())) {
      return V::CreateEncoder(format);
    }

    if constexpr (sizeof...(Vs) > 0) {
      return CreateVideoEncoderInternal<Vs...>(format);
    }

    return nullptr;
  }

  template <typename V, typename... Vs>
  CodecSupport QueryCodecSupportInternal(
      const SdpVideoFormat& format,
      const absl::optional<std::string>& scalability_mode) const {
    if (IsFormatInList(format, V::SupportedFormats())) {
      return {.is_supported = IsScalabilityModeSupported<V>(scalability_mode)};
    }

    if constexpr (sizeof...(Vs) > 0) {
      return QueryCodecSupportInternal<Vs...>(format, scalability_mode);
    }

    return {.is_supported = false};
  }
};

}

#endif
