/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_AUDIO_ENCODER_FACTORY_TEMPLATE_H_
#define API_AUDIO_CODECS_AUDIO_ENCODER_FACTORY_TEMPLATE_H_

#include <memory>
#include <vector>

#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/field_trials_view.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"

namespace webrtc {

namespace audio_encoder_factory_template_impl {

template <typename... Ts>
struct Helper;

template <>
struct Helper<> {
  static void AppendSupportedEncoders(std::vector<AudioCodecSpec>* specs) {}
  static absl::optional<AudioCodecInfo> QueryAudioEncoder(
      const SdpAudioFormat& format) {
    return absl::nullopt;
  }
  static std::unique_ptr<AudioEncoder> MakeAudioEncoder(
      int payload_type,
      const SdpAudioFormat& format,
      absl::optional<AudioCodecPairId> codec_pair_id,
      const FieldTrialsView* field_trials) {
    return nullptr;
  }
};

template <typename T, typename... Ts>
struct Helper<T, Ts...> {
  static void AppendSupportedEncoders(std::vector<AudioCodecSpec>* specs) {
    T::AppendSupportedEncoders(specs);
    Helper<Ts...>::AppendSupportedEncoders(specs);
  }
  static absl::optional<AudioCodecInfo> QueryAudioEncoder(
      const SdpAudioFormat& format) {
    auto opt_config = T::SdpToConfig(format);
    static_assert(std::is_same<decltype(opt_config),
                               absl::optional<typename T::Config>>::value,
                  "T::SdpToConfig() must return a value of type "
                  "absl::optional<T::Config>");
    return opt_config ? absl::optional<AudioCodecInfo>(
                            T::QueryAudioEncoder(*opt_config))
                      : Helper<Ts...>::QueryAudioEncoder(format);
  }
  static std::unique_ptr<AudioEncoder> MakeAudioEncoder(
      int payload_type,
      const SdpAudioFormat& format,
      absl::optional<AudioCodecPairId> codec_pair_id,
      const FieldTrialsView* field_trials) {
    auto opt_config = T::SdpToConfig(format);
    if (opt_config) {
      return T::MakeAudioEncoder(*opt_config, payload_type, codec_pair_id);
    } else {
      return Helper<Ts...>::MakeAudioEncoder(payload_type, format,
                                             codec_pair_id, field_trials);
    }
  }
};

template <typename... Ts>
class AudioEncoderFactoryT : public AudioEncoderFactory {
 public:
  explicit AudioEncoderFactoryT(const FieldTrialsView* field_trials) {
    field_trials_ = field_trials;
  }

  std::vector<AudioCodecSpec> GetSupportedEncoders() override {
    std::vector<AudioCodecSpec> specs;
    Helper<Ts...>::AppendSupportedEncoders(&specs);
    return specs;
  }

  absl::optional<AudioCodecInfo> QueryAudioEncoder(
      const SdpAudioFormat& format) override {
    return Helper<Ts...>::QueryAudioEncoder(format);
  }

  std::unique_ptr<AudioEncoder> MakeAudioEncoder(
      int payload_type,
      const SdpAudioFormat& format,
      absl::optional<AudioCodecPairId> codec_pair_id) override {
    return Helper<Ts...>::MakeAudioEncoder(payload_type, format, codec_pair_id,
                                           field_trials_);
  }

  const FieldTrialsView* field_trials_;
};

}

template <typename... Ts>
rtc::scoped_refptr<AudioEncoderFactory> CreateAudioEncoderFactory(
    const FieldTrialsView* field_trials = nullptr) {

  static_assert(sizeof...(Ts) >= 1,
                "Caller must give at least one template parameter");

  return rtc::make_ref_counted<
      audio_encoder_factory_template_impl::AudioEncoderFactoryT<Ts...>>(
      field_trials);
}

}

#endif
