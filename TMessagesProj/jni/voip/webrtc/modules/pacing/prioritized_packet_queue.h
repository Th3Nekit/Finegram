/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_PRIORITIZED_PACKET_QUEUE_H_
#define MODULES_PACING_PRIORITIZED_PACKET_QUEUE_H_

#include <stddef.h>

#include <array>
#include <deque>
#include <list>
#include <memory>
#include <unordered_map>

#include "absl/container/inlined_vector.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"

namespace webrtc {

struct PacketQueueTTL {
  TimeDelta audio_retransmission = TimeDelta::PlusInfinity();
  TimeDelta video_retransmission = TimeDelta::PlusInfinity();
  TimeDelta video = TimeDelta::PlusInfinity();
};

class PrioritizedPacketQueue {
 public:
  explicit PrioritizedPacketQueue(
      Timestamp creation_time,
      bool prioritize_audio_retransmission = false,
      PacketQueueTTL packet_queue_ttl = PacketQueueTTL());
  PrioritizedPacketQueue(const PrioritizedPacketQueue&) = delete;
  PrioritizedPacketQueue& operator=(const PrioritizedPacketQueue&) = delete;

  void Push(Timestamp enqueue_time, std::unique_ptr<RtpPacketToSend> packet);

  std::unique_ptr<RtpPacketToSend> Pop();

  int SizeInPackets() const;

  DataSize SizeInPayloadBytes() const;

  bool Empty() const;

  const std::array<int, kNumMediaTypes>& SizeInPacketsPerRtpPacketMediaType()
      const;

  Timestamp LeadingPacketEnqueueTime(RtpPacketMediaType type) const;
  Timestamp LeadingPacketEnqueueTimeForRetransmission() const;

  Timestamp OldestEnqueueTime() const;

  TimeDelta AverageQueueTime() const;

  void UpdateAverageQueueTime(Timestamp now);

  void SetPauseState(bool paused, Timestamp now);

  void RemovePacketsForSsrc(uint32_t ssrc);

  bool HasKeyframePackets(uint32_t ssrc) const;

 private:
  static constexpr int kNumPriorityLevels = 5;

  class QueuedPacket {
   public:
    DataSize PacketSize() const;

    std::unique_ptr<RtpPacketToSend> packet;
    Timestamp enqueue_time;
    std::list<Timestamp>::iterator enqueue_time_iterator;
  };

  class StreamQueue {
   public:
    explicit StreamQueue(Timestamp creation_time);
    StreamQueue(StreamQueue&&) = default;
    StreamQueue& operator=(StreamQueue&&) = default;

    StreamQueue(const StreamQueue&) = delete;
    StreamQueue& operator=(const StreamQueue&) = delete;

    bool EnqueuePacket(QueuedPacket packet, int priority_level);

    QueuedPacket DequeuePacket(int priority_level);

    bool HasPacketsAtPrio(int priority_level) const;
    bool IsEmpty() const;
    Timestamp LeadingPacketEnqueueTime(int priority_level) const;
    Timestamp LastEnqueueTime() const;
    bool has_keyframe_packets() const { return num_keyframe_packets_ > 0; }

    std::array<std::deque<QueuedPacket>, kNumPriorityLevels> DequeueAll();

   private:
    std::deque<QueuedPacket> packets_[kNumPriorityLevels];
    Timestamp last_enqueue_time_;
    int num_keyframe_packets_;
  };

  void DequeuePacketInternal(QueuedPacket& packet);

  void MaybeUpdateTopPrioLevel();

  void PurgeOldPacketsAtPriorityLevel(int prio_level, Timestamp now);

  static absl::InlinedVector<TimeDelta, kNumPriorityLevels> ToTtlPerPrio(
      PacketQueueTTL);

  const bool prioritize_audio_retransmission_;
  const absl::InlinedVector<TimeDelta, kNumPriorityLevels>
      time_to_live_per_prio_;

  TimeDelta queue_time_sum_;

  TimeDelta pause_time_sum_;

  int size_packets_;

  std::array<int, kNumMediaTypes> size_packets_per_media_type_;

  DataSize size_payload_;

  Timestamp last_update_time_;
  bool paused_;

  Timestamp last_culling_time_;

  std::unordered_map<uint32_t, std::unique_ptr<StreamQueue>> streams_;

  std::deque<StreamQueue*> streams_by_prio_[kNumPriorityLevels];

  int top_active_prio_level_;

  std::list<Timestamp> enqueue_times_;
};

}

#endif
