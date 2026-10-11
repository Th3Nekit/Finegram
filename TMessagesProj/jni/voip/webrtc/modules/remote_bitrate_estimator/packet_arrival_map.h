/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_PACKET_ARRIVAL_MAP_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_PACKET_ARRIVAL_MAP_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "api/units/timestamp.h"
#include "rtc_base/checks.h"

namespace webrtc {

class PacketArrivalTimeMap {
 public:
  struct PacketArrivalTime {
    Timestamp arrival_time;
    int64_t sequence_number;
  };

  static constexpr int kMaxNumberOfPackets = (1 << 15);

  PacketArrivalTimeMap() = default;
  PacketArrivalTimeMap(const PacketArrivalTimeMap&) = delete;
  PacketArrivalTimeMap& operator=(const PacketArrivalTimeMap&) = delete;
  ~PacketArrivalTimeMap() = default;

  bool has_received(int64_t sequence_number) const {
    return sequence_number >= begin_sequence_number() &&
           sequence_number < end_sequence_number() &&
           arrival_times_[Index(sequence_number)] >= Timestamp::Zero();
  }

  int64_t begin_sequence_number() const { return begin_sequence_number_; }

  int64_t end_sequence_number() const { return end_sequence_number_; }

  Timestamp get(int64_t sequence_number) {
    RTC_DCHECK_GE(sequence_number, begin_sequence_number());
    RTC_DCHECK_LT(sequence_number, end_sequence_number());
    return arrival_times_[Index(sequence_number)];
  }

  PacketArrivalTime FindNextAtOrAfter(int64_t sequence_number) const {
    RTC_DCHECK_GE(sequence_number, begin_sequence_number());
    RTC_DCHECK_LT(sequence_number, end_sequence_number());
    while (true) {
      Timestamp t = arrival_times_[Index(sequence_number)];
      if (t >= Timestamp::Zero()) {
        return {.arrival_time = t, .sequence_number = sequence_number};
      }
      ++sequence_number;
    }
  }

  int64_t clamp(int64_t sequence_number) const {
    return std::clamp(sequence_number, begin_sequence_number(),
                      end_sequence_number());
  }

  void EraseTo(int64_t sequence_number);

  void AddPacket(int64_t sequence_number, Timestamp arrival_time);

  void RemoveOldPackets(int64_t sequence_number, Timestamp arrival_time_limit);

 private:
  static constexpr int kMinCapacity = 128;

  int Index(int64_t sequence_number) const {

    return sequence_number & capacity_minus_1_;
  }

  void SetNotReceived(int64_t begin_sequence_number_inclusive,
                      int64_t end_sequence_number_exclusive);

  void AdjustToSize(int new_size);
  void Reallocate(int new_capacity);

  int capacity() const { return capacity_minus_1_ + 1; }
  bool has_seen_packet() const { return arrival_times_ != nullptr; }

  std::unique_ptr<Timestamp[]> arrival_times_ = nullptr;

  int capacity_minus_1_ = -1;

  int64_t begin_sequence_number_ = 0;
  int64_t end_sequence_number_ = 0;
};

}

#endif
