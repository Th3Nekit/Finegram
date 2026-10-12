/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains the PeerConnection interface as defined in
// https://w3c.github.io/webrtc-pc/#peer-to-peer-connections
//
// The PeerConnectionFactory class provides factory methods to create
// PeerConnection, MediaStream and MediaStreamTrack objects.
//
// The following steps are needed to setup a typical call using WebRTC:
//
// 1. Create a PeerConnectionFactoryInterface. Check constructors for more
// information about input parameters.
//
// 2. Create a PeerConnection object. Provide a configuration struct which
// points to STUN and/or TURN servers used to generate ICE candidates, and
// provide an object that implements the PeerConnectionObserver interface,
// which is used to receive callbacks from the PeerConnection.
//
// 3. Create local MediaStreamTracks using the PeerConnectionFactory and add
// them to PeerConnection by calling AddTrack (or legacy method, AddStream).
//
// 4. Create an offer, call SetLocalDescription with it, serialize it, and send
// it to the remote peer
//
// 5. Once an ICE candidate has been gathered, the PeerConnection will call the
// observer function OnIceCandidate. The candidates must also be serialized and
// sent to the remote peer.
//
// 6. Once an answer is received from the remote peer, call
// SetRemoteDescription with the remote answer.
//
// 7. Once a remote candidate is received from the remote peer, provide it to
// the PeerConnection by calling AddIceCandidate.
//
// The receiver of a call (assuming the application is "call"-based) can decide
// to accept or reject the call; this decision will be taken by the application,
// not the PeerConnection.
//
// If the application decides to accept the call, it should:
//
// 1. Create PeerConnectionFactoryInterface if it doesn't exist.
//
// 2. Create a new PeerConnection.
//
// 3. Provide the remote offer to the new PeerConnection object by calling
// SetRemoteDescription.
//
// 4. Generate an answer to the remote offer by calling CreateAnswer and send it
// back to the remote peer.
//
// 5. Provide the local answer to the new PeerConnection by calling
// SetLocalDescription with the answer.
//
// 6. Provide the remote ICE candidates by calling AddIceCandidate.
//
// 7. Once a candidate has been gathered, the PeerConnection will call the
// observer function OnIceCandidate. Send these candidates to the remote peer.

#ifndef API_PEER_CONNECTION_INTERFACE_H_
#define API_PEER_CONNECTION_INTERFACE_H_

#include <stdint.h>
#include <stdio.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/adaptation/resource.h"
#include "api/async_dns_resolver.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/candidate.h"
#include "api/crypto/crypto_options.h"
#include "api/data_channel_interface.h"
#include "api/dtls_transport_interface.h"
#include "api/fec_controller.h"
#include "api/field_trials_view.h"
#include "api/ice_transport_interface.h"
#include "api/jsep.h"
#include "api/legacy_stats_types.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/metronome/metronome.h"
#include "api/neteq/neteq_factory.h"
#include "api/network_state_predictor.h"
#include "api/packet_socket_factory.h"
#include "api/rtc_error.h"
#include "api/rtc_event_log/rtc_event_log_factory_interface.h"
#include "api/rtc_event_log_output.h"
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/sctp_transport_interface.h"
#include "api/set_local_description_observer_interface.h"
#include "api/set_remote_description_observer_interface.h"
#include "api/stats/rtc_stats_collector_callback.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/transport/bandwidth_estimation_settings.h"
#include "api/transport/bitrate_settings.h"
#include "api/transport/enums.h"
#include "api/transport/network_control.h"
#include "api/transport/sctp_transport_factory_interface.h"
#include "api/turn_customizer.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "call/rtp_transport_controller_send_factory_interface.h"
#include "media/base/media_config.h"
#include "media/base/media_engine.h"

#include "api/ref_count.h"
#include "p2p/base/port_allocator.h"
#include "rtc_base/network.h"
#include "rtc_base/network_constants.h"
#include "rtc_base/network_monitor_factory.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread.h"

namespace rtc {
class Thread;
}

namespace webrtc {

class MediaFactory;

class StreamCollectionInterface : public webrtc::RefCountInterface {
 public:

  virtual size_t count() = 0;
  virtual MediaStreamInterface* at(size_t index) = 0;
  virtual MediaStreamInterface* find(const std::string& label) = 0;
  virtual MediaStreamTrackInterface* FindAudioTrack(const std::string& id) = 0;
  virtual MediaStreamTrackInterface* FindVideoTrack(const std::string& id) = 0;

 protected:

  ~StreamCollectionInterface() override = default;
};

class StatsObserver : public webrtc::RefCountInterface {
 public:
  virtual void OnComplete(const StatsReports& reports) = 0;

 protected:
  ~StatsObserver() override = default;
};

enum class SdpSemantics {

  kPlanB_DEPRECATED,
  kPlanB [[deprecated]] = kPlanB_DEPRECATED,
  kUnifiedPlan,
};

class RTC_EXPORT PeerConnectionInterface : public webrtc::RefCountInterface {
 public:

  enum SignalingState {
    kStable,
    kHaveLocalOffer,
    kHaveLocalPrAnswer,
    kHaveRemoteOffer,
    kHaveRemotePrAnswer,
    kClosed,
  };
  static constexpr absl::string_view AsString(SignalingState);

  enum IceGatheringState {
    kIceGatheringNew,
    kIceGatheringGathering,
    kIceGatheringComplete
  };
  static constexpr absl::string_view AsString(IceGatheringState state);

  enum class PeerConnectionState {
    kNew,
    kConnecting,
    kConnected,
    kDisconnected,
    kFailed,
    kClosed,
  };
  static constexpr absl::string_view AsString(PeerConnectionState state);

  enum IceConnectionState {
    kIceConnectionNew,
    kIceConnectionChecking,
    kIceConnectionConnected,
    kIceConnectionCompleted,
    kIceConnectionFailed,
    kIceConnectionDisconnected,
    kIceConnectionClosed,
    kIceConnectionMax,
  };
  static constexpr absl::string_view AsString(IceConnectionState state);

  enum TlsCertPolicy {

    kTlsCertPolicySecure,

    kTlsCertPolicyInsecureNoCheck,
  };

  struct RTC_EXPORT IceServer {
    IceServer();
    IceServer(const IceServer&);
    ~IceServer();

    std::string uri;
    std::vector<std::string> urls;
    std::string username;
    std::string password;
    TlsCertPolicy tls_cert_policy = kTlsCertPolicySecure;

    std::string hostname;

    std::vector<std::string> tls_alpn_protocols;

    std::vector<std::string> tls_elliptic_curves;

    bool operator==(const IceServer& o) const {
      return uri == o.uri && urls == o.urls && username == o.username &&
             password == o.password && tls_cert_policy == o.tls_cert_policy &&
             hostname == o.hostname &&
             tls_alpn_protocols == o.tls_alpn_protocols &&
             tls_elliptic_curves == o.tls_elliptic_curves;
    }
    bool operator!=(const IceServer& o) const { return !(*this == o); }
  };
  typedef std::vector<IceServer> IceServers;

  enum IceTransportsType {

    kNone,
    kRelay,
    kNoHost,
    kAll
  };

  enum BundlePolicy {
    kBundlePolicyBalanced,
    kBundlePolicyMaxBundle,
    kBundlePolicyMaxCompat
  };

  enum RtcpMuxPolicy {
    kRtcpMuxPolicyNegotiate,
    kRtcpMuxPolicyRequire,
  };

  enum TcpCandidatePolicy {
    kTcpCandidatePolicyEnabled,
    kTcpCandidatePolicyDisabled
  };

  enum CandidateNetworkPolicy {
    kCandidateNetworkPolicyAll,
    kCandidateNetworkPolicyLowCost
  };

  enum ContinualGatheringPolicy { GATHER_ONCE, GATHER_CONTINUALLY };

  struct PortAllocatorConfig {

    int min_port = 0;
    int max_port = 0;
    uint32_t flags = 0;
  };

  enum class RTCConfigurationType {

    kSafe,

    kAggressive
  };

  struct RTC_EXPORT RTCConfiguration {

    RTCConfiguration();
    RTCConfiguration(const RTCConfiguration&);
    explicit RTCConfiguration(RTCConfigurationType type);
    ~RTCConfiguration();

    bool operator==(const RTCConfiguration& o) const;
    bool operator!=(const RTCConfiguration& o) const;

    bool dscp() const { return media_config.enable_dscp; }
    void set_dscp(bool enable) { media_config.enable_dscp = enable; }

    bool cpu_adaptation() const {
      return media_config.video.enable_cpu_adaptation;
    }
    void set_cpu_adaptation(bool enable) {
      media_config.video.enable_cpu_adaptation = enable;
    }

    bool suspend_below_min_bitrate() const {
      return media_config.video.suspend_below_min_bitrate;
    }
    void set_suspend_below_min_bitrate(bool enable) {
      media_config.video.suspend_below_min_bitrate = enable;
    }

    bool prerenderer_smoothing() const {
      return media_config.video.enable_prerenderer_smoothing;
    }
    void set_prerenderer_smoothing(bool enable) {
      media_config.video.enable_prerenderer_smoothing = enable;
    }

    bool experiment_cpu_load_estimator() const {
      return media_config.video.experiment_cpu_load_estimator;
    }
    void set_experiment_cpu_load_estimator(bool enable) {
      media_config.video.experiment_cpu_load_estimator = enable;
    }

    int audio_rtcp_report_interval_ms() const {
      return media_config.audio.rtcp_report_interval_ms;
    }
    void set_audio_rtcp_report_interval_ms(int audio_rtcp_report_interval_ms) {
      media_config.audio.rtcp_report_interval_ms =
          audio_rtcp_report_interval_ms;
    }

    int video_rtcp_report_interval_ms() const {
      return media_config.video.rtcp_report_interval_ms;
    }
    void set_video_rtcp_report_interval_ms(int video_rtcp_report_interval_ms) {
      media_config.video.rtcp_report_interval_ms =
          video_rtcp_report_interval_ms;
    }

    int min_port() const { return port_allocator_config.min_port; }
    void set_min_port(int port) { port_allocator_config.min_port = port; }
    int max_port() const { return port_allocator_config.max_port; }
    void set_max_port(int port) { port_allocator_config.max_port = port; }
    uint32_t port_allocator_flags() { return port_allocator_config.flags; }
    void set_port_allocator_flags(uint32_t flags) {
      port_allocator_config.flags = flags;
    }

    static const int kUndefined = -1;

    static const int kAudioJitterBufferMaxPackets = 200;

    static const int kAggressiveIceConnectionReceivingTimeout = 1000;

    IceServers servers;

    IceTransportsType type = kAll;
    BundlePolicy bundle_policy = kBundlePolicyBalanced;
    RtcpMuxPolicy rtcp_mux_policy = kRtcpMuxPolicyRequire;
    std::vector<rtc::scoped_refptr<rtc::RTCCertificate>> certificates;
    int ice_candidate_pool_size = 0;

    bool disable_ipv6_on_wifi = false;

    int max_ipv6_networks = cricket::kDefaultMaxIPv6Networks;

    bool disable_link_local_networks = false;

    absl::optional<int> screencast_min_bitrate;

    TcpCandidatePolicy tcp_candidate_policy = kTcpCandidatePolicyEnabled;

    CandidateNetworkPolicy candidate_network_policy =
        kCandidateNetworkPolicyAll;

    int audio_jitter_buffer_max_packets = kAudioJitterBufferMaxPackets;

    bool audio_jitter_buffer_fast_accelerate = false;

    int audio_jitter_buffer_min_delay_ms = 0;

    int ice_connection_receiving_timeout = kUndefined;

    int ice_backup_candidate_pair_ping_interval = kUndefined;

    ContinualGatheringPolicy continual_gathering_policy = GATHER_ONCE;

    bool prioritize_most_likely_ice_candidate_pairs = false;

    struct cricket::MediaConfig media_config;

    bool prune_turn_ports = false;

    PortPrunePolicy turn_port_prune_policy = NO_PRUNE;

    PortPrunePolicy GetTurnPortPrunePolicy() const {
      return prune_turn_ports ? PRUNE_BASED_ON_PRIORITY
                              : turn_port_prune_policy;
    }

    bool presume_writable_when_fully_relayed = false;

    bool enable_ice_renomination = false;

    bool redetermine_role_on_ice_restart = true;

    bool surface_ice_candidates_on_ice_transport_type_changed = false;

    absl::optional<int> ice_check_interval_strong_connectivity;
    absl::optional<int> ice_check_interval_weak_connectivity;
    absl::optional<int> ice_check_min_interval;

    absl::optional<int> ice_unwritable_timeout;

    absl::optional<int> ice_unwritable_min_checks;

    absl::optional<int> ice_inactive_timeout;

    absl::optional<int> stun_candidate_keepalive_interval;

    webrtc::TurnCustomizer* turn_customizer = nullptr;

    absl::optional<rtc::AdapterType> network_preference;

    SdpSemantics sdp_semantics = SdpSemantics::kUnifiedPlan;

    bool active_reset_srtp_params = false;

    absl::optional<CryptoOptions> crypto_options;

    bool offer_extmap_allow_mixed = true;

    std::string turn_logging_id;

    bool enable_implicit_rollback = false;

    absl::optional<int> report_usage_pattern_delay_ms;

    absl::optional<int> stable_writable_connection_ping_interval_ms;

    VpnPreference vpn_preference = VpnPreference::kDefault;

    std::vector<rtc::NetworkMask> vpn_list;

    PortAllocatorConfig port_allocator_config;

    absl::optional<TimeDelta> pacer_burst_interval;

  };

  struct RTCOfferAnswerOptions {
    static const int kUndefined = -1;
    static const int kMaxOfferToReceiveMedia = 1;

    static const int kOfferToReceiveMediaTrue = 1;

    int offer_to_receive_video = kUndefined;
    int offer_to_receive_audio = kUndefined;

    bool voice_activity_detection = true;
    bool ice_restart = false;

    bool use_rtp_mux = true;

    bool raw_packetization_for_video = false;

    int num_simulcast_layers = 1;

    bool use_obsolete_sctp_sdp = false;

    RTCOfferAnswerOptions() = default;

    RTCOfferAnswerOptions(int offer_to_receive_video,
                          int offer_to_receive_audio,
                          bool voice_activity_detection,
                          bool ice_restart,
                          bool use_rtp_mux)
        : offer_to_receive_video(offer_to_receive_video),
          offer_to_receive_audio(offer_to_receive_audio),
          voice_activity_detection(voice_activity_detection),
          ice_restart(ice_restart),
          use_rtp_mux(use_rtp_mux) {}
  };

  enum StatsOutputLevel {
    kStatsOutputLevelStandard,
    kStatsOutputLevelDebug,
  };

  virtual rtc::scoped_refptr<StreamCollectionInterface> local_streams() = 0;

  virtual rtc::scoped_refptr<StreamCollectionInterface> remote_streams() = 0;

  virtual bool AddStream(MediaStreamInterface* stream) = 0;

  virtual void RemoveStream(MediaStreamInterface* stream) = 0;

  virtual RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids) = 0;

  virtual RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids,
      const std::vector<RtpEncodingParameters>& init_send_encodings) = 0;

  virtual RTCError RemoveTrackOrError(
      rtc::scoped_refptr<RtpSenderInterface> sender) {
    RTC_CHECK_NOTREACHED();
    return RTCError();
  }

  virtual RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
  AddTransceiver(rtc::scoped_refptr<MediaStreamTrackInterface> track) = 0;
  virtual RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
  AddTransceiver(rtc::scoped_refptr<MediaStreamTrackInterface> track,
                 const RtpTransceiverInit& init) = 0;

  virtual RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
  AddTransceiver(cricket::MediaType media_type) = 0;
  virtual RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
  AddTransceiver(cricket::MediaType media_type,
                 const RtpTransceiverInit& init) = 0;

  virtual rtc::scoped_refptr<RtpSenderInterface> CreateSender(
      const std::string& kind,
      const std::string& stream_id) = 0;

  virtual std::vector<rtc::scoped_refptr<RtpSenderInterface>> GetSenders()
      const = 0;

  virtual std::vector<rtc::scoped_refptr<RtpReceiverInterface>> GetReceivers()
      const = 0;

  virtual std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>
  GetTransceivers() const = 0;

  virtual bool GetStats(StatsObserver* observer,
                        MediaStreamTrackInterface* track,
                        StatsOutputLevel level) = 0;

  virtual void GetStats(RTCStatsCollectorCallback* callback) = 0;

  virtual void GetStats(
      rtc::scoped_refptr<RtpSenderInterface> selector,
      rtc::scoped_refptr<RTCStatsCollectorCallback> callback) = 0;

  virtual void GetStats(
      rtc::scoped_refptr<RtpReceiverInterface> selector,
      rtc::scoped_refptr<RTCStatsCollectorCallback> callback) = 0;

  virtual void ClearStatsCache() {}

  virtual RTCErrorOr<rtc::scoped_refptr<DataChannelInterface>>
  CreateDataChannelOrError(const std::string& label,
                           const DataChannelInit* config) {
    return RTCError(RTCErrorType::INTERNAL_ERROR, "dummy function called");
  }

  ABSL_DEPRECATED("Use CreateDataChannelOrError")
  virtual rtc::scoped_refptr<DataChannelInterface> CreateDataChannel(
      const std::string& label,
      const DataChannelInit* config) {
    auto result = CreateDataChannelOrError(label, config);
    if (!result.ok()) {
      return nullptr;
    } else {
      return result.MoveValue();
    }
  }

  virtual const SessionDescriptionInterface* local_description() const = 0;
  virtual const SessionDescriptionInterface* remote_description() const = 0;

  virtual const SessionDescriptionInterface* current_local_description()
      const = 0;
  virtual const SessionDescriptionInterface* current_remote_description()
      const = 0;

  virtual const SessionDescriptionInterface* pending_local_description()
      const = 0;
  virtual const SessionDescriptionInterface* pending_remote_description()
      const = 0;

  virtual void RestartIce() = 0;

  virtual void CreateOffer(CreateSessionDescriptionObserver* observer,
                           const RTCOfferAnswerOptions& options) = 0;

  virtual void CreateAnswer(CreateSessionDescriptionObserver* observer,
                            const RTCOfferAnswerOptions& options) = 0;

  virtual void SetLocalDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer) {}

  virtual void SetLocalDescription(
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer) {}

  virtual void SetLocalDescription(SetSessionDescriptionObserver* observer,
                                   SessionDescriptionInterface* desc) = 0;
  virtual void SetLocalDescription(SetSessionDescriptionObserver* observer) {}

  virtual void SetRemoteDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer) = 0;

  virtual void SetRemoteDescription(SetSessionDescriptionObserver* observer,
                                    SessionDescriptionInterface* desc) {}

  virtual bool ShouldFireNegotiationNeededEvent(uint32_t event_id) {
    return true;
  }

  virtual PeerConnectionInterface::RTCConfiguration GetConfiguration() = 0;

  virtual RTCError SetConfiguration(
      const PeerConnectionInterface::RTCConfiguration& config) = 0;

  virtual bool AddIceCandidate(const IceCandidateInterface* candidate) = 0;

  virtual void AddIceCandidate(std::unique_ptr<IceCandidateInterface> candidate,
                               std::function<void(RTCError)> callback) {}

  virtual bool RemoveIceCandidates(
      const std::vector<cricket::Candidate>& candidates) = 0;

  virtual RTCError SetBitrate(const BitrateSettings& bitrate) = 0;

  virtual void ReconfigureBandwidthEstimation(
      const BandwidthEstimationSettings& settings) {}

  virtual void SetAudioPlayout(bool playout) {}

  virtual void SetAudioRecording(bool recording) {}

  virtual rtc::scoped_refptr<DtlsTransportInterface> LookupDtlsTransportByMid(
      const std::string& mid) = 0;

  virtual rtc::scoped_refptr<SctpTransportInterface> GetSctpTransport()
      const = 0;

  virtual SignalingState signaling_state() = 0;

  virtual IceConnectionState ice_connection_state() = 0;

  virtual IceConnectionState standardized_ice_connection_state() = 0;

  virtual PeerConnectionState peer_connection_state() = 0;

  virtual IceGatheringState ice_gathering_state() = 0;

  virtual absl::optional<bool> can_trickle_ice_candidates() {

    return absl::nullopt;
  }

  virtual void AddAdaptationResource(rtc::scoped_refptr<Resource> resource) {}

  virtual bool StartRtcEventLog(std::unique_ptr<RtcEventLogOutput> output,
                                int64_t output_period_ms) = 0;
  virtual bool StartRtcEventLog(std::unique_ptr<RtcEventLogOutput> output) = 0;

  virtual void StopRtcEventLog() = 0;

  virtual void Close() = 0;

  virtual rtc::Thread* signaling_thread() const { return nullptr; }

 protected:

  ~PeerConnectionInterface() override = default;
};

class PeerConnectionObserver {
 public:
  virtual ~PeerConnectionObserver() = default;

  virtual void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) = 0;

  virtual void OnAddStream(rtc::scoped_refptr<MediaStreamInterface> stream) {}

  virtual void OnRemoveStream(rtc::scoped_refptr<MediaStreamInterface> stream) {
  }

  virtual void OnDataChannel(
      rtc::scoped_refptr<DataChannelInterface> data_channel) = 0;

  virtual void OnRenegotiationNeeded() {}

  virtual void OnNegotiationNeededEvent(uint32_t event_id) {}

  virtual void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) {}

  virtual void OnStandardizedIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) {}

  virtual void OnConnectionChange(
      PeerConnectionInterface::PeerConnectionState new_state) {}

  virtual void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) = 0;

  virtual void OnIceCandidate(const IceCandidateInterface* candidate) = 0;

  virtual void OnIceCandidateError(const std::string& address,
                                   int port,
                                   const std::string& url,
                                   int error_code,
                                   const std::string& error_text) {}

  virtual void OnIceCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates) {}

  virtual void OnIceConnectionReceivingChange(bool receiving) {}

  virtual void OnIceSelectedCandidatePairChanged(
      const cricket::CandidatePairChangeEvent& event) {}

  virtual void OnAddTrack(
      rtc::scoped_refptr<RtpReceiverInterface> receiver,
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) {}

  virtual void OnTrack(
      rtc::scoped_refptr<RtpTransceiverInterface> transceiver) {}

  virtual void OnRemoveTrack(
      rtc::scoped_refptr<RtpReceiverInterface> receiver) {}

  virtual void OnInterestingUsage(int usage_pattern) {}
};

struct RTC_EXPORT PeerConnectionDependencies final {
  explicit PeerConnectionDependencies(PeerConnectionObserver* observer_in);

  PeerConnectionDependencies(const PeerConnectionDependencies&) = delete;
  PeerConnectionDependencies& operator=(const PeerConnectionDependencies&) =
      delete;

  PeerConnectionDependencies(PeerConnectionDependencies&&);
  PeerConnectionDependencies& operator=(PeerConnectionDependencies&&) = default;
  ~PeerConnectionDependencies();

  PeerConnectionObserver* observer = nullptr;

  std::unique_ptr<cricket::PortAllocator> allocator;

  std::unique_ptr<webrtc::AsyncDnsResolverFactoryInterface>
      async_dns_resolver_factory;
  std::unique_ptr<webrtc::IceTransportFactory> ice_transport_factory;
  std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator;
  std::unique_ptr<rtc::SSLCertificateVerifier> tls_cert_verifier;
  std::unique_ptr<webrtc::VideoBitrateAllocatorFactory>
      video_bitrate_allocator_factory;

  std::unique_ptr<FieldTrialsView> trials;
};

struct RTC_EXPORT PeerConnectionFactoryDependencies final {
  PeerConnectionFactoryDependencies();

  PeerConnectionFactoryDependencies(const PeerConnectionFactoryDependencies&) =
      delete;
  PeerConnectionFactoryDependencies& operator=(
      const PeerConnectionFactoryDependencies&) = delete;

  PeerConnectionFactoryDependencies(PeerConnectionFactoryDependencies&&);
  PeerConnectionFactoryDependencies& operator=(
      PeerConnectionFactoryDependencies&&) = default;
  ~PeerConnectionFactoryDependencies();

  rtc::Thread* network_thread = nullptr;
  rtc::Thread* worker_thread = nullptr;
  rtc::Thread* signaling_thread = nullptr;
  rtc::SocketFactory* socket_factory = nullptr;

  std::unique_ptr<rtc::PacketSocketFactory> packet_socket_factory;
  std::unique_ptr<TaskQueueFactory> task_queue_factory;
  std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory;
  std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory;
  std::unique_ptr<NetworkStatePredictorFactoryInterface>
      network_state_predictor_factory;
  std::unique_ptr<NetworkControllerFactoryInterface> network_controller_factory;

  std::unique_ptr<rtc::NetworkManager> network_manager;

  std::unique_ptr<rtc::NetworkMonitorFactory> network_monitor_factory;
  std::unique_ptr<NetEqFactory> neteq_factory;
  std::unique_ptr<SctpTransportFactoryInterface> sctp_factory;
  std::unique_ptr<FieldTrialsView> trials;
  std::unique_ptr<RtpTransportControllerSendFactoryInterface>
      transport_controller_send_factory;

  std::unique_ptr<Metronome> decode_metronome;

  std::unique_ptr<Metronome> encode_metronome;

  rtc::scoped_refptr<AudioDeviceModule> adm;
  rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory;
  rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory;
  rtc::scoped_refptr<AudioMixer> audio_mixer;
  rtc::scoped_refptr<AudioProcessing> audio_processing;
  std::unique_ptr<AudioFrameProcessor> audio_frame_processor;
  std::unique_ptr<VideoEncoderFactory> video_encoder_factory;
  std::unique_ptr<VideoDecoderFactory> video_decoder_factory;

  std::unique_ptr<MediaFactory> media_factory;
};

class RTC_EXPORT PeerConnectionFactoryInterface
    : public webrtc::RefCountInterface {
 public:
  class Options {
   public:
    Options() {}

    bool disable_encryption = false;

    bool disable_network_monitor = false;

    int network_ignore_mask = rtc::kDefaultNetworkIgnoreMask;

    rtc::SSLProtocolVersion ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;

    CryptoOptions crypto_options = {};
  };

  virtual void SetOptions(const Options& options) = 0;

  virtual RTCErrorOr<rtc::scoped_refptr<PeerConnectionInterface>>
  CreatePeerConnectionOrError(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      PeerConnectionDependencies dependencies);

  ABSL_DEPRECATED("Use CreatePeerConnectionOrError")
  virtual rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      PeerConnectionDependencies dependencies);

  ABSL_DEPRECATED("Use CreatePeerConnectionOrError")
  virtual rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      std::unique_ptr<cricket::PortAllocator> allocator,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      PeerConnectionObserver* observer);

  virtual RtpCapabilities GetRtpSenderCapabilities(
      cricket::MediaType kind) const;

  virtual RtpCapabilities GetRtpReceiverCapabilities(
      cricket::MediaType kind) const;

  virtual rtc::scoped_refptr<MediaStreamInterface> CreateLocalMediaStream(
      const std::string& stream_id) = 0;

  virtual rtc::scoped_refptr<AudioSourceInterface> CreateAudioSource(
      const cricket::AudioOptions& options) = 0;

  virtual rtc::scoped_refptr<VideoTrackInterface> CreateVideoTrack(
      rtc::scoped_refptr<VideoTrackSourceInterface> source,
      absl::string_view label) = 0;
  ABSL_DEPRECATED("Use version with scoped_refptr")
  virtual rtc::scoped_refptr<VideoTrackInterface> CreateVideoTrack(
      const std::string& label,
      VideoTrackSourceInterface* source) {
    return CreateVideoTrack(
        rtc::scoped_refptr<VideoTrackSourceInterface>(source), label);
  }

  virtual rtc::scoped_refptr<AudioTrackInterface> CreateAudioTrack(
      const std::string& label,
      AudioSourceInterface* source) = 0;

  virtual bool StartAecDump(FILE* file, int64_t max_size_bytes) {
    return false;
  }

  virtual void StopAecDump() = 0;

 protected:

  PeerConnectionFactoryInterface() {}
  ~PeerConnectionFactoryInterface() override = default;
};

RTC_EXPORT rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreateModularPeerConnectionFactory(
    PeerConnectionFactoryDependencies dependencies);

inline constexpr absl::string_view PeerConnectionInterface::AsString(
    SignalingState state) {
  switch (state) {
    case SignalingState::kStable:
      return "stable";
    case SignalingState::kHaveLocalOffer:
      return "have-local-offer";
    case SignalingState::kHaveLocalPrAnswer:
      return "have-local-pranswer";
    case SignalingState::kHaveRemoteOffer:
      return "have-remote-offer";
    case SignalingState::kHaveRemotePrAnswer:
      return "have-remote-pranswer";
    case SignalingState::kClosed:
      return "closed";
  }

  return "";
}

inline constexpr absl::string_view PeerConnectionInterface::AsString(
    IceGatheringState state) {
  switch (state) {
    case IceGatheringState::kIceGatheringNew:
      return "new";
    case IceGatheringState::kIceGatheringGathering:
      return "gathering";
    case IceGatheringState::kIceGatheringComplete:
      return "complete";
  }

  return "";
}

inline constexpr absl::string_view PeerConnectionInterface::AsString(
    PeerConnectionState state) {
  switch (state) {
    case PeerConnectionState::kNew:
      return "new";
    case PeerConnectionState::kConnecting:
      return "connecting";
    case PeerConnectionState::kConnected:
      return "connected";
    case PeerConnectionState::kDisconnected:
      return "disconnected";
    case PeerConnectionState::kFailed:
      return "failed";
    case PeerConnectionState::kClosed:
      return "closed";
  }

  return "";
}

inline constexpr absl::string_view PeerConnectionInterface::AsString(
    IceConnectionState state) {
  switch (state) {
    case kIceConnectionNew:
      return "new";
    case kIceConnectionChecking:
      return "checking";
    case kIceConnectionConnected:
      return "connected";
    case kIceConnectionCompleted:
      return "completed";
    case kIceConnectionFailed:
      return "failed";
    case kIceConnectionDisconnected:
      return "disconnected";
    case kIceConnectionClosed:
      return "closed";
    case kIceConnectionMax:

      return "";
  }

  return "";
}

}

#endif
