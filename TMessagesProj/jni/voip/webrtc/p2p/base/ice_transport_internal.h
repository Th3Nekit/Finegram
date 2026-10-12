/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICE_TRANSPORT_INTERNAL_H_
#define P2P_BASE_ICE_TRANSPORT_INTERNAL_H_

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/candidate.h"
#include "api/rtc_error.h"
#include "api/transport/enums.h"
#include "p2p/base/connection.h"
#include "p2p/base/packet_transport_internal.h"
#include "p2p/base/port.h"
#include "p2p/base/stun_dictionary.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/network_constants.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/time_utils.h"

namespace cricket {

struct IceTransportStats {
  CandidateStatsList candidate_stats_list;
  ConnectionInfos connection_infos;

  uint32_t selected_candidate_pair_changes = 0;

  uint64_t bytes_sent = 0;
  uint64_t bytes_received = 0;
  uint64_t packets_sent = 0;
  uint64_t packets_received = 0;

  IceRole ice_role = ICEROLE_UNKNOWN;
  std::string ice_local_username_fragment;
  webrtc::IceTransportState ice_state = webrtc::IceTransportState::kNew;
};

typedef std::vector<Candidate> Candidates;

enum IceConnectionState {
  kIceConnectionConnecting = 0,
  kIceConnectionFailed,
  kIceConnectionConnected,

  kIceConnectionCompleted,
};

enum IceGatheringState {
  kIceGatheringNew = 0,
  kIceGatheringGathering,
  kIceGatheringComplete,
};

enum ContinualGatheringPolicy {

  GATHER_ONCE = 0,

  GATHER_CONTINUALLY,
};

enum class NominationMode {
  REGULAR,
  AGGRESSIVE,

  SEMI_AGGRESSIVE

};

webrtc::RTCError VerifyCandidate(const Candidate& cand);

webrtc::RTCError VerifyCandidates(const Candidates& candidates);

struct RTC_EXPORT IceConfig {

  absl::optional<int> receiving_timeout;

  absl::optional<int> backup_connection_ping_interval;

  ContinualGatheringPolicy continual_gathering_policy = GATHER_ONCE;

  bool gather_continually() const {
    return continual_gathering_policy == GATHER_CONTINUALLY;
  }

  bool prioritize_most_likely_candidate_pairs = false;

  absl::optional<int> stable_writable_connection_ping_interval;

  bool presume_writable_when_fully_relayed = false;

  bool surface_ice_candidates_on_ice_transport_type_changed = false;

  absl::optional<int> regather_on_failed_networks_interval;

  absl::optional<int> receiving_switching_delay;

  NominationMode default_nomination_mode = NominationMode::SEMI_AGGRESSIVE;

  absl::optional<int> ice_check_interval_strong_connectivity;

  absl::optional<int> ice_check_interval_weak_connectivity;

  absl::optional<int> ice_check_min_interval;

  absl::optional<int> ice_unwritable_timeout;

  absl::optional<int> ice_unwritable_min_checks;

  absl::optional<int> ice_inactive_timeout;

  absl::optional<int> stun_keepalive_interval;

  absl::optional<rtc::AdapterType> network_preference;

  webrtc::VpnPreference vpn_preference = webrtc::VpnPreference::kDefault;

  IceConfig();
  IceConfig(int receiving_timeout_ms,
            int backup_connection_ping_interval,
            ContinualGatheringPolicy gathering_policy,
            bool prioritize_most_likely_candidate_pairs,
            int stable_writable_connection_ping_interval_ms,
            bool presume_writable_when_fully_relayed,
            int regather_on_failed_networks_interval_ms,
            int receiving_switching_delay_ms);
  ~IceConfig();

  int receiving_timeout_or_default() const;
  int backup_connection_ping_interval_or_default() const;
  int stable_writable_connection_ping_interval_or_default() const;
  int regather_on_failed_networks_interval_or_default() const;
  int receiving_switching_delay_or_default() const;
  int ice_check_interval_strong_connectivity_or_default() const;
  int ice_check_interval_weak_connectivity_or_default() const;
  int ice_check_min_interval_or_default() const;
  int ice_unwritable_timeout_or_default() const;
  int ice_unwritable_min_checks_or_default() const;
  int ice_inactive_timeout_or_default() const;
  int stun_keepalive_interval_or_default() const;
};

enum class IceTransportState {
  STATE_INIT,
  STATE_CONNECTING,
  STATE_COMPLETED,
  STATE_FAILED
};

class RTC_EXPORT IceTransportInternal : public rtc::PacketTransportInternal {
 public:
  IceTransportInternal();
  ~IceTransportInternal() override;

  virtual IceTransportState GetState() const = 0;
  virtual webrtc::IceTransportState GetIceTransportState() const = 0;

  virtual int component() const = 0;

  virtual IceRole GetIceRole() const = 0;

  virtual void SetIceRole(IceRole role) = 0;

  virtual void SetIceTiebreaker(uint64_t tiebreaker) = 0;

  virtual void SetIceCredentials(absl::string_view ice_ufrag,
                                 absl::string_view ice_pwd);

  virtual void SetRemoteIceCredentials(absl::string_view ice_ufrag,
                                       absl::string_view ice_pwd);

  virtual void SetIceParameters(const IceParameters& ice_params) = 0;

  virtual void SetRemoteIceParameters(const IceParameters& ice_params) = 0;

  virtual void SetRemoteIceMode(IceMode mode) = 0;

  virtual void SetIceConfig(const IceConfig& config) = 0;

  virtual void MaybeStartGathering() = 0;

  virtual void AddRemoteCandidate(const Candidate& candidate) = 0;

  virtual void RemoveRemoteCandidate(const Candidate& candidate) = 0;

  virtual void RemoveAllRemoteCandidates() = 0;

  virtual IceGatheringState gathering_state() const = 0;

  virtual bool GetStats(IceTransportStats* ice_transport_stats) = 0;

  virtual absl::optional<int> GetRttEstimate() = 0;

  virtual const Connection* selected_connection() const = 0;

  virtual absl::optional<const CandidatePair> GetSelectedCandidatePair()
      const = 0;

  virtual absl::optional<std::reference_wrapper<StunDictionaryWriter>>
  GetDictionaryWriter() {
    return absl::nullopt;
  }

  sigslot::signal1<IceTransportInternal*> SignalGatheringState;
  void SetGatheringStateCallback(
      absl::AnyInvocable<void(IceTransportInternal*)> callback) {
    RTC_DCHECK(!gathering_state_callback_);
    gathering_state_callback_ = std::move(callback);
  }

  sigslot::signal2<IceTransportInternal*, const Candidate&>
      SignalCandidateGathered;

  void SetCandidateErrorCallback(
      absl::AnyInvocable<void(IceTransportInternal*,
                              const IceCandidateErrorEvent&)> callback) {
    RTC_DCHECK(!candidate_error_callback_);
    candidate_error_callback_ = std::move(callback);
  }

  void SetCandidatesRemovedCallback(
      absl::AnyInvocable<void(IceTransportInternal*, const Candidates&)>
          callback) {
    RTC_DCHECK(!candidates_removed_callback_);
    candidates_removed_callback_ = std::move(callback);
  }

  sigslot::signal2<IceTransportInternal*, const Candidate&> SignalRouteChange;

  void SetCandidatePairChangeCallback(
      absl::AnyInvocable<void(const cricket::CandidatePairChangeEvent&)>
          callback) {
    RTC_DCHECK(!candidate_pair_change_callback_);
    candidate_pair_change_callback_ = std::move(callback);
  }

  sigslot::signal1<IceTransportInternal*> SignalRoleConflict;

  sigslot::signal1<IceTransportInternal*> SignalStateChanged;

  sigslot::signal1<IceTransportInternal*> SignalIceTransportStateChanged;

  sigslot::signal1<IceTransportInternal*> SignalDestroyed;

  template <typename F>
  void AddDictionaryViewUpdatedCallback(const void* tag, F&& callback) {
    dictionary_view_updated_callback_list_.AddReceiver(
        tag, std::forward<F>(callback));
  }
  void RemoveDictionaryViewUpdatedCallback(const void* tag) {
    dictionary_view_updated_callback_list_.RemoveReceivers(tag);
  }

  template <typename F>
  void AddDictionaryWriterSyncedCallback(const void* tag, F&& callback) {
    dictionary_writer_synced_callback_list_.AddReceiver(
        tag, std::forward<F>(callback));
  }
  void RemoveDictionaryWriterSyncedCallback(const void* tag) {
    dictionary_writer_synced_callback_list_.RemoveReceivers(tag);
  }

 protected:
  void SendGatheringStateEvent() { SignalGatheringState(this); }

  webrtc::CallbackList<IceTransportInternal*,
                       const StunDictionaryView&,
                       rtc::ArrayView<uint16_t>>
      dictionary_view_updated_callback_list_;
  webrtc::CallbackList<IceTransportInternal*, const StunDictionaryWriter&>
      dictionary_writer_synced_callback_list_;

  absl::AnyInvocable<void(IceTransportInternal*)> gathering_state_callback_;

  absl::AnyInvocable<void(IceTransportInternal*, const IceCandidateErrorEvent&)>
      candidate_error_callback_;

  absl::AnyInvocable<void(IceTransportInternal*, const Candidates&)>
      candidates_removed_callback_;

  absl::AnyInvocable<void(const cricket::CandidatePairChangeEvent&)>
      candidate_pair_change_callback_;

 private:

  void SignalGatheringStateFired(IceTransportInternal* transport) {
    if (gathering_state_callback_) {
      gathering_state_callback_(transport);
    }
  }
};

}

#endif
