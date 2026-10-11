/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_PCLF_MEDIA_QUALITY_TEST_PARAMS_H_
#define API_TEST_PCLF_MEDIA_QUALITY_TEST_PARAMS_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "api/async_dns_resolver.h"
#include "api/audio/audio_mixer.h"
#include "api/fec_controller.h"
#include "api/field_trials_view.h"
#include "api/rtc_event_log/rtc_event_log_factory_interface.h"
#include "api/test/pclf/media_configuration.h"
#include "api/transport/network_control.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "p2p/base/port_allocator.h"
#include "rtc_base/network.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace webrtc_pc_e2e {

struct PeerConnectionFactoryComponents {
  std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory;
  std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory;
  std::unique_ptr<NetworkControllerFactoryInterface> network_controller_factory;
  std::unique_ptr<NetEqFactory> neteq_factory;

  std::unique_ptr<VideoEncoderFactory> video_encoder_factory;
  std::unique_ptr<VideoDecoderFactory> video_decoder_factory;
  rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory;
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory;

  std::unique_ptr<FieldTrialsView> trials;

  rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing;
  rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer;
};

struct PeerConnectionComponents {
  PeerConnectionComponents(rtc::NetworkManager* network_manager,
                           rtc::PacketSocketFactory* packet_socket_factory)
      : network_manager(network_manager),
        packet_socket_factory(packet_socket_factory) {
    RTC_CHECK(network_manager);
  }

  rtc::NetworkManager* const network_manager;
  rtc::PacketSocketFactory* const packet_socket_factory;
  std::unique_ptr<webrtc::AsyncDnsResolverFactoryInterface>
      async_dns_resolver_factory;
  std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator;
  std::unique_ptr<rtc::SSLCertificateVerifier> tls_cert_verifier;
  std::unique_ptr<IceTransportFactory> ice_transport_factory;
};

struct InjectableComponents {
  InjectableComponents(rtc::Thread* network_thread,
                       rtc::NetworkManager* network_manager,
                       rtc::PacketSocketFactory* packet_socket_factory)
      : network_thread(network_thread),
        worker_thread(nullptr),
        pcf_dependencies(std::make_unique<PeerConnectionFactoryComponents>()),
        pc_dependencies(
            std::make_unique<PeerConnectionComponents>(network_manager,
                                                       packet_socket_factory)) {
    RTC_CHECK(network_thread);
  }

  rtc::Thread* const network_thread;
  rtc::Thread* worker_thread;

  std::unique_ptr<PeerConnectionFactoryComponents> pcf_dependencies;
  std::unique_ptr<PeerConnectionComponents> pc_dependencies;
};

struct Params {

  absl::optional<std::string> name;

  absl::optional<AudioConfig> audio_config;

  uint32_t port_allocator_extra_flags = cricket::kDefaultPortAllocatorFlags;

  absl::optional<std::string> rtc_event_log_path;

  absl::optional<std::string> aec_dump_path;

  bool use_ulp_fec = false;
  bool use_flex_fec = false;

  double video_encoder_bitrate_multiplier = 1.0;

  PeerConnectionFactoryInterface::Options peer_connection_factory_options;
  PeerConnectionInterface::RTCConfiguration rtc_configuration;
  PeerConnectionInterface::RTCOfferAnswerOptions rtc_offer_answer_options;
  BitrateSettings bitrate_settings;
  std::vector<VideoCodecConfig> video_codecs;

  std::vector<std::string> extra_video_rtp_header_extensions;

  std::vector<std::string> extra_audio_rtp_header_extensions;
};

struct ConfigurableParams {

  std::vector<VideoConfig> video_configs;

  VideoSubscription video_subscription =
      VideoSubscription().SubscribeToAllPeers();
};

struct RunParams {
  explicit RunParams(TimeDelta run_duration) : run_duration(run_duration) {}

  TimeDelta run_duration;

  bool enable_flex_fec_support = false;

  bool use_conference_mode = false;

  absl::optional<EchoEmulationConfig> echo_emulation_config;
};

}
}

#endif
