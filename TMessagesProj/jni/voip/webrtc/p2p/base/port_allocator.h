/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_PORT_ALLOCATOR_H_
#define P2P_BASE_PORT_ALLOCATOR_H_

#include <stdint.h>

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/candidate.h"
#include "api/sequence_checker.h"
#include "api/transport/enums.h"
#include "p2p/base/port.h"
#include "p2p/base/port_interface.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/checks.h"
#include "rtc_base/helpers.h"
#include "rtc_base/network.h"
#include "rtc_base/proxy_info.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"

namespace webrtc {
class TurnCustomizer;
}

namespace cricket {

enum {

  PORTALLOCATOR_DISABLE_UDP = 0x01,
  PORTALLOCATOR_DISABLE_STUN = 0x02,
  PORTALLOCATOR_DISABLE_RELAY = 0x04,

  PORTALLOCATOR_DISABLE_TCP = 0x08,
  PORTALLOCATOR_ENABLE_IPV6 = 0x40,
  PORTALLOCATOR_ENABLE_SHARED_SOCKET = 0x100,
  PORTALLOCATOR_ENABLE_STUN_RETRANSMIT_ATTRIBUTE = 0x200,

  PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION = 0x400,

  PORTALLOCATOR_DISABLE_DEFAULT_LOCAL_CANDIDATE = 0x800,

  PORTALLOCATOR_DISABLE_UDP_RELAY = 0x1000,

  PORTALLOCATOR_DISABLE_COSTLY_NETWORKS = 0x2000,

  PORTALLOCATOR_ENABLE_IPV6_ON_WIFI = 0x4000,

  PORTALLOCATOR_ENABLE_ANY_ADDRESS_PORTS = 0x8000,

  PORTALLOCATOR_DISABLE_LINK_LOCAL_NETWORKS = 0x10000,
};

enum class IceRegatheringReason {
  NETWORK_CHANGE,
  NETWORK_FAILURE,
  OCCASIONAL_REFRESH,
  MAX_VALUE
};

const uint32_t kDefaultPortAllocatorFlags = 0;

const uint32_t kDefaultStepDelay = 1000;

const uint32_t kMinimumStepDelay = 50;

constexpr int kDefaultMaxIPv6Networks = 5;

enum : uint32_t {
  CF_NONE = 0x0,
  CF_HOST = 0x1,
  CF_REFLEXIVE = 0x2,
  CF_RELAY = 0x4,
  CF_ALL = 0x7,
};

enum class TlsCertPolicy {

  TLS_CERT_POLICY_SECURE,

  TLS_CERT_POLICY_INSECURE_NO_CHECK,
};

struct RelayCredentials {
  RelayCredentials() {}
  RelayCredentials(absl::string_view username, absl::string_view password)
      : username(username), password(password) {}

  bool operator==(const RelayCredentials& o) const {
    return username == o.username && password == o.password;
  }
  bool operator!=(const RelayCredentials& o) const { return !(*this == o); }

  std::string username;
  std::string password;
};

typedef std::vector<ProtocolAddress> PortList;

struct RTC_EXPORT RelayServerConfig {
  RelayServerConfig();
  RelayServerConfig(const rtc::SocketAddress& address,
                    absl::string_view username,
                    absl::string_view password,
                    ProtocolType proto);
  RelayServerConfig(absl::string_view address,
                    int port,
                    absl::string_view username,
                    absl::string_view password,
                    ProtocolType proto);

  RelayServerConfig(absl::string_view address,
                    int port,
                    absl::string_view username,
                    absl::string_view password,
                    ProtocolType proto,
                    bool secure);
  RelayServerConfig(const RelayServerConfig&);
  ~RelayServerConfig();

  bool operator==(const RelayServerConfig& o) const {
    return ports == o.ports && credentials == o.credentials;
  }
  bool operator!=(const RelayServerConfig& o) const { return !(*this == o); }

  PortList ports;
  RelayCredentials credentials;
  TlsCertPolicy tls_cert_policy = TlsCertPolicy::TLS_CERT_POLICY_SECURE;
  std::vector<std::string> tls_alpn_protocols;
  std::vector<std::string> tls_elliptic_curves;
  rtc::SSLCertificateVerifier* tls_cert_verifier = nullptr;
  std::string turn_logging_id;
};

class RTC_EXPORT PortAllocatorSession : public sigslot::has_slots<> {
 public:

  PortAllocatorSession(absl::string_view content_name,
                       int component,
                       absl::string_view ice_ufrag,
                       absl::string_view ice_pwd,
                       uint32_t flags);

  ~PortAllocatorSession() override;

  uint32_t flags() const { return flags_; }
  void set_flags(uint32_t flags) { flags_ = flags; }
  std::string content_name() const { return content_name_; }
  int component() const { return component_; }
  const std::string& ice_ufrag() const { return ice_ufrag_; }
  const std::string& ice_pwd() const { return ice_pwd_; }
  bool pooled() const { return pooled_; }

  virtual void SetCandidateFilter(uint32_t filter) = 0;

  virtual void StartGettingPorts() = 0;

  virtual void StopGettingPorts() = 0;

  virtual bool IsGettingPorts() = 0;

  virtual void ClearGettingPorts() = 0;

  virtual bool IsCleared() const;

  virtual bool IsStopped() const;

  virtual void RegatherOnFailedNetworks() {}

  virtual void GetCandidateStatsFromReadyPorts(
      CandidateStatsList* candidate_stats_list) const {}

  virtual void SetStunKeepaliveIntervalForReadyPorts(
      const absl::optional<int>& stun_keepalive_interval) {}

  virtual std::vector<PortInterface*> ReadyPorts() const = 0;
  virtual std::vector<Candidate> ReadyCandidates() const = 0;
  virtual bool CandidatesAllocationDone() const = 0;

  virtual void PruneAllPorts() {}

  sigslot::signal2<PortAllocatorSession*, PortInterface*> SignalPortReady;

  sigslot::signal2<PortAllocatorSession*, const std::vector<PortInterface*>&>
      SignalPortsPruned;
  sigslot::signal2<PortAllocatorSession*, const std::vector<Candidate>&>
      SignalCandidatesReady;
  sigslot::signal2<PortAllocatorSession*, const IceCandidateErrorEvent&>
      SignalCandidateError;

  sigslot::signal2<PortAllocatorSession*, const std::vector<Candidate>&>
      SignalCandidatesRemoved;
  sigslot::signal1<PortAllocatorSession*> SignalCandidatesAllocationDone;

  sigslot::signal2<PortAllocatorSession*, IceRegatheringReason>
      SignalIceRegathering;

  virtual uint32_t generation();
  virtual void set_generation(uint32_t generation);

 protected:

  virtual void UpdateIceParametersInternal() {}

  const std::string& username() const { return ice_ufrag_; }
  const std::string& password() const { return ice_pwd_; }

 private:
  void SetIceParameters(absl::string_view content_name,
                        int component,
                        absl::string_view ice_ufrag,
                        absl::string_view ice_pwd) {
    content_name_ = std::string(content_name);
    component_ = component;
    ice_ufrag_ = std::string(ice_ufrag);
    ice_pwd_ = std::string(ice_pwd);
    UpdateIceParametersInternal();
  }

  void set_pooled(bool value) { pooled_ = value; }

  uint32_t flags_;
  uint32_t generation_;
  std::string content_name_;
  int component_;
  std::string ice_ufrag_;
  std::string ice_pwd_;

  bool pooled_ = false;

  friend class PortAllocator;
};

class RTC_EXPORT PortAllocator : public sigslot::has_slots<> {
 public:
  PortAllocator();
  ~PortAllocator() override;

  virtual void Initialize();

  virtual void set_restrict_ice_credentials_change(bool value);

  bool SetConfiguration(const ServerAddresses& stun_servers,
                        const std::vector<RelayServerConfig>& turn_servers,
                        int candidate_pool_size,
                        bool prune_turn_ports,
                        webrtc::TurnCustomizer* turn_customizer = nullptr,
                        const absl::optional<int>&
                            stun_candidate_keepalive_interval = absl::nullopt);
  bool SetConfiguration(const ServerAddresses& stun_servers,
                        const std::vector<RelayServerConfig>& turn_servers,
                        int candidate_pool_size,
                        webrtc::PortPrunePolicy turn_port_prune_policy,
                        webrtc::TurnCustomizer* turn_customizer = nullptr,
                        const absl::optional<int>&
                            stun_candidate_keepalive_interval = absl::nullopt);

  const ServerAddresses& stun_servers() const {
    CheckRunOnValidThreadIfInitialized();
    return stun_servers_;
  }

  const std::vector<RelayServerConfig>& turn_servers() const {
    CheckRunOnValidThreadIfInitialized();
    return turn_servers_;
  }

  int candidate_pool_size() const {
    CheckRunOnValidThreadIfInitialized();
    return candidate_pool_size_;
  }

  const absl::optional<int>& stun_candidate_keepalive_interval() const {
    CheckRunOnValidThreadIfInitialized();
    return stun_candidate_keepalive_interval_;
  }

  virtual void SetNetworkIgnoreMask(int network_ignore_mask) = 0;

  virtual void SetVpnPreference(webrtc::VpnPreference preference) {
    vpn_preference_ = preference;
  }

  virtual void SetVpnList(const std::vector<rtc::NetworkMask>& vpn_list) {}

  std::unique_ptr<PortAllocatorSession> CreateSession(
      absl::string_view content_name,
      int component,
      absl::string_view ice_ufrag,
      absl::string_view ice_pwd);

  std::unique_ptr<PortAllocatorSession> TakePooledSession(
      absl::string_view content_name,
      int component,
      absl::string_view ice_ufrag,
      absl::string_view ice_pwd);

  const PortAllocatorSession* GetPooledSession(
      const IceParameters* ice_credentials = nullptr) const;

  void DiscardCandidatePool();

  Candidate SanitizeCandidate(const Candidate& c) const;

  uint64_t ice_tiebreaker() const { return tiebreaker_; }

  uint32_t flags() const {
    CheckRunOnValidThreadIfInitialized();
    return flags_;
  }

  void set_flags(uint32_t flags) {
    CheckRunOnValidThreadIfInitialized();
    flags_ = flags;
  }

  const std::string& user_agent() const {
    CheckRunOnValidThreadIfInitialized();
    return agent_;
  }

  const rtc::ProxyInfo& proxy() const {
    CheckRunOnValidThreadIfInitialized();
    return proxy_;
  }

  void set_proxy(absl::string_view agent, const rtc::ProxyInfo& proxy) {
    CheckRunOnValidThreadIfInitialized();
    agent_ = std::string(agent);
    proxy_ = proxy;
  }

  int min_port() const {
    CheckRunOnValidThreadIfInitialized();
    return min_port_;
  }

  int max_port() const {
    CheckRunOnValidThreadIfInitialized();
    return max_port_;
  }

  bool SetPortRange(int min_port, int max_port) {
    CheckRunOnValidThreadIfInitialized();
    if (min_port > max_port) {
      return false;
    }

    min_port_ = min_port;
    max_port_ = max_port;
    return true;
  }

  void set_max_ipv6_networks(int networks) {
    CheckRunOnValidThreadIfInitialized();
    max_ipv6_networks_ = networks;
  }

  int max_ipv6_networks() {
    CheckRunOnValidThreadIfInitialized();
    return max_ipv6_networks_;
  }

  uint32_t step_delay() const {
    CheckRunOnValidThreadIfInitialized();
    return step_delay_;
  }

  void set_step_delay(uint32_t delay) {
    CheckRunOnValidThreadIfInitialized();
    step_delay_ = delay;
  }

  bool allow_tcp_listen() const {
    CheckRunOnValidThreadIfInitialized();
    return allow_tcp_listen_;
  }

  void set_allow_tcp_listen(bool allow_tcp_listen) {
    CheckRunOnValidThreadIfInitialized();
    allow_tcp_listen_ = allow_tcp_listen;
  }

  uint32_t candidate_filter() {
    CheckRunOnValidThreadIfInitialized();
    return candidate_filter_;
  }

  void SetCandidateFilter(uint32_t filter);

  void set_candidate_filter(uint32_t filter) { SetCandidateFilter(filter); }

  bool prune_turn_ports() const {
    CheckRunOnValidThreadIfInitialized();
    return turn_port_prune_policy_ == webrtc::PRUNE_BASED_ON_PRIORITY;
  }

  webrtc::PortPrunePolicy turn_port_prune_policy() const {
    CheckRunOnValidThreadIfInitialized();
    return turn_port_prune_policy_;
  }

  webrtc::TurnCustomizer* turn_customizer() {
    CheckRunOnValidThreadIfInitialized();
    return turn_customizer_;
  }

  virtual void GetCandidateStatsFromPooledSessions(
      CandidateStatsList* candidate_stats_list);

  std::vector<IceParameters> GetPooledIceCredentials();

  sigslot::signal2<uint32_t                  , uint32_t                 >
      SignalCandidateFilterChanged;

 protected:

  virtual PortAllocatorSession* CreateSessionInternal(
      absl::string_view content_name,
      int component,
      absl::string_view ice_ufrag,
      absl::string_view ice_pwd) = 0;

  const std::vector<std::unique_ptr<PortAllocatorSession>>& pooled_sessions() {
    return pooled_sessions_;
  }

  virtual bool MdnsObfuscationEnabled() const { return false; }

  void CheckRunOnValidThreadIfInitialized() const {
    RTC_DCHECK(!initialized_ || thread_checker_.IsCurrent());
  }

  void CheckRunOnValidThreadAndInitialized() const {
    RTC_DCHECK(initialized_ && thread_checker_.IsCurrent());
  }

  bool initialized_ = false;
  uint32_t flags_;
  std::string agent_;
  rtc::ProxyInfo proxy_;
  int min_port_;
  int max_port_;
  int max_ipv6_networks_;
  uint32_t step_delay_;
  bool allow_tcp_listen_;
  uint32_t candidate_filter_;
  std::string origin_;
  webrtc::SequenceChecker thread_checker_;
  webrtc::VpnPreference vpn_preference_ = webrtc::VpnPreference::kDefault;

 private:
  ServerAddresses stun_servers_;
  std::vector<RelayServerConfig> turn_servers_;
  int candidate_pool_size_ = 0;
  std::vector<std::unique_ptr<PortAllocatorSession>> pooled_sessions_;
  webrtc::PortPrunePolicy turn_port_prune_policy_ = webrtc::NO_PRUNE;

  webrtc::TurnCustomizer* turn_customizer_ = nullptr;

  absl::optional<int> stun_candidate_keepalive_interval_;

  bool restrict_ice_credentials_change_ = false;

  std::vector<std::unique_ptr<PortAllocatorSession>>::const_iterator
  FindPooledSession(const IceParameters* ice_credentials = nullptr) const;

  uint64_t tiebreaker_;
};

}

#endif
