/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_USAGE_PATTERN_H_
#define PC_USAGE_PATTERN_H_

#include "api/peer_connection_interface.h"

namespace webrtc {

class PeerConnectionObserver;

enum class UsageEvent : int {
  TURN_SERVER_ADDED = 0x01,
  STUN_SERVER_ADDED = 0x02,
  DATA_ADDED = 0x04,
  AUDIO_ADDED = 0x08,
  VIDEO_ADDED = 0x10,

  SET_LOCAL_DESCRIPTION_SUCCEEDED = 0x20,

  SET_REMOTE_DESCRIPTION_SUCCEEDED = 0x40,

  CANDIDATE_COLLECTED = 0x80,

  ADD_ICE_CANDIDATE_SUCCEEDED = 0x100,
  ICE_STATE_CONNECTED = 0x200,
  CLOSE_CALLED = 0x400,

  PRIVATE_CANDIDATE_COLLECTED = 0x800,

  REMOTE_PRIVATE_CANDIDATE_ADDED = 0x1000,

  MDNS_CANDIDATE_COLLECTED = 0x2000,

  REMOTE_MDNS_CANDIDATE_ADDED = 0x4000,

  IPV6_CANDIDATE_COLLECTED = 0x8000,

  REMOTE_IPV6_CANDIDATE_ADDED = 0x10000,

  REMOTE_CANDIDATE_ADDED = 0x20000,

  DIRECT_CONNECTION_SELECTED = 0x40000,
  MAX_VALUE = 0x80000,
};

class UsagePattern {
 public:
  void NoteUsageEvent(UsageEvent event);
  void ReportUsagePattern(PeerConnectionObserver* observer) const;

 private:
  int usage_event_accumulator_ = 0;
};

}
#endif
