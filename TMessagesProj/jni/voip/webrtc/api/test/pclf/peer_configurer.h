/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_PCLF_PEER_CONFIGURER_H_
#define API_TEST_PCLF_PEER_CONFIGURER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "api/async_dns_resolver.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/fec_controller.h"
#include "api/field_trials_view.h"
#include "api/ice_transport_interface.h"
#include "api/neteq/neteq_factory.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_event_log/rtc_event_log_factory_interface.h"
#include "api/scoped_refptr.h"
#include "api/test/frame_generator_interface.h"
#include "api/test/pclf/media_configuration.h"
#include "api/test/pclf/media_quality_test_params.h"
#include "api/test/peer_network_dependencies.h"
#include "api/transport/bitrate_settings.h"
#include "api/transport/network_control.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/ssl_certificate.h"

namespace webrtc {
namespace webrtc_pc_e2e {

class PeerConfigurer {
 public:
  using VideoSource =
      absl::variant<std::unique_ptr<test::FrameGeneratorInterface>,
                    CapturingDeviceIndex>;

  explicit PeerConfigurer(const PeerNetworkDependencies& network_dependencies);

  PeerConfigurer* SetName(absl::string_view name);

  PeerConfigurer* SetEventLogFactory(
      std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory);
  PeerConfigurer* SetFecControllerFactory(
      std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory);
  PeerConfigurer* SetNetworkControllerFactory(
      std::unique_ptr<NetworkControllerFactoryInterface>
          network_controller_factory);
  PeerConfigurer* SetVideoEncoderFactory(
      std::unique_ptr<VideoEncoderFactory> video_encoder_factory);
  PeerConfigurer* SetVideoDecoderFactory(
      std::unique_ptr<VideoDecoderFactory> video_decoder_factory);
  PeerConfigurer* SetAudioEncoderFactory(
      rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory);
  PeerConfigurer* SetAudioDecoderFactory(
      rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory);

  PeerConfigurer* SetNetEqFactory(std::unique_ptr<NetEqFactory> neteq_factory);
  PeerConfigurer* SetAudioProcessing(
      rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing);
  PeerConfigurer* SetAudioMixer(
      rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer);

  PeerConfigurer* SetUseNetworkThreadAsWorkerThread();

  PeerConfigurer* SetAsyncDnsResolverFactory(
      std::unique_ptr<webrtc::AsyncDnsResolverFactoryInterface>
          async_dns_resolver_factory);
  PeerConfigurer* SetRTCCertificateGenerator(
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator);
  PeerConfigurer* SetSSLCertificateVerifier(
      std::unique_ptr<rtc::SSLCertificateVerifier> tls_cert_verifier);
  PeerConfigurer* SetIceTransportFactory(
      std::unique_ptr<IceTransportFactory> factory);

  PeerConfigurer* SetPortAllocatorExtraFlags(uint32_t extra_flags);

  PeerConfigurer* AddVideoConfig(VideoConfig config);

  PeerConfigurer* AddVideoConfig(
      VideoConfig config,
      std::unique_ptr<test::FrameGeneratorInterface> generator);

  PeerConfigurer* AddVideoConfig(VideoConfig config,
                                 CapturingDeviceIndex capturing_device_index);

  PeerConfigurer* SetVideoSubscription(VideoSubscription subscription);

  PeerConfigurer* SetVideoCodecs(std::vector<VideoCodecConfig> video_codecs);

  PeerConfigurer* SetExtraVideoRtpHeaderExtensions(
      std::vector<std::string> extensions);

  PeerConfigurer* SetAudioConfig(AudioConfig config);

  PeerConfigurer* SetExtraAudioRtpHeaderExtensions(
      std::vector<std::string> extensions);

  PeerConfigurer* SetUseUlpFEC(bool value);

  PeerConfigurer* SetUseFlexFEC(bool value);

  PeerConfigurer* SetVideoEncoderBitrateMultiplier(double multiplier);

  PeerConfigurer* SetRtcEventLogPath(absl::string_view path);

  PeerConfigurer* SetAecDumpPath(absl::string_view path);
  PeerConfigurer* SetPCFOptions(
      PeerConnectionFactoryInterface::Options options);
  PeerConfigurer* SetRTCConfiguration(
      PeerConnectionInterface::RTCConfiguration configuration);
  PeerConfigurer* SetRTCOfferAnswerOptions(
      PeerConnectionInterface::RTCOfferAnswerOptions options);

  PeerConfigurer* SetBitrateSettings(BitrateSettings bitrate_settings);

  PeerConfigurer* SetFieldTrials(std::unique_ptr<FieldTrialsView> field_trials);

  std::unique_ptr<InjectableComponents> ReleaseComponents();

  std::unique_ptr<Params> ReleaseParams();

  std::unique_ptr<ConfigurableParams> ReleaseConfigurableParams();

  std::vector<VideoSource> ReleaseVideoSources();

  InjectableComponents* components() { return components_.get(); }
  Params* params() { return params_.get(); }
  ConfigurableParams* configurable_params() {
    return configurable_params_.get();
  }
  const Params& params() const { return *params_; }
  const ConfigurableParams& configurable_params() const {
    return *configurable_params_;
  }
  std::vector<VideoSource>* video_sources() { return &video_sources_; }

 private:
  std::unique_ptr<InjectableComponents> components_;
  std::unique_ptr<Params> params_;
  std::unique_ptr<ConfigurableParams> configurable_params_;
  std::vector<VideoSource> video_sources_;
};

}
}

#endif
