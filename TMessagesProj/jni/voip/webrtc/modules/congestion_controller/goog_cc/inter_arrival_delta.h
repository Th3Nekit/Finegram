/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_INTER_ARRIVAL_DELTA_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_INTER_ARRIVAL_DELTA_H_

#include <cstddef>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {

class InterArrivalDelta {
 public:

  static constexpr int kReorderedResetThreshold = 3;
  static constexpr TimeDelta kArrivalTimeOffsetThreshold =
      TimeDelta::Seconds(3);

  explicit InterArrivalDelta(TimeDelta send_time_group_length);

  InterArrivalDelta() = delete;
  InterArrivalDelta(const InterArrivalDelta&) = delete;
  InterArrivalDelta& operator=(const InterArrivalDelta&) = delete;

  bool ComputeDeltas(Timestamp send_time,
                     Timestamp arrival_time,
                     Timestamp system_time,
                     size_t packet_size,
                     TimeDelta* send_time_delta,
                     TimeDelta* arrival_time_delta,
                     int* packet_size_delta);

 private:
  struct SendTimeGroup {
    SendTimeGroup()
        : size(0),
          first_send_time(Timestamp::MinusInfinity()),
          send_time(Timestamp::MinusInfinity()),
          first_arrival(Timestamp::MinusInfinity()),
          complete_time(Timestamp::MinusInfinity()),
          last_system_time(Timestamp::MinusInfinity()) {}

    bool IsFirstPacket() const { return complete_time.IsInfinite(); }

    size_t size;
    Timestamp first_send_time;
    Timestamp send_time;
    Timestamp first_arrival;
    Timestamp complete_time;
    Timestamp last_system_time;
  };

  bool NewTimestampGroup(Timestamp arrival_time, Timestamp send_time) const;

  bool BelongsToBurst(Timestamp arrival_time, Timestamp send_time) const;

  void Reset();

  const TimeDelta send_time_group_length_;
  SendTimeGroup current_timestamp_group_;
  SendTimeGroup prev_timestamp_group_;
  int num_consecutive_reordered_packets_;
};
}

#endif
