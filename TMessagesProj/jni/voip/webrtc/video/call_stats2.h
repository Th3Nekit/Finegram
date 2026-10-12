/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CALL_STATS2_H_
#define VIDEO_CALL_STATS2_H_

#include <list>
#include <memory>

#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/timestamp.h"
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace internal {

class CallStats {
 public:

  static constexpr TimeDelta kUpdateInterval = TimeDelta::Millis(1000);

  CallStats(Clock* clock, TaskQueueBase* task_queue);
  ~CallStats();

  CallStats(const CallStats&) = delete;
  CallStats& operator=(const CallStats&) = delete;

  void EnsureStarted();

  RtcpRttStats* AsRtcpRttStats() { return &rtcp_rtt_stats_impl_; }

  void RegisterStatsObserver(CallStatsObserver* observer);
  void DeregisterStatsObserver(CallStatsObserver* observer);

  int64_t LastProcessedRtt() const;

  void UpdateHistogramsForTest() { UpdateHistograms(); }

  struct RttTime {
    RttTime(int64_t new_rtt, int64_t rtt_time) : rtt(new_rtt), time(rtt_time) {}
    const int64_t rtt;
    const int64_t time;
  };

 private:

  void OnRttUpdate(int64_t rtt);

  void UpdateAndReport();

  void UpdateHistograms();

  class RtcpRttStatsImpl : public RtcpRttStats {
   public:
    explicit RtcpRttStatsImpl(CallStats* owner) : owner_(owner) {}
    ~RtcpRttStatsImpl() override = default;

   private:
    void OnRttUpdate(int64_t rtt) override {

      owner_->OnRttUpdate(rtt);
    }

    int64_t LastProcessedRtt() const override {

      RTC_DCHECK_NOTREACHED() << "Legacy call path";
      return 0;
    }

    CallStats* const owner_;
  } rtcp_rtt_stats_impl_{this};

  Clock* const clock_;

  RepeatingTaskHandle repeating_task_ RTC_GUARDED_BY(task_queue_);

  int64_t max_rtt_ms_ RTC_GUARDED_BY(task_queue_);

  int64_t avg_rtt_ms_ RTC_GUARDED_BY(task_queue_);

  int64_t sum_avg_rtt_ms_ RTC_GUARDED_BY(task_queue_);
  int64_t num_avg_rtt_ RTC_GUARDED_BY(task_queue_);
  int64_t time_of_first_rtt_ms_ RTC_GUARDED_BY(task_queue_);

  std::list<RttTime> reports_ RTC_GUARDED_BY(task_queue_);

  std::list<CallStatsObserver*> observers_ RTC_GUARDED_BY(task_queue_);

  TaskQueueBase* const task_queue_;

  ScopedTaskSafety task_safety_;
};

}
}

#endif
