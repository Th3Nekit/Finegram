/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_SOURCE_TRACKER_H_
#define MODULES_RTP_RTCP_SOURCE_SOURCE_TRACKER_H_

#include <cstdint>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/rtp_packet_infos.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/transport/rtp/rtp_source.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class SourceTracker {
 public:

  static constexpr TimeDelta kTimeout = TimeDelta::Seconds(10);

  explicit SourceTracker(Clock* clock);

  SourceTracker(const SourceTracker& other) = delete;
  SourceTracker(SourceTracker&& other) = delete;
  SourceTracker& operator=(const SourceTracker& other) = delete;
  SourceTracker& operator=(SourceTracker&& other) = delete;

  void OnFrameDelivered(RtpPacketInfos packet_infos);

  std::vector<RtpSource> GetSources() const;

 private:
  struct SourceKey {
    SourceKey(RtpSourceType source_type, uint32_t source)
        : source_type(source_type), source(source) {}

    RtpSourceType source_type;

    uint32_t source;
  };

  struct SourceKeyComparator {
    bool operator()(const SourceKey& lhs, const SourceKey& rhs) const {
      return (lhs.source_type == rhs.source_type) && (lhs.source == rhs.source);
    }
  };

  struct SourceKeyHasher {
    size_t operator()(const SourceKey& value) const {
      return static_cast<size_t>(value.source_type) +
             static_cast<size_t>(value.source) * 11076425802534262905ULL;
    }
  };

  struct SourceEntry {

    Timestamp timestamp = Timestamp::MinusInfinity();

    absl::optional<uint8_t> audio_level;

    absl::optional<AbsoluteCaptureTime> absolute_capture_time;

    absl::optional<TimeDelta> local_capture_clock_offset;

    uint32_t rtp_timestamp = 0;
  };

  using SourceList = std::list<std::pair<const SourceKey, SourceEntry>>;
  using SourceMap = std::unordered_map<SourceKey,
                                       SourceList::iterator,
                                       SourceKeyHasher,
                                       SourceKeyComparator>;

  void OnFrameDeliveredInternal(Timestamp now,
                                const RtpPacketInfos& packet_infos)
      RTC_RUN_ON(worker_thread_);

  SourceEntry& UpdateEntry(const SourceKey& key) RTC_RUN_ON(worker_thread_);

  void PruneEntries(Timestamp now) const RTC_RUN_ON(worker_thread_);

  TaskQueueBase* const worker_thread_;
  Clock* const clock_;

  mutable SourceList list_ RTC_GUARDED_BY(worker_thread_);
  mutable SourceMap map_ RTC_GUARDED_BY(worker_thread_);
  ScopedTaskSafety worker_safety_;
};

}

#endif
