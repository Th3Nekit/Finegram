/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NETWORK_H_
#define RTC_BASE_NETWORK_H_

#include <stdint.h>

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/field_trials_view.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/transport/field_trial_based_config.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/mdns_responder_interface.h"
#include "rtc_base/memory/always_valid_pointer.h"
#include "rtc_base/network_monitor.h"
#include "rtc_base/network_monitor_factory.h"
#include "rtc_base/socket_factory.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread_annotations.h"

#if defined(WEBRTC_POSIX)
struct ifaddrs;
#endif

namespace rtc {

extern const char kPublicIPv4Host[];
extern const char kPublicIPv6Host[];

class IfAddrsConverter;
class Network;
class NetworkMonitorInterface;
class Thread;

const int kDefaultNetworkIgnoreMask = ADAPTER_TYPE_LOOPBACK;

namespace webrtc_network_internal {
bool CompareNetworks(const std::unique_ptr<Network>& a,
                     const std::unique_ptr<Network>& b);
}

std::string MakeNetworkKey(absl::string_view name,
                           const IPAddress& prefix,
                           int prefix_length);

RTC_EXPORT AdapterType GetAdapterTypeFromName(absl::string_view network_name);

class DefaultLocalAddressProvider {
 public:
  virtual ~DefaultLocalAddressProvider() = default;

  virtual bool GetDefaultLocalAddress(int family, IPAddress* ipaddr) const = 0;
};

class MdnsResponderProvider {
 public:
  virtual ~MdnsResponderProvider() = default;

  virtual webrtc::MdnsResponderInterface* GetMdnsResponder() const = 0;
};

class NetworkMask {
 public:
  NetworkMask(const IPAddress& addr, int prefix_length)
      : address_(addr), prefix_length_(prefix_length) {}

  const IPAddress& address() const { return address_; }
  int prefix_length() const { return prefix_length_; }

  bool operator==(const NetworkMask& o) const {
    return address_ == o.address_ && prefix_length_ == o.prefix_length_;
  }

 private:
  IPAddress address_;

  int prefix_length_;
};

class RTC_EXPORT NetworkManager : public DefaultLocalAddressProvider,
                                  public MdnsResponderProvider {
 public:

  enum EnumerationPermission {
    ENUMERATION_ALLOWED,

    ENUMERATION_BLOCKED,

  };

  sigslot::signal0<> SignalNetworksChanged;

  sigslot::signal0<> SignalError;

  virtual void Initialize() {}

  virtual void StartUpdating() = 0;
  virtual void StopUpdating() = 0;

  virtual std::vector<const Network*> GetNetworks() const = 0;

  virtual EnumerationPermission enumeration_permission() const;

  virtual std::vector<const Network*> GetAnyAddressNetworks() = 0;

  virtual void DumpNetworks() {}
  bool GetDefaultLocalAddress(int family, IPAddress* ipaddr) const override;

  struct Stats {
    int ipv4_network_count;
    int ipv6_network_count;
    Stats() {
      ipv4_network_count = 0;
      ipv6_network_count = 0;
    }
  };

  webrtc::MdnsResponderInterface* GetMdnsResponder() const override;

  virtual void set_vpn_list(const std::vector<NetworkMask>& vpn) {}
};

class RTC_EXPORT NetworkManagerBase : public NetworkManager {
 public:
  NetworkManagerBase(const webrtc::FieldTrialsView* field_trials = nullptr);

  std::vector<const Network*> GetNetworks() const override;
  std::vector<const Network*> GetAnyAddressNetworks() override;

  EnumerationPermission enumeration_permission() const override;

  bool GetDefaultLocalAddress(int family, IPAddress* ipaddr) const override;

  static bool IsVpnMacAddress(rtc::ArrayView<const uint8_t> address);

 protected:

  void MergeNetworkList(std::vector<std::unique_ptr<Network>> list,
                        bool* changed);

  void MergeNetworkList(std::vector<std::unique_ptr<Network>> list,
                        bool* changed,
                        NetworkManager::Stats* stats);

  void set_enumeration_permission(EnumerationPermission state) {
    enumeration_permission_ = state;
  }

  void set_default_local_addresses(const IPAddress& ipv4,
                                   const IPAddress& ipv6);

  Network* GetNetworkFromAddress(const rtc::IPAddress& ip) const;

  const std::vector<Network*>& GetNetworksInternal() const { return networks_; }

  std::unique_ptr<Network> CreateNetwork(absl::string_view name,
                                         absl::string_view description,
                                         const IPAddress& prefix,
                                         int prefix_length,
                                         AdapterType type) const;

  const webrtc::FieldTrialsView* field_trials() const {
    return field_trials_.get();
  }

 private:
  friend class NetworkTest;
  webrtc::AlwaysValidPointer<const webrtc::FieldTrialsView,
                             webrtc::FieldTrialBasedConfig>
      field_trials_;
  EnumerationPermission enumeration_permission_;

  std::vector<Network*> networks_;

  std::map<std::string, std::unique_ptr<Network>> networks_map_;

  std::unique_ptr<rtc::Network> ipv4_any_address_network_;
  std::unique_ptr<rtc::Network> ipv6_any_address_network_;

  IPAddress default_local_ipv4_address_;
  IPAddress default_local_ipv6_address_;

  uint16_t next_available_network_id_ = 1;

  bool signal_network_preference_change_ = false;
};

class RTC_EXPORT BasicNetworkManager : public NetworkManagerBase,
                                       public NetworkBinderInterface,
                                       public sigslot::has_slots<> {
 public:

  BasicNetworkManager(SocketFactory* socket_factory,
                      const webrtc::FieldTrialsView* field_trials = nullptr)
      : BasicNetworkManager(                               nullptr,
                            socket_factory,
                            field_trials) {}

  BasicNetworkManager(NetworkMonitorFactory* network_monitor_factory,
                      SocketFactory* socket_factory,
                      const webrtc::FieldTrialsView* field_trials = nullptr);
  ~BasicNetworkManager() override;

  void StartUpdating() override;
  void StopUpdating() override;

  void DumpNetworks() override;

  bool started() { return start_count_ > 0; }

  void set_network_ignore_list(const std::vector<std::string>& list) {
    RTC_DCHECK(thread_ == nullptr);
    network_ignore_list_ = list;
  }

  void set_vpn_list(const std::vector<NetworkMask>& vpn) override;

  bool IsConfiguredVpn(IPAddress prefix, int prefix_length) const;

  NetworkBindingResult BindSocketToNetwork(int socket_fd,
                                           const IPAddress& address) override;

 protected:
#if defined(WEBRTC_POSIX)

  void ConvertIfAddrs(ifaddrs* interfaces,
                      IfAddrsConverter* converter,
                      bool include_ignored,
                      std::vector<std::unique_ptr<Network>>* networks) const
      RTC_RUN_ON(thread_);
  NetworkMonitorInterface::InterfaceInfo GetInterfaceInfo(
      struct ifaddrs* cursor) const RTC_RUN_ON(thread_);
#endif

  bool CreateNetworks(bool include_ignored,
                      std::vector<std::unique_ptr<Network>>* networks) const
      RTC_RUN_ON(thread_);

  bool IsIgnoredNetwork(const Network& network) const RTC_RUN_ON(thread_);

  IPAddress QueryDefaultLocalAddress(int family) const RTC_RUN_ON(thread_);

 private:
  friend class NetworkTest;

  void StartNetworkMonitor() RTC_RUN_ON(thread_);

  void StopNetworkMonitor() RTC_RUN_ON(thread_);

  void OnNetworksChanged();

  void UpdateNetworksContinually() RTC_RUN_ON(thread_);

  void UpdateNetworksOnce() RTC_RUN_ON(thread_);

  Thread* thread_ = nullptr;
  bool sent_first_update_ = true;
  int start_count_ = 0;

  webrtc::AlwaysValidPointer<const webrtc::FieldTrialsView,
                             webrtc::FieldTrialBasedConfig>
      field_trials_;
  std::vector<std::string> network_ignore_list_;
  NetworkMonitorFactory* const network_monitor_factory_;
  SocketFactory* const socket_factory_;
  std::unique_ptr<NetworkMonitorInterface> network_monitor_
      RTC_GUARDED_BY(thread_);
  bool allow_mac_based_ipv6_ RTC_GUARDED_BY(thread_) = false;
  bool bind_using_ifname_ RTC_GUARDED_BY(thread_) = false;

  std::vector<NetworkMask> vpn_;
  rtc::scoped_refptr<webrtc::PendingTaskSafetyFlag> task_safety_flag_;
};

class RTC_EXPORT Network {
 public:
  Network(absl::string_view name,
          absl::string_view description,
          const IPAddress& prefix,
          int prefix_length)
      : Network(name,
                description,
                prefix,
                prefix_length,
                rtc::ADAPTER_TYPE_UNKNOWN) {}

  Network(absl::string_view name,
          absl::string_view description,
          const IPAddress& prefix,
          int prefix_length,
          AdapterType type);

  Network(const Network&);
  ~Network();

  mutable sigslot::signal1<const Network*> SignalTypeChanged;

  sigslot::signal1<const Network*> SignalNetworkPreferenceChanged;

  const DefaultLocalAddressProvider* default_local_address_provider() const {
    return default_local_address_provider_;
  }
  void set_default_local_address_provider(
      const DefaultLocalAddressProvider* provider) {
    default_local_address_provider_ = provider;
  }

  void set_mdns_responder_provider(const MdnsResponderProvider* provider) {
    mdns_responder_provider_ = provider;
  }

  const std::string& name() const { return name_; }

  const std::string& description() const { return description_; }

  const IPAddress& prefix() const { return prefix_; }

  int prefix_length() const { return prefix_length_; }

  int family() const { return prefix_.family(); }

  std::string key() const { return key_; }

  IPAddress GetBestIP() const;

  void AddIP(const InterfaceAddress& ip) { ips_.push_back(ip); }
  void AddIP(const IPAddress& ip) { ips_.push_back(rtc::InterfaceAddress(ip)); }

  bool SetIPs(const std::vector<InterfaceAddress>& ips, bool already_changed);

  const std::vector<InterfaceAddress>& GetIPs() const { return ips_; }

  void ClearIPs() { ips_.clear(); }

  webrtc::MdnsResponderInterface* GetMdnsResponder() const;

  int scope_id() const { return scope_id_; }
  void set_scope_id(int id) { scope_id_ = id; }

  bool ignored() const { return ignored_; }
  void set_ignored(bool ignored) { ignored_ = ignored; }

  AdapterType type() const { return type_; }

  AdapterType underlying_type_for_vpn() const {
    return underlying_type_for_vpn_;
  }
  void set_type(AdapterType type) {
    if (type_ == type) {
      return;
    }
    type_ = type;
    if (type != ADAPTER_TYPE_VPN) {
      underlying_type_for_vpn_ = ADAPTER_TYPE_UNKNOWN;
    }
    SignalTypeChanged(this);
  }

  void set_underlying_type_for_vpn(AdapterType type) {
    if (underlying_type_for_vpn_ == type) {
      return;
    }
    underlying_type_for_vpn_ = type;
    SignalTypeChanged(this);
  }

  bool IsVpn() const { return type_ == ADAPTER_TYPE_VPN; }

  bool IsCellular() const { return IsCellular(type_); }

  static bool IsCellular(AdapterType type) {
    switch (type) {
      case ADAPTER_TYPE_CELLULAR:
      case ADAPTER_TYPE_CELLULAR_2G:
      case ADAPTER_TYPE_CELLULAR_3G:
      case ADAPTER_TYPE_CELLULAR_4G:
      case ADAPTER_TYPE_CELLULAR_5G:
        return true;
      default:
        return false;
    }
  }

  ABSL_DEPRECATED(
      "Use the version with field trials, see bugs.webrtc.org/webrtc:10335")
  uint16_t GetCost(const webrtc::FieldTrialsView* field_trials = nullptr) const;
  uint16_t GetCost(const webrtc::FieldTrialsView& field_trials) const;

  uint16_t id() const { return id_; }
  void set_id(uint16_t id) { id_ = id; }

  int preference() const { return preference_; }
  void set_preference(int preference) { preference_ = preference; }

  bool active() const { return active_; }
  void set_active(bool active) {
    if (active_ != active) {
      active_ = active;
    }
  }

  NetworkPreference network_preference() const { return network_preference_; }
  void set_network_preference(NetworkPreference val) {
    if (network_preference_ == val) {
      return;
    }
    network_preference_ = val;
    SignalNetworkPreferenceChanged(this);
  }

  static std::pair<rtc::AdapterType, bool          >
  GuessAdapterFromNetworkCost(int network_cost);

  std::string ToString() const;

 private:
  const DefaultLocalAddressProvider* default_local_address_provider_ = nullptr;
  const MdnsResponderProvider* mdns_responder_provider_ = nullptr;
  std::string name_;
  std::string description_;
  IPAddress prefix_;
  int prefix_length_;
  std::string key_;
  std::vector<InterfaceAddress> ips_;
  int scope_id_;
  bool ignored_;
  AdapterType type_;
  AdapterType underlying_type_for_vpn_ = ADAPTER_TYPE_UNKNOWN;
  int preference_;
  bool active_ = true;
  uint16_t id_ = 0;
  NetworkPreference network_preference_ = NetworkPreference::NEUTRAL;

  friend class NetworkManager;
};

}

#endif
