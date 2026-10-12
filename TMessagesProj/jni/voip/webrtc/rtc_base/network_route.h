/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NETWORK_ROUTE_H_
#define RTC_BASE_NETWORK_ROUTE_H_

#include <stdint.h>

#include <string>

#include "rtc_base/network_constants.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/inline.h"

namespace rtc {

class RouteEndpoint {
 public:
  RouteEndpoint() {}
  RouteEndpoint(AdapterType adapter_type,
                uint16_t adapter_id,
                uint16_t network_id,
                bool uses_turn)
      : adapter_type_(adapter_type),
        adapter_id_(adapter_id),
        network_id_(network_id),
        uses_turn_(uses_turn) {}

  RouteEndpoint(const RouteEndpoint&) = default;
  RouteEndpoint& operator=(const RouteEndpoint&) = default;

  static RouteEndpoint CreateWithNetworkId(uint16_t network_id) {
    return RouteEndpoint(ADAPTER_TYPE_UNKNOWN,
                                            0, network_id,
                                           false);
  }
  RouteEndpoint CreateWithTurn(bool uses_turn) const {
    return RouteEndpoint(adapter_type_, adapter_id_, network_id_, uses_turn);
  }

  AdapterType adapter_type() const { return adapter_type_; }
  uint16_t adapter_id() const { return adapter_id_; }
  uint16_t network_id() const { return network_id_; }
  bool uses_turn() const { return uses_turn_; }

  bool operator==(const RouteEndpoint& other) const;

 private:
  AdapterType adapter_type_ = ADAPTER_TYPE_UNKNOWN;
  uint16_t adapter_id_ = 0;
  uint16_t network_id_ = 0;
  bool uses_turn_ = false;
};

struct NetworkRoute {
  bool connected = false;
  RouteEndpoint local;
  RouteEndpoint remote;

  int last_sent_packet_id = -1;

  int packet_overhead = 0;

  RTC_NO_INLINE inline std::string DebugString() const {
    rtc::StringBuilder oss;
    oss << "[ connected: " << connected << " local: [ " << local.adapter_id()
        << "/" << local.network_id() << " "
        << AdapterTypeToString(local.adapter_type())
        << " turn: " << local.uses_turn() << " ] remote: [ "
        << remote.adapter_id() << "/" << remote.network_id() << " "
        << AdapterTypeToString(remote.adapter_type())
        << " turn: " << remote.uses_turn()
        << " ] packet_overhead_bytes: " << packet_overhead << " ]";
    return oss.Release();
  }

  bool operator==(const NetworkRoute& other) const;
};

}

#endif
