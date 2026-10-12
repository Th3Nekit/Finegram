/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_PEER_CONNECTION_INTERNAL_H_
#define PC_PEER_CONNECTION_INTERNAL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/peer_connection_interface.h"
#include "call/call.h"
#include "modules/audio_device/include/audio_device.h"
#include "pc/jsep_transport_controller.h"
#include "pc/peer_connection_message_handler.h"
#include "pc/rtp_transceiver.h"
#include "pc/rtp_transmission_manager.h"
#include "pc/sctp_data_channel.h"

namespace webrtc {

class DataChannelController;
class LegacyStatsCollector;

class PeerConnectionSdpMethods {
 public:
  virtual ~PeerConnectionSdpMethods() = default;

  virtual std::string session_id() const = 0;

  virtual bool NeedsIceRestart(const std::string& content_name) const = 0;

  virtual absl::optional<std::string> sctp_mid() const = 0;

  virtual const PeerConnectionInterface::RTCConfiguration* configuration()
      const = 0;

  virtual void ReportSdpBundleUsage(
      const SessionDescriptionInterface& remote_description) = 0;

  virtual PeerConnectionMessageHandler* message_handler() = 0;
  virtual RtpTransmissionManager* rtp_manager() = 0;
  virtual const RtpTransmissionManager* rtp_manager() const = 0;
  virtual bool dtls_enabled() const = 0;
  virtual const PeerConnectionFactoryInterface::Options* options() const = 0;

  virtual CryptoOptions GetCryptoOptions() = 0;
  virtual JsepTransportController* transport_controller_s() = 0;
  virtual JsepTransportController* transport_controller_n() = 0;
  virtual DataChannelController* data_channel_controller() = 0;
  virtual cricket::PortAllocator* port_allocator() = 0;
  virtual LegacyStatsCollector* legacy_stats() = 0;

  virtual PeerConnectionObserver* Observer() const = 0;
  virtual absl::optional<rtc::SSLRole> GetSctpSslRole_n() = 0;
  virtual PeerConnectionInterface::IceConnectionState
  ice_connection_state_internal() = 0;
  virtual void SetIceConnectionState(
      PeerConnectionInterface::IceConnectionState new_state) = 0;
  virtual void NoteUsageEvent(UsageEvent event) = 0;
  virtual bool IsClosed() const = 0;

  virtual bool IsUnifiedPlan() const = 0;
  virtual bool ValidateBundleSettings(
      const cricket::SessionDescription* desc,
      const std::map<std::string, const cricket::ContentGroup*>&
          bundle_groups_by_mid) = 0;

  virtual RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>
  AddTransceiver(cricket::MediaType media_type,
                 rtc::scoped_refptr<MediaStreamTrackInterface> track,
                 const RtpTransceiverInit& init,
                 bool fire_callback = true) = 0;

  virtual void StartSctpTransport(int local_port,
                                  int remote_port,
                                  int max_message_size) = 0;

  virtual void AddRemoteCandidate(const std::string& mid,
                                  const cricket::Candidate& candidate) = 0;

  virtual Call* call_ptr() = 0;

  virtual bool SrtpRequired() const = 0;

  virtual bool CreateDataChannelTransport(absl::string_view mid) = 0;

  virtual void DestroyDataChannelTransport(RTCError error) = 0;
  virtual const FieldTrialsView& trials() const = 0;

  virtual void ClearStatsCache() = 0;
};

class PeerConnectionInternal : public PeerConnectionInterface,
                               public PeerConnectionSdpMethods {
 public:
  virtual rtc::Thread* network_thread() const = 0;
  virtual rtc::Thread* worker_thread() const = 0;

  virtual bool initial_offerer() const = 0;

  virtual std::vector<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
  GetTransceiversInternal() const = 0;

  virtual std::vector<DataChannelStats> GetDataChannelStats() const {
    return {};
  }

  virtual absl::optional<std::string> sctp_transport_name() const = 0;

  virtual cricket::CandidateStatsList GetPooledCandidateStats() const = 0;

  virtual std::map<std::string, cricket::TransportStats>
  GetTransportStatsByNames(const std::set<std::string>& transport_names) = 0;

  virtual Call::Stats GetCallStats() = 0;

  virtual absl::optional<AudioDeviceModule::Stats> GetAudioDeviceStats() = 0;

  virtual bool GetLocalCertificate(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) = 0;
  virtual std::unique_ptr<rtc::SSLCertChain> GetRemoteSSLCertChain(
      const std::string& transport_name) = 0;

  virtual bool IceRestartPending(const std::string& content_name) const = 0;

  virtual bool GetSslRole(const std::string& content_name,
                          rtc::SSLRole* role) = 0;

  virtual void NoteDataAddedEvent() {}

  virtual void OnSctpDataChannelStateChanged(
      int channel_id,
      DataChannelInterface::DataState state) {}
};

}

#endif
