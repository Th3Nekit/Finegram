/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_CONNECTION_INFO_H_
#define P2P_BASE_CONNECTION_INFO_H_

#include <vector>

#include "absl/types/optional.h"
#include "api/candidate.h"
#include "api/units/timestamp.h"

namespace cricket {

enum class IceCandidatePairState {
  WAITING = 0,
  IN_PROGRESS,
  SUCCEEDED,
  FAILED,

};

struct ConnectionInfo {
  ConnectionInfo();
  ConnectionInfo(const ConnectionInfo&);
  ~ConnectionInfo();

  bool best_connection;
  bool writable;
  bool receiving;
  bool timeout;
  size_t rtt;
  size_t sent_discarded_bytes;

  size_t sent_total_bytes;

  size_t sent_bytes_second;
  size_t sent_discarded_packets;

  size_t sent_total_packets;

  size_t sent_ping_requests_total;
  size_t sent_ping_requests_before_first_response;

  size_t sent_ping_responses;

  size_t recv_total_bytes;
  size_t recv_bytes_second;
  size_t packets_received;
  size_t recv_ping_requests;
  size_t recv_ping_responses;
  Candidate local_candidate;
  Candidate remote_candidate;
  void* key;

  IceCandidatePairState state;

  uint64_t priority;

  bool nominated;

  uint64_t total_round_trip_time_ms;

  absl::optional<uint32_t> current_round_trip_time_ms;

  absl::optional<webrtc::Timestamp> last_data_received;
  absl::optional<webrtc::Timestamp> last_data_sent;
};

typedef std::vector<ConnectionInfo> ConnectionInfos;

}

#endif
