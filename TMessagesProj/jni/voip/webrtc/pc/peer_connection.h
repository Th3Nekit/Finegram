/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_PEER_CONNECTION_H_
#define PC_PEER_CONNECTION_H_

#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/adaptation/resource.h"
#include "api/async_dns_resolver.h"
#include "api/candidate.h"
#include "api/crypto/crypto_options.h"
#include "api/data_channel_interface.h"
#include "api/dtls_transport_interface.h"
#include "api/environment/environment.h"
#include "api/field_trials_view.h"
#include "api/ice_transport_interface.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/rtc_event_log_output.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/sctp_transport_interface.h"
#include "api/sequence_checker.h"
#include "api/set_local_description_observer_interface.h"
#include "api/set_remote_description_observer_interface.h"
#include "api/stats/rtc_stats_collector_callback.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/transport/bitrate_settings.h"
#include "api/transport/data_channel_transport_interface.h"
#include "api/transport/enums.h"
#include "api/turn_customizer.h"
#include "call/call.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/port.h"
#include "p2p/base/port_allocator.h"
#include "p2p/base/transport_description.h"
#include "pc/channel_interface.h"
#include "pc/connection_context.h"
#include "pc/data_channel_controller.h"
#include "pc/data_channel_utils.h"
#include "pc/dtls_transport.h"
#include "pc/jsep_transport_controller.h"
#include "pc/legacy_stats_collector.h"
#include "pc/peer_connection_internal.h"
#include "pc/peer_connection_message_handler.h"
#include "pc/rtc_stats_collector.h"
#include "pc/rtp_transceiver.h"
#include "pc/rtp_transmission_manager.h"
#include "pc/rtp_transport_internal.h"
#include "pc/sctp_data_channel.h"
#include "pc/sdp_offer_answer.h"
#include "pc/session_description.h"
#include "pc/transceiver_list.h"
#include "pc/transport_stats.h"
#include "pc/usage_pattern.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {

class PeerConnection : public PeerConnectionInternal,
                       public JsepTransportController::Observer {
 public:

  static RTCErrorOr<rtc::scoped_refptr<PeerConnection>> Create(
      const Environment& env,
      rtc::scoped_refptr<ConnectionContext> context,
      const PeerConnectionFactoryInterface::Options& options,
      std::unique_ptr<Call> call,
      const PeerConnectionInterface::RTCConfiguration& configuration,
      PeerConnectionDependencies dependencies);

  rtc::scoped_refptr<StreamCollectionInterface> local_streams() override;
  rtc::scoped_refptr<StreamCollectionInterface> remote_streams() override;
  bool AddStream(MediaStreamInterface* local_stream) override;
  void RemoveStream(MediaStreamInterface* local_stream) override;

  RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids) override;
  RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids,
      const std::vector<RtpEncodingParameters>& init_send_encodings) override;
  RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids,
      const std::vector<RtpEncodingParameters>* init_send_encodings);
  RTCError RemoveTrackOrError(
      rtc::scoped_refptr<RtpSenderInterface> sender) override;

  RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> AddTransceiver(
      rtc::scoped_refptr<MediaStreamTrackInterface> track) override;
  RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> AddTransceiver(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const RtpTransceiverInit& init) override;
  RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> AddTransceiver(
      cricket::MediaType media_type) override;
  RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> AddTransceiver(
      cricket::MediaType media_type,
      const RtpTransceiverInit& init) override;

  rtc::scoped_refptr<RtpSenderInterface> CreateSender(
      const std::string& kind,
      const std::string& stream_id) override;

  std::vector<rtc::scoped_refptr<RtpSenderInterface>> GetSenders()
      const override;
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> GetReceivers()
      const override;
  std::vector<rtc::scoped_refptr<RtpTransceiverInterface>> GetTransceivers()
      const override;

  RTCErrorOr<rtc::scoped_refptr<DataChannelInterface>> CreateDataChannelOrError(
      const std::string& label,
      const DataChannelInit* config) override;

  bool GetStats(StatsObserver* observer,
                MediaStreamTrackInterface* track,
                StatsOutputLevel level) override;

  void GetStats(RTCStatsCollectorCallback* callback) override;
  void GetStats(
      rtc::scoped_refptr<RtpSenderInterface> selector,
      rtc::scoped_refptr<RTCStatsCollectorCallback> callback) override;
  void GetStats(
      rtc::scoped_refptr<RtpReceiverInterface> selector,
      rtc::scoped_refptr<RTCStatsCollectorCallback> callback) override;
  void ClearStatsCache() override;

  SignalingState signaling_state() override;

  IceConnectionState ice_connection_state() override;
  IceConnectionState ice_connection_state_internal() override {
    return ice_connection_state();
  }
  IceConnectionState standardized_ice_connection_state() override;
  PeerConnectionState peer_connection_state() override;
  IceGatheringState ice_gathering_state() override;
  absl::optional<bool> can_trickle_ice_candidates() override;

  const SessionDescriptionInterface* local_description() const override;
  const SessionDescriptionInterface* remote_description() const override;
  const SessionDescriptionInterface* current_local_description() const override;
  const SessionDescriptionInterface* current_remote_description()
      const override;
  const SessionDescriptionInterface* pending_local_description() const override;
  const SessionDescriptionInterface* pending_remote_description()
      const override;

  void RestartIce() override;

  void CreateOffer(CreateSessionDescriptionObserver* observer,
                   const RTCOfferAnswerOptions& options) override;
  void CreateAnswer(CreateSessionDescriptionObserver* observer,
                    const RTCOfferAnswerOptions& options) override;

  void SetLocalDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer)
      override;
  void SetLocalDescription(
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer)
      override;

  void SetLocalDescription(SetSessionDescriptionObserver* observer,
                           SessionDescriptionInterface* desc) override;
  void SetLocalDescription(SetSessionDescriptionObserver* observer) override;

  void SetRemoteDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer)
      override;

  void SetRemoteDescription(SetSessionDescriptionObserver* observer,
                            SessionDescriptionInterface* desc) override;

  PeerConnectionInterface::RTCConfiguration GetConfiguration() override;
  RTCError SetConfiguration(
      const PeerConnectionInterface::RTCConfiguration& configuration) override;
  bool AddIceCandidate(const IceCandidateInterface* candidate) override;
  void AddIceCandidate(std::unique_ptr<IceCandidateInterface> candidate,
                       std::function<void(RTCError)> callback) override;
  bool RemoveIceCandidates(
      const std::vector<cricket::Candidate>& candidates) override;

  RTCError SetBitrate(const BitrateSettings& bitrate) override;
  void ReconfigureBandwidthEstimation(
      const BandwidthEstimationSettings& settings) override;

  void SetAudioPlayout(bool playout) override;
  void SetAudioRecording(bool recording) override;

  rtc::scoped_refptr<DtlsTransportInterface> LookupDtlsTransportByMid(
      const std::string& mid) override;
  rtc::scoped_refptr<DtlsTransport> LookupDtlsTransportByMidInternal(
      const std::string& mid);

  rtc::scoped_refptr<SctpTransportInterface> GetSctpTransport() const override;

  void AddAdaptationResource(rtc::scoped_refptr<Resource> resource) override;

  bool StartRtcEventLog(std::unique_ptr<RtcEventLogOutput> output,
                        int64_t output_period_ms) override;
  bool StartRtcEventLog(std::unique_ptr<RtcEventLogOutput> output) override;
  void StopRtcEventLog() override;

  void Close() override;

  rtc::Thread* signaling_thread() const final {
    return context_->signaling_thread();
  }

  rtc::Thread* network_thread() const final {
    return context_->network_thread();
  }
  rtc::Thread* worker_thread() const final { return context_->worker_thread(); }

  std::string session_id() const override { return session_id_; }

  bool initial_offerer() const override {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return sdp_handler_->initial_offerer();
  }

  std::vector<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
  GetTransceiversInternal() const override {
    RTC_DCHECK_RUN_ON(signaling_thread());
    if (!ConfiguredForMedia()) {
      return {};
    }
    return rtp_manager()->transceivers()->List();
  }

  std::vector<DataChannelStats> GetDataChannelStats() const override;

  absl::optional<std::string> sctp_transport_name() const override;
  absl::optional<std::string> sctp_mid() const override;

  cricket::CandidateStatsList GetPooledCandidateStats() const override;
  std::map<std::string, cricket::TransportStats> GetTransportStatsByNames(
      const std::set<std::string>& transport_names) override;
  Call::Stats GetCallStats() override;

  absl::optional<AudioDeviceModule::Stats> GetAudioDeviceStats() override;

  bool GetLocalCertificate(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) override;
  std::unique_ptr<rtc::SSLCertChain> GetRemoteSSLCertChain(
      const std::string& transport_name) override;
  bool IceRestartPending(const std::string& content_name) const override;
  bool NeedsIceRestart(const std::string& content_name) const override;
  bool GetSslRole(const std::string& content_name, rtc::SSLRole* role) override;

  void NoteDataAddedEvent() override { NoteUsageEvent(UsageEvent::DATA_ADDED); }

  PeerConnectionObserver* Observer() const override;
  bool IsClosed() const override {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return !sdp_handler_ ||
           sdp_handler_->signaling_state() == PeerConnectionInterface::kClosed;
  }

  absl::optional<rtc::SSLRole> GetSctpSslRole_n() override;

  void OnSctpDataChannelStateChanged(
      int channel_id,
      DataChannelInterface::DataState state) override;

  bool ShouldFireNegotiationNeededEvent(uint32_t event_id) override;

  LegacyStatsCollector* legacy_stats() override {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return legacy_stats_.get();
  }
  DataChannelController* data_channel_controller() override {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return &data_channel_controller_;
  }
  bool dtls_enabled() const override {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return dtls_enabled_;
  }
  const PeerConnectionInterface::RTCConfiguration* configuration()
      const override {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return &configuration_;
  }
  PeerConnectionMessageHandler* message_handler() override {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return &message_handler_;
  }

  RtpTransmissionManager* rtp_manager() override { return rtp_manager_.get(); }
  const RtpTransmissionManager* rtp_manager() const override {
    return rtp_manager_.get();
  }

  JsepTransportController* transport_controller_s() override {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return transport_controller_copy_;
  }
  JsepTransportController* transport_controller_n() override {
    RTC_DCHECK_RUN_ON(network_thread());
    return transport_controller_.get();
  }
  cricket::PortAllocator* port_allocator() override {
    return port_allocator_.get();
  }
  Call* call_ptr() override { return call_ptr_; }

  ConnectionContext* context() { return context_.get(); }
  const PeerConnectionFactoryInterface::Options* options() const override {
    return &options_;
  }
  void SetIceConnectionState(IceConnectionState new_state) override;
  void NoteUsageEvent(UsageEvent event) override;

  void AddRemoteCandidate(const std::string& mid,
                          const cricket::Candidate& candidate) override;

  void ReportSdpBundleUsage(
      const SessionDescriptionInterface& remote_description) override;

  void ReportFirstConnectUsageMetrics() RTC_RUN_ON(signaling_thread());

  bool IsUnifiedPlan() const override {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return is_unified_plan_;
  }
  bool ValidateBundleSettings(
      const cricket::SessionDescription* desc,
      const std::map<std::string, const cricket::ContentGroup*>&
          bundle_groups_by_mid) override;

  bool CreateDataChannelTransport(absl::string_view mid) override;
  void DestroyDataChannelTransport(RTCError error) override;

  void StartSctpTransport(int local_port,
                          int remote_port,
                          int max_message_size) override;

  CryptoOptions GetCryptoOptions() override;

  RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>> AddTransceiver(
      cricket::MediaType media_type,
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const RtpTransceiverInit& init,
      bool fire_callback = true) override;

  bool SrtpRequired() const override;

  absl::optional<std::string> SetupDataChannelTransport_n(absl::string_view mid)
      RTC_RUN_ON(network_thread());
  void TeardownDataChannelTransport_n(RTCError error)
      RTC_RUN_ON(network_thread());

  const FieldTrialsView& trials() const override { return env_.field_trials(); }

  bool ConfiguredForMedia() const;

  void ReturnHistogramVeryQuicklyForTesting() {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return_histogram_very_quickly_ = true;
  }
  void RequestUsagePatternReportForTesting();

 protected:

  PeerConnection(const Environment& env,
                 rtc::scoped_refptr<ConnectionContext> context,
                 const PeerConnectionFactoryInterface::Options& options,
                 bool is_unified_plan,
                 std::unique_ptr<Call> call,
                 PeerConnectionDependencies& dependencies,
                 bool dtls_enabled);

  ~PeerConnection() override;

 private:
  RTCError Initialize(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      PeerConnectionDependencies dependencies);
  JsepTransportController* InitializeTransportController_n(
      const RTCConfiguration& configuration,
      const PeerConnectionDependencies& dependencies)
      RTC_RUN_ON(network_thread());

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  FindTransceiverBySender(rtc::scoped_refptr<RtpSenderInterface> sender)
      RTC_RUN_ON(signaling_thread());

  void SetStandardizedIceConnectionState(
      PeerConnectionInterface::IceConnectionState new_state)
      RTC_RUN_ON(signaling_thread());
  void SetConnectionState(
      PeerConnectionInterface::PeerConnectionState new_state)
      RTC_RUN_ON(signaling_thread());

  void OnIceGatheringChange(IceGatheringState new_state)
      RTC_RUN_ON(signaling_thread());

  void OnIceCandidate(std::unique_ptr<IceCandidateInterface> candidate)
      RTC_RUN_ON(signaling_thread());

  void OnIceCandidateError(const std::string& address,
                           int port,
                           const std::string& url,
                           int error_code,
                           const std::string& error_text)
      RTC_RUN_ON(signaling_thread());

  void OnIceCandidatesRemoved(const std::vector<cricket::Candidate>& candidates)
      RTC_RUN_ON(signaling_thread());

  void OnSelectedCandidatePairChanged(
      const cricket::CandidatePairChangeEvent& event)
      RTC_RUN_ON(signaling_thread());

  void OnNegotiationNeeded();

  struct InitializePortAllocatorResult {
    bool enable_ipv6;
  };
  InitializePortAllocatorResult InitializePortAllocator_n(
      const cricket::ServerAddresses& stun_servers,
      const std::vector<cricket::RelayServerConfig>& turn_servers,
      const RTCConfiguration& configuration);

  bool ReconfigurePortAllocator_n(
      const cricket::ServerAddresses& stun_servers,
      const std::vector<cricket::RelayServerConfig>& turn_servers,
      IceTransportsType type,
      int candidate_pool_size,
      PortPrunePolicy turn_port_prune_policy,
      TurnCustomizer* turn_customizer,
      absl::optional<int> stun_candidate_keepalive_interval,
      bool have_local_description);

  bool StartRtcEventLog_w(std::unique_ptr<RtcEventLogOutput> output,
                          int64_t output_period_ms);

  void StopRtcEventLog_w();

  static bool GetTransportDescription(
      const cricket::SessionDescription* description,
      const std::string& content_name,
      cricket::TransportDescription* info);

  bool GetLocalCandidateMediaIndex(const std::string& content_name,
                                   int* sdp_mline_index)
      RTC_RUN_ON(signaling_thread());

  void OnTransportControllerConnectionState(cricket::IceConnectionState state)
      RTC_RUN_ON(signaling_thread());
  void OnTransportControllerGatheringState(cricket::IceGatheringState state)
      RTC_RUN_ON(signaling_thread());
  void OnTransportControllerCandidatesGathered(
      const std::string& transport_name,
      const std::vector<cricket::Candidate>& candidates)
      RTC_RUN_ON(signaling_thread());
  void OnTransportControllerCandidateError(
      const cricket::IceCandidateErrorEvent& event)
      RTC_RUN_ON(signaling_thread());
  void OnTransportControllerCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates)
      RTC_RUN_ON(signaling_thread());
  void OnTransportControllerCandidateChanged(
      const cricket::CandidatePairChangeEvent& event)
      RTC_RUN_ON(signaling_thread());
  void OnTransportControllerDtlsHandshakeError(rtc::SSLHandshakeError error);

  void ReportTransportStats(std::vector<RtpTransceiverProxyRefPtr> transceivers)
      RTC_RUN_ON(network_thread());

  static void ReportBestConnectionState(const cricket::TransportStats& stats);

  static void ReportNegotiatedCiphers(
      bool dtls_enabled,
      const cricket::TransportStats& stats,
      const std::set<cricket::MediaType>& media_types);
  void ReportIceCandidateCollected(const cricket::Candidate& candidate)
      RTC_RUN_ON(signaling_thread());

  void ReportUsagePattern() const RTC_RUN_ON(signaling_thread());

  void ReportRemoteIceCandidateAdded(const cricket::Candidate& candidate);

  bool OnTransportChanged(
      const std::string& mid,
      RtpTransportInternal* rtp_transport,
      rtc::scoped_refptr<DtlsTransport> dtls_transport,
      DataChannelTransportInterface* data_channel_transport) override;

  void SetSctpTransportName(std::string sctp_transport_name);

  std::function<void(const rtc::CopyOnWriteBuffer& packet,
                     int64_t packet_time_us)>
  InitializeRtcpCallback();

  std::function<void(const RtpPacketReceived& parsed_packet)>
  InitializeUnDemuxablePacketHandler();

  const Environment env_;
  const rtc::scoped_refptr<ConnectionContext> context_;
  const PeerConnectionFactoryInterface::Options options_;
  PeerConnectionObserver* observer_ RTC_GUARDED_BY(signaling_thread()) =
      nullptr;

  const bool is_unified_plan_;

  IceConnectionState ice_connection_state_ RTC_GUARDED_BY(signaling_thread()) =
      kIceConnectionNew;
  PeerConnectionInterface::IceConnectionState standardized_ice_connection_state_
      RTC_GUARDED_BY(signaling_thread()) = kIceConnectionNew;
  PeerConnectionInterface::PeerConnectionState connection_state_
      RTC_GUARDED_BY(signaling_thread()) = PeerConnectionState::kNew;

  IceGatheringState ice_gathering_state_ RTC_GUARDED_BY(signaling_thread()) =
      kIceGatheringNew;
  PeerConnectionInterface::RTCConfiguration configuration_
      RTC_GUARDED_BY(signaling_thread());

  const std::unique_ptr<AsyncDnsResolverFactoryInterface>
      async_dns_resolver_factory_;
  std::unique_ptr<cricket::PortAllocator>
      port_allocator_;

  const std::unique_ptr<IceTransportFactory>
      ice_transport_factory_;

  const std::unique_ptr<rtc::SSLCertificateVerifier> tls_cert_verifier_
      RTC_GUARDED_BY(network_thread());

  std::unique_ptr<Call> call_ RTC_GUARDED_BY(worker_thread());
  ScopedTaskSafety signaling_thread_safety_;
  rtc::scoped_refptr<PendingTaskSafetyFlag> network_thread_safety_;
  rtc::scoped_refptr<PendingTaskSafetyFlag> worker_thread_safety_;

  Call* const call_ptr_;

  std::unique_ptr<LegacyStatsCollector> legacy_stats_
      RTC_GUARDED_BY(signaling_thread());
  rtc::scoped_refptr<RTCStatsCollector> stats_collector_
      RTC_GUARDED_BY(signaling_thread());

  const std::string session_id_;

  std::unique_ptr<JsepTransportController> transport_controller_
      RTC_GUARDED_BY(network_thread());
  JsepTransportController* transport_controller_copy_
      RTC_GUARDED_BY(signaling_thread()) = nullptr;

  absl::optional<std::string> sctp_mid_s_ RTC_GUARDED_BY(signaling_thread());
  absl::optional<std::string> sctp_mid_n_ RTC_GUARDED_BY(network_thread());
  std::string sctp_transport_name_s_ RTC_GUARDED_BY(signaling_thread());

  std::unique_ptr<SdpOfferAnswerHandler> sdp_handler_
      RTC_GUARDED_BY(signaling_thread()) RTC_PT_GUARDED_BY(signaling_thread());

  const bool dtls_enabled_;

  UsagePattern usage_pattern_ RTC_GUARDED_BY(signaling_thread());
  bool return_histogram_very_quickly_ RTC_GUARDED_BY(signaling_thread()) =
      false;

  DataChannelController data_channel_controller_;

  PeerConnectionMessageHandler message_handler_
      RTC_GUARDED_BY(signaling_thread());

  std::unique_ptr<RtpTransmissionManager> rtp_manager_;

  bool was_ever_connected_ RTC_GUARDED_BY(signaling_thread()) = false;

  rtc::WeakPtrFactory<PeerConnection> weak_factory_;
};

}

#endif
