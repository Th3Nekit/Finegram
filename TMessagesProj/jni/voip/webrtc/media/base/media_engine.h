/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_MEDIA_ENGINE_H_
#define MEDIA_BASE_MEDIA_ENGINE_H_

#include <memory>
#include <string>
#include <vector>

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/crypto/crypto_options.h"
#include "api/field_trials_view.h"
#include "api/rtp_parameters.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "call/audio_state.h"
#include "media/base/codec.h"
#include "media/base/media_channel.h"
#include "media/base/media_channel_impl.h"
#include "media/base/media_config.h"
#include "media/base/video_common.h"
#include "rtc_base/system/file_wrapper.h"

namespace webrtc {
class AudioDeviceModule;
class AudioMixer;
class AudioProcessing;
class Call;
}

namespace cricket {

webrtc::RTCError CheckScalabilityModeValues(
    const webrtc::RtpParameters& new_parameters,
    rtc::ArrayView<cricket::Codec> codec_preferences,
    absl::optional<cricket::Codec> send_codec);

webrtc::RTCError CheckRtpParametersValues(
    const webrtc::RtpParameters& new_parameters,
    rtc::ArrayView<cricket::Codec> codec_preferences,
    absl::optional<cricket::Codec> send_codec);

webrtc::RTCError CheckRtpParametersInvalidModificationAndValues(
    const webrtc::RtpParameters& old_parameters,
    const webrtc::RtpParameters& new_parameters,
    rtc::ArrayView<cricket::Codec> codec_preferences,
    absl::optional<cricket::Codec> send_codec);

webrtc::RTCError CheckRtpParametersInvalidModificationAndValues(
    const webrtc::RtpParameters& old_parameters,
    const webrtc::RtpParameters& new_parameters);

struct RtpCapabilities {
  RtpCapabilities();
  ~RtpCapabilities();
  std::vector<webrtc::RtpExtension> header_extensions;
};

class RtpHeaderExtensionQueryInterface {
 public:
  virtual ~RtpHeaderExtensionQueryInterface() = default;

  virtual std::vector<webrtc::RtpHeaderExtensionCapability>
  GetRtpHeaderExtensions() const = 0;
};

class VoiceEngineInterface : public RtpHeaderExtensionQueryInterface {
 public:
  VoiceEngineInterface() = default;
  virtual ~VoiceEngineInterface() = default;

  VoiceEngineInterface(const VoiceEngineInterface&) = delete;
  VoiceEngineInterface& operator=(const VoiceEngineInterface&) = delete;

  virtual void Init() = 0;

  virtual rtc::scoped_refptr<webrtc::AudioState> GetAudioState() const = 0;

  virtual std::unique_ptr<VoiceMediaSendChannelInterface> CreateSendChannel(
      webrtc::Call* call,
      const MediaConfig& config,
      const AudioOptions& options,
      const webrtc::CryptoOptions& crypto_options,
      webrtc::AudioCodecPairId codec_pair_id) {

    RTC_CHECK_NOTREACHED();
    return nullptr;
  }

  virtual std::unique_ptr<VoiceMediaReceiveChannelInterface>
  CreateReceiveChannel(webrtc::Call* call,
                       const MediaConfig& config,
                       const AudioOptions& options,
                       const webrtc::CryptoOptions& crypto_options,
                       webrtc::AudioCodecPairId codec_pair_id) {

    RTC_CHECK_NOTREACHED();
    return nullptr;
  }

  virtual const std::vector<AudioCodec>& send_codecs() const = 0;
  virtual const std::vector<AudioCodec>& recv_codecs() const = 0;

  virtual bool StartAecDump(webrtc::FileWrapper file,
                            int64_t max_size_bytes) = 0;

  virtual void StopAecDump() = 0;

  virtual absl::optional<webrtc::AudioDeviceModule::Stats>
  GetAudioDeviceStats() = 0;
};

class VideoEngineInterface : public RtpHeaderExtensionQueryInterface {
 public:
  VideoEngineInterface() = default;
  virtual ~VideoEngineInterface() = default;

  VideoEngineInterface(const VideoEngineInterface&) = delete;
  VideoEngineInterface& operator=(const VideoEngineInterface&) = delete;

  virtual std::unique_ptr<VideoMediaSendChannelInterface> CreateSendChannel(
      webrtc::Call* call,
      const MediaConfig& config,
      const VideoOptions& options,
      const webrtc::CryptoOptions& crypto_options,
      webrtc::VideoBitrateAllocatorFactory* video_bitrate_allocator_factory) {

    RTC_CHECK_NOTREACHED();
    return nullptr;
  }

  virtual std::unique_ptr<VideoMediaReceiveChannelInterface>
  CreateReceiveChannel(webrtc::Call* call,
                       const MediaConfig& config,
                       const VideoOptions& options,
                       const webrtc::CryptoOptions& crypto_options) {

    RTC_CHECK_NOTREACHED();
    return nullptr;
  }

  virtual std::vector<VideoCodec> send_codecs() const = 0;
  virtual std::vector<VideoCodec> recv_codecs() const = 0;

  virtual std::vector<VideoCodec> send_codecs(bool include_rtx) const {
    RTC_DCHECK(include_rtx);
    return send_codecs();
  }
  virtual std::vector<VideoCodec> recv_codecs(bool include_rtx) const {
    RTC_DCHECK(include_rtx);
    return recv_codecs();
  }
};

class MediaEngineInterface {
 public:
  virtual ~MediaEngineInterface() {}

  virtual bool Init() = 0;

  virtual VoiceEngineInterface& voice() = 0;
  virtual VideoEngineInterface& video() = 0;
  virtual const VoiceEngineInterface& voice() const = 0;
  virtual const VideoEngineInterface& video() const = 0;
};

class CompositeMediaEngine : public MediaEngineInterface {
 public:
  CompositeMediaEngine(std::unique_ptr<webrtc::FieldTrialsView> trials,
                       std::unique_ptr<VoiceEngineInterface> audio_engine,
                       std::unique_ptr<VideoEngineInterface> video_engine);
  CompositeMediaEngine(std::unique_ptr<VoiceEngineInterface> audio_engine,
                       std::unique_ptr<VideoEngineInterface> video_engine);
  ~CompositeMediaEngine() override;

  bool Init() override;

  VoiceEngineInterface& voice() override;
  VideoEngineInterface& video() override;
  const VoiceEngineInterface& voice() const override;
  const VideoEngineInterface& video() const override;

 private:
  const std::unique_ptr<webrtc::FieldTrialsView> trials_;
  const std::unique_ptr<VoiceEngineInterface> voice_engine_;
  const std::unique_ptr<VideoEngineInterface> video_engine_;
};

webrtc::RtpParameters CreateRtpParametersWithOneEncoding();
webrtc::RtpParameters CreateRtpParametersWithEncodings(StreamParams sp);

std::vector<webrtc::RtpExtension> GetDefaultEnabledRtpHeaderExtensions(
    const RtpHeaderExtensionQueryInterface& query_interface);

}

#endif
