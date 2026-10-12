/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NETWORK_MONITOR_H_
#define RTC_BASE_NETWORK_MONITOR_H_

#include <functional>
#include <utility>

#include "absl/strings/string_view.h"
#include "rtc_base/network_constants.h"

namespace rtc {

class IPAddress;

enum class NetworkBindingResult {
  SUCCESS = 0,
  FAILURE = -1,
  NOT_IMPLEMENTED = -2,
  ADDRESS_NOT_FOUND = -3,
  NETWORK_CHANGED = -4
};

enum class NetworkPreference {
  NEUTRAL = 0,
  NOT_PREFERRED = -1,
};

const char* NetworkPreferenceToString(NetworkPreference preference);

class NetworkBinderInterface {
 public:

  virtual NetworkBindingResult BindSocketToNetwork(
      int socket_fd,
      const IPAddress& address) = 0;
  virtual ~NetworkBinderInterface() {}
};

class NetworkMonitorInterface {
 public:
  struct InterfaceInfo {

    AdapterType adapter_type;

    AdapterType underlying_type_for_vpn = ADAPTER_TYPE_UNKNOWN;

    NetworkPreference network_preference = NetworkPreference::NEUTRAL;

    bool available = true;
  };

  NetworkMonitorInterface();
  virtual ~NetworkMonitorInterface();

  virtual void Start() = 0;
  virtual void Stop() = 0;

  virtual InterfaceInfo GetInterfaceInfo(absl::string_view interface_name) = 0;

  virtual bool SupportsBindSocketToNetwork() const { return false; }

  virtual NetworkBindingResult BindSocketToNetwork(
      int socket_fd,
      const IPAddress& address,
      absl::string_view interface_name) {
    return NetworkBindingResult::NOT_IMPLEMENTED;
  }

  void SetNetworksChangedCallback(std::function<void()> callback) {
    networks_changed_callback_ = std::move(callback);
  }

 protected:
  void InvokeNetworksChangedCallback() {
    if (networks_changed_callback_) {
      networks_changed_callback_();
    }
  }

 private:
  std::function<void()> networks_changed_callback_;
};

}

#endif
