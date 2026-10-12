/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_P2P_TRANSPORT_CHANNEL_ICE_FIELD_TRIALS_H_
#define P2P_BASE_P2P_TRANSPORT_CHANNEL_ICE_FIELD_TRIALS_H_

#include "absl/types/optional.h"

namespace cricket {

struct IceFieldTrials {

  bool skip_relay_to_non_relay_connections = false;
  absl::optional<int> max_outstanding_pings;

  absl::optional<int> initial_select_dampening;

  absl::optional<int> initial_select_dampening_ping_received;

  bool announce_goog_ping = true;

  bool enable_goog_ping = false;

  int rtt_estimate_halftime_ms = 500;

  bool send_ping_on_switch_ice_controlling = false;

  bool send_ping_on_selected_ice_controlling = false;

  bool send_ping_on_nomination_ice_controlled = false;

  int dead_connection_timeout_ms = 30000;

  bool stop_gather_on_strongly_connected = true;

  absl::optional<int> override_dscp;

  bool piggyback_ice_check_acknowledgement = false;
  bool extra_ice_ping = false;

  bool enable_goog_delta = true;
  bool answer_goog_delta = true;
};

}

#endif
