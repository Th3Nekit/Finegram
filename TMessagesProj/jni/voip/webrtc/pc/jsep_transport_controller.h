/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_JSEP_TRANSPORT_CONTROLLER_H_
#define PC_JSEP_TRANSPORT_CONTROLLER_H_

#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/types/optional.h"
#include "api/async_dns_resolver.h"
#include "api/candidate.h"
#include "api/crypto/crypto_options.h"
#include "api/ice_transport_factory.h"
#include "api/ice_transport_interface.h"
#include "api/jsep.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/transport/data_channel_transport_interface.h"
#include "api/transport/sctp_transport_factory_interface.h"
#include "media/sctp/sctp_transport_internal.h"
#include "p2p/base/dtls_transport.h"
#include "p2p/base/dtls_transport_factory.h"
#include "p2p/base/dtls_transport_internal.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/p2p_transport_channel.h"
#include "p2p/base/packet_transport_internal.h"
#include "p2p/base/port.h"
#include "p2p/base/port_allocator.h"
#include "p2p/base/transport_description.h"
#include "p2p/base/transport_info.h"
#include "pc/dtls_srtp_transport.h"
#include "pc/dtls_transport.h"
#include "pc/jsep_transport.h"
#include "pc/jsep_transport_collection.h"
#include "pc/rtp_transport.h"
#include "pc/rtp_transport_internal.h"
#include "pc/sctp_transport.h"
#include "pc/session_description.h"
#include "pc/srtp_transport.h"
#include "pc/transport_stats.h"
#include "rtc_base/callback_list.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/helpers.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"

namespace rtc {
class Thread;
class PacketTransportInternal;
}

namespace webrtc {

class JsepTransportController : public sigslot::has_slots<> {
 public:

  class Observer {
   public:
    virtual ~Observer() {}

    virtual bool OnTransportChanged(
        const std::string& mid,
        RtpTransportInternal* rtp_transport,
        rtc::scoped_refptr<DtlsTransport> dtls_transport,
        DataChannelTransportInterface* data_channel_transport) = 0;
  };

  struct Config {

    bool redetermine_role_on_ice_restart = true;
    rtc::SSLProtocolVersion ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;

    CryptoOptions crypto_options;
    PeerConnectionInterface::BundlePolicy bundle_policy =
        PeerConnectionInterface::kBundlePolicyBalanced;
    PeerConnectionInterface::RtcpMuxPolicy rtcp_mux_policy =
        PeerConnectionInterface::kRtcpMuxPolicyRequire;
    bool disable_encryption = false;
    bool enable_external_auth = false;

    IceTransportFactory* ice_transport_factory = nullptr;
    cricket::DtlsTransportFactory* dtls_transport_factory = nullptr;
    Observer* transport_observer = nullptr;

    absl::AnyInvocable<void(const rtc::CopyOnWriteBuffer& packet,
                            int64_t packet_time_us) const>
        rtcp_handler;
    absl::AnyInvocable<void(const RtpPacketReceived& parsed_packet) const>
        un_demuxable_packet_handler;

    bool active_reset_srtp_params = false;
    RtcEventLog* event_log = nullptr;

    SctpTransportFactoryInterface* sctp_factory = nullptr;
    std::function<void(rtc::SSLHandshakeError)> on_dtls_handshake_error_;

    const FieldTrialsView* field_trials;
  };

  JsepTransportController(
      rtc::Thread* network_thread,
      cricket::PortAllocator* port_allocator,
      AsyncDnsResolverFactoryInterface* async_dns_resolver_factory,
      Config config);
  virtual ~JsepTransportController();

  JsepTransportController(const JsepTransportController&) = delete;
  JsepTransportController& operator=(const JsepTransportController&) = delete;

  RTCError SetLocalDescription(SdpType type,
                               const cricket::SessionDescription* local_desc,
                               const cricket::SessionDescription* remote_desc);

  RTCError SetRemoteDescription(SdpType type,
                                const cricket::SessionDescription* local_desc,
                                const cricket::SessionDescription* remote_desc);

  RtpTransportInternal* GetRtpTransport(absl::string_view mid) const;
  cricket::DtlsTransportInternal* GetDtlsTransport(const std::string& mid);
  const cricket::DtlsTransportInternal* GetRtcpDtlsTransport(
      const std::string& mid) const;

  rtc::scoped_refptr<DtlsTransport> LookupDtlsTransportByMid(
      const std::string& mid);
  rtc::scoped_refptr<SctpTransport> GetSctpTransport(
      const std::string& mid) const;

  DataChannelTransportInterface* GetDataChannelTransport(
      const std::string& mid) const;

  void SetIceConfig(const cricket::IceConfig& config);

  void SetNeedsIceRestartFlag();

  bool NeedsIceRestart(const std::string& mid) const;

  void MaybeStartGathering();
  RTCError AddRemoteCandidates(
      const std::string& mid,
      const std::vector<cricket::Candidate>& candidates);
  RTCError RemoveRemoteCandidates(
      const std::vector<cricket::Candidate>& candidates);

  bool SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);
  rtc::scoped_refptr<rtc::RTCCertificate> GetLocalCertificate(
      const std::string& mid) const;

  std::unique_ptr<rtc::SSLCertChain> GetRemoteSSLCertChain(
      const std::string& mid) const;

  absl::optional<rtc::SSLRole> GetDtlsRole(const std::string& mid) const;

  bool GetStats(const std::string& mid, cricket::TransportStats* stats);

  bool initial_offerer() const { return initial_offerer_ && *initial_offerer_; }

  void SetActiveResetSrtpParams(bool active_reset_srtp_params);

  RTCError RollbackTransports();

  template <typename F>
  void SubscribeIceCandidateGathered(F&& callback) {
    RTC_DCHECK_RUN_ON(network_thread_);
    signal_ice_candidates_gathered_.AddReceiver(std::forward<F>(callback));
  }

  template <typename F>
  void SubscribeIceConnectionState(F&& callback) {
    RTC_DCHECK_RUN_ON(network_thread_);
    signal_ice_connection_state_.AddReceiver(std::forward<F>(callback));
  }

  template <typename F>
  void SubscribeConnectionState(F&& callback) {
    RTC_DCHECK_RUN_ON(network_thread_);
    signal_connection_state_.AddReceiver(std::forward<F>(callback));
  }

  template <typename F>
  void SubscribeStandardizedIceConnectionState(F&& callback) {
    RTC_DCHECK_RUN_ON(network_thread_);
    signal_standardized_ice_connection_state_.AddReceiver(
        std::forward<F>(callback));
  }

  template <typename F>
  void SubscribeIceGatheringState(F&& callback) {
    RTC_DCHECK_RUN_ON(network_thread_);
    signal_ice_gathering_state_.AddReceiver(std::forward<F>(callback));
  }

  template <typename F>
  void SubscribeIceCandidateError(F&& callback) {
    RTC_DCHECK_RUN_ON(network_thread_);
    signal_ice_candidate_error_.AddReceiver(std::forward<F>(callback));
  }

  template <typename F>
  void SubscribeIceCandidatesRemoved(F&& callback) {
    RTC_DCHECK_RUN_ON(network_thread_);
    signal_ice_candidates_removed_.AddReceiver(std::forward<F>(callback));
  }

  template <typename F>
  void SubscribeIceCandidatePairChanged(F&& callback) {
    RTC_DCHECK_RUN_ON(network_thread_);
    signal_ice_candidate_pair_changed_.AddReceiver(std::forward<F>(callback));
  }

 private:

  CallbackList<cricket::IceConnectionState> signal_ice_connection_state_
      RTC_GUARDED_BY(network_thread_);

  CallbackList<PeerConnectionInterface::PeerConnectionState>
      signal_connection_state_ RTC_GUARDED_BY(network_thread_);

  CallbackList<PeerConnectionInterface::IceConnectionState>
      signal_standardized_ice_connection_state_ RTC_GUARDED_BY(network_thread_);

  CallbackList<cricket::IceGatheringState> signal_ice_gathering_state_
      RTC_GUARDED_BY(network_thread_);

  CallbackList<const std::string&, const std::vector<cricket::Candidate>&>
      signal_ice_candidates_gathered_ RTC_GUARDED_BY(network_thread_);

  CallbackList<const cricket::IceCandidateErrorEvent&>
      signal_ice_candidate_error_ RTC_GUARDED_BY(network_thread_);

  CallbackList<const std::vector<cricket::Candidate>&>
      signal_ice_candidates_removed_ RTC_GUARDED_BY(network_thread_);

  CallbackList<const cricket::CandidatePairChangeEvent&>
      signal_ice_candidate_pair_changed_ RTC_GUARDED_BY(network_thread_);

  RTCError ApplyDescription_n(bool local,
                              SdpType type,
                              const cricket::SessionDescription* local_desc,
                              const cricket::SessionDescription* remote_desc)
      RTC_RUN_ON(network_thread_);
  RTCError ValidateAndMaybeUpdateBundleGroups(
      bool local,
      SdpType type,
      const cricket::SessionDescription* local_desc,
      const cricket::SessionDescription* remote_desc)
      RTC_RUN_ON(network_thread_);
  RTCError ValidateContent(const cricket::ContentInfo& content_info);

  void HandleRejectedContent(const cricket::ContentInfo& content_info)
      RTC_RUN_ON(network_thread_);
  bool HandleBundledContent(const cricket::ContentInfo& content_info,
                            const cricket::ContentGroup& bundle_group)
      RTC_RUN_ON(network_thread_);

  cricket::JsepTransportDescription CreateJsepTransportDescription(
      const cricket::ContentInfo& content_info,
      const cricket::TransportInfo& transport_info,
      const std::vector<int>& encrypted_extension_ids,
      int rtp_abs_sendtime_extn_id);

  std::map<const cricket::ContentGroup*, std::vector<int>>
  MergeEncryptedHeaderExtensionIdsForBundles(
      const cricket::SessionDescription* description);
  std::vector<int> GetEncryptedHeaderExtensionIds(
      const cricket::ContentInfo& content_info);

  int GetRtpAbsSendTimeHeaderExtensionId(
      const cricket::ContentInfo& content_info);

  const cricket::JsepTransport* GetJsepTransportForMid(
      const std::string& mid) const RTC_RUN_ON(network_thread_);
  cricket::JsepTransport* GetJsepTransportForMid(const std::string& mid)
      RTC_RUN_ON(network_thread_);
  const cricket::JsepTransport* GetJsepTransportForMid(
      absl::string_view mid) const RTC_RUN_ON(network_thread_);
  cricket::JsepTransport* GetJsepTransportForMid(absl::string_view mid)
      RTC_RUN_ON(network_thread_);

  const cricket::JsepTransport* GetJsepTransportByName(
      const std::string& transport_name) const RTC_RUN_ON(network_thread_);
  cricket::JsepTransport* GetJsepTransportByName(
      const std::string& transport_name) RTC_RUN_ON(network_thread_);

  RTCError MaybeCreateJsepTransport(
      bool local,
      const cricket::ContentInfo& content_info,
      const cricket::SessionDescription& description)
      RTC_RUN_ON(network_thread_);

  void DestroyAllJsepTransports_n() RTC_RUN_ON(network_thread_);

  void SetIceRole_n(cricket::IceRole ice_role) RTC_RUN_ON(network_thread_);

  cricket::IceRole DetermineIceRole(
      cricket::JsepTransport* jsep_transport,
      const cricket::TransportInfo& transport_info,
      SdpType type,
      bool local);

  std::unique_ptr<cricket::DtlsTransportInternal> CreateDtlsTransport(
      const cricket::ContentInfo& content_info,
      cricket::IceTransportInternal* ice);
  rtc::scoped_refptr<IceTransportInterface> CreateIceTransport(
      const std::string& transport_name,
      bool rtcp);

  std::unique_ptr<RtpTransport> CreateUnencryptedRtpTransport(
      const std::string& transport_name,
      rtc::PacketTransportInternal* rtp_packet_transport,
      rtc::PacketTransportInternal* rtcp_packet_transport);
  std::unique_ptr<SrtpTransport> CreateSdesTransport(
      const std::string& transport_name,
      cricket::DtlsTransportInternal* rtp_dtls_transport,
      cricket::DtlsTransportInternal* rtcp_dtls_transport);
  std::unique_ptr<DtlsSrtpTransport> CreateDtlsSrtpTransport(
      const std::string& transport_name,
      cricket::DtlsTransportInternal* rtp_dtls_transport,
      cricket::DtlsTransportInternal* rtcp_dtls_transport);

  std::vector<cricket::DtlsTransportInternal*> GetDtlsTransports();

  std::vector<cricket::DtlsTransportInternal*> GetActiveDtlsTransports();

  void OnTransportWritableState_n(rtc::PacketTransportInternal* transport)
      RTC_RUN_ON(network_thread_);
  void OnTransportReceivingState_n(rtc::PacketTransportInternal* transport)
      RTC_RUN_ON(network_thread_);
  void OnTransportGatheringState_n(cricket::IceTransportInternal* transport)
      RTC_RUN_ON(network_thread_);
  void OnTransportCandidateGathered_n(cricket::IceTransportInternal* transport,
                                      const cricket::Candidate& candidate)
      RTC_RUN_ON(network_thread_);
  void OnTransportCandidateError_n(cricket::IceTransportInternal* transport,
                                   const cricket::IceCandidateErrorEvent& event)
      RTC_RUN_ON(network_thread_);
  void OnTransportCandidatesRemoved_n(cricket::IceTransportInternal* transport,
                                      const cricket::Candidates& candidates)
      RTC_RUN_ON(network_thread_);
  void OnTransportRoleConflict_n(cricket::IceTransportInternal* transport)
      RTC_RUN_ON(network_thread_);
  void OnTransportStateChanged_n(cricket::IceTransportInternal* transport)
      RTC_RUN_ON(network_thread_);
  void OnTransportCandidatePairChanged_n(
      const cricket::CandidatePairChangeEvent& event)
      RTC_RUN_ON(network_thread_);
  void UpdateAggregateStates_n() RTC_RUN_ON(network_thread_);

  void OnRtcpPacketReceived_n(rtc::CopyOnWriteBuffer* packet,
                              int64_t packet_time_us)
      RTC_RUN_ON(network_thread_);
  void OnUnDemuxableRtpPacketReceived_n(const RtpPacketReceived& packet)
      RTC_RUN_ON(network_thread_);

  void OnDtlsHandshakeError(rtc::SSLHandshakeError error);

  bool OnTransportChanged(const std::string& mid,
                          cricket::JsepTransport* transport);

  rtc::Thread* const network_thread_ = nullptr;
  cricket::PortAllocator* const port_allocator_ = nullptr;
  AsyncDnsResolverFactoryInterface* const async_dns_resolver_factory_ = nullptr;

  JsepTransportCollection transports_ RTC_GUARDED_BY(network_thread_);

  cricket::IceConnectionState ice_connection_state_ =
      cricket::kIceConnectionConnecting;
  PeerConnectionInterface::IceConnectionState
      standardized_ice_connection_state_ =
          PeerConnectionInterface::kIceConnectionNew;
  PeerConnectionInterface::PeerConnectionState combined_connection_state_ =
      PeerConnectionInterface::PeerConnectionState::kNew;
  cricket::IceGatheringState ice_gathering_state_ = cricket::kIceGatheringNew;

  const Config config_;
  bool active_reset_srtp_params_ RTC_GUARDED_BY(network_thread_);

  absl::optional<bool> initial_offerer_;

  cricket::IceConfig ice_config_;
  cricket::IceRole ice_role_ = cricket::ICEROLE_CONTROLLING;
  uint64_t ice_tiebreaker_;
  rtc::scoped_refptr<rtc::RTCCertificate> certificate_;

  BundleManager bundles_;
};

}

#endif
