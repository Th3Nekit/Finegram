/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/remote_bitrate_estimator/packet_arrival_map.h"

#include <algorithm>
#include <cstdint>

#include "api/units/timestamp.h"
#include "rtc_base/checks.h"

namespace webrtc {

void PacketArrivalTimeMap::AddPacket(int64_t sequence_number,
                                     Timestamp arrival_time) {
  RTC_DCHECK_GE(arrival_time, Timestamp::Zero());
  if (!has_seen_packet()) {

    Reallocate(kMinCapacity);
    begin_sequence_number_ = sequence_number;
    end_sequence_number_ = sequence_number + 1;
    arrival_times_[Index(sequence_number)] = arrival_time;
    return;
  }

  if (sequence_number >= begin_sequence_number() &&
      sequence_number < end_sequence_number()) {

    arrival_times_[Index(sequence_number)] = arrival_time;
    return;
  }

  if (sequence_number < begin_sequence_number()) {

    int64_t new_size = end_sequence_number() - sequence_number;
    if (new_size > kMaxNumberOfPackets) {

      return;
    }
    AdjustToSize(new_size);

    arrival_times_[Index(sequence_number)] = arrival_time;
    SetNotReceived(sequence_number + 1, begin_sequence_number_);
    begin_sequence_number_ = sequence_number;
    return;
  }

  RTC_DCHECK_GE(sequence_number, end_sequence_number_);
  int64_t new_end_sequence_number = sequence_number + 1;

  if (new_end_sequence_number >= end_sequence_number_ + kMaxNumberOfPackets) {

    begin_sequence_number_ = sequence_number;
    end_sequence_number_ = new_end_sequence_number;
    arrival_times_[Index(sequence_number)] = arrival_time;
    return;
  }

  if (begin_sequence_number_ < new_end_sequence_number - kMaxNumberOfPackets) {

    begin_sequence_number_ = new_end_sequence_number - kMaxNumberOfPackets;
    RTC_DCHECK_GT(end_sequence_number_, begin_sequence_number_);
  }

  AdjustToSize(new_end_sequence_number - begin_sequence_number_);

  SetNotReceived(end_sequence_number_, sequence_number);
  end_sequence_number_ = new_end_sequence_number;
  arrival_times_[Index(sequence_number)] = arrival_time;
}

void PacketArrivalTimeMap::SetNotReceived(
    int64_t begin_sequence_number_inclusive,
    int64_t end_sequence_number_exclusive) {
  static constexpr Timestamp value = Timestamp::MinusInfinity();

  int begin_index = Index(begin_sequence_number_inclusive);
  int end_index = Index(end_sequence_number_exclusive);

  if (begin_index <= end_index) {

    std::fill(arrival_times_.get() + begin_index,
              arrival_times_.get() + end_index, value);
  } else {

    std::fill(arrival_times_.get() + begin_index,
              arrival_times_.get() + capacity(), value);
    std::fill(arrival_times_.get(), arrival_times_.get() + end_index, value);
  }
}

void PacketArrivalTimeMap::RemoveOldPackets(int64_t sequence_number,
                                            Timestamp arrival_time_limit) {
  int64_t check_to = std::min(sequence_number, end_sequence_number_);
  while (begin_sequence_number_ < check_to &&
         arrival_times_[Index(begin_sequence_number_)] <= arrival_time_limit) {
    ++begin_sequence_number_;
  }
  AdjustToSize(end_sequence_number_ - begin_sequence_number_);
}

void PacketArrivalTimeMap::EraseTo(int64_t sequence_number) {
  if (sequence_number < begin_sequence_number_) {
    return;
  }
  if (sequence_number >= end_sequence_number_) {

    begin_sequence_number_ = end_sequence_number_;
    return;
  }

  begin_sequence_number_ = sequence_number;
  AdjustToSize(end_sequence_number_ - begin_sequence_number_);
}

void PacketArrivalTimeMap::AdjustToSize(int new_size) {
  if (new_size > capacity()) {
    int new_capacity = capacity();
    while (new_capacity < new_size)
      new_capacity *= 2;
    Reallocate(new_capacity);
  }
  if (capacity() > std::max(kMinCapacity, 4 * new_size)) {
    int new_capacity = capacity();
    while (new_capacity > 2 * std::max(new_size, kMinCapacity)) {
      new_capacity /= 2;
    }
    Reallocate(new_capacity);
  }
  RTC_DCHECK_LE(new_size, capacity());
}

void PacketArrivalTimeMap::Reallocate(int new_capacity) {
  int new_capacity_minus_1 = new_capacity - 1;

  RTC_DCHECK_EQ(new_capacity & new_capacity_minus_1, 0);

  void* raw = operator new[](new_capacity * sizeof(Timestamp));
  Timestamp* new_buffer = static_cast<Timestamp*>(raw);

  for (int64_t sequence_number = begin_sequence_number_;
       sequence_number < end_sequence_number_; ++sequence_number) {
    new_buffer[sequence_number & new_capacity_minus_1] =
        arrival_times_[sequence_number & capacity_minus_1_];
  }
  arrival_times_.reset(new_buffer);
  capacity_minus_1_ = new_capacity_minus_1;
}

}
