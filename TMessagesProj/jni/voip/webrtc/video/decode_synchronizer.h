/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_DECODE_SYNCHRONIZER_H_
#define VIDEO_DECODE_SYNCHRONIZER_H_

#include <stdint.h>

#include <functional>
#include <memory>
#include <set>
#include <utility>

#include "absl/types/optional.h"
#include "api/metronome/metronome.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/thread_annotations.h"
#include "video/frame_decode_scheduler.h"
#include "video/frame_decode_timing.h"

namespace webrtc {

class DecodeSynchronizer {
 public:
  DecodeSynchronizer(Clock* clock,
                     Metronome* metronome,
                     TaskQueueBase* worker_queue);
  ~DecodeSynchronizer();
  DecodeSynchronizer(const DecodeSynchronizer&) = delete;
  DecodeSynchronizer& operator=(const DecodeSynchronizer&) = delete;

  std::unique_ptr<FrameDecodeScheduler> CreateSynchronizedFrameScheduler();

 private:
  class ScheduledFrame {
   public:
    ScheduledFrame(uint32_t rtp_timestamp,
                   FrameDecodeTiming::FrameSchedule schedule,
                   FrameDecodeScheduler::FrameReleaseCallback callback);

    ScheduledFrame(const ScheduledFrame&) = delete;
    ScheduledFrame& operator=(const ScheduledFrame&) = delete;
    ScheduledFrame(ScheduledFrame&&) = default;
    ScheduledFrame& operator=(ScheduledFrame&&) = default;

    void RunFrameReleaseCallback() &&;

    uint32_t rtp_timestamp() const { return rtp_timestamp_; }
    Timestamp LatestDecodeTime() const;

   private:
    uint32_t rtp_timestamp_;
    FrameDecodeTiming::FrameSchedule schedule_;
    FrameDecodeScheduler::FrameReleaseCallback callback_;
  };

  class SynchronizedFrameDecodeScheduler : public FrameDecodeScheduler {
   public:
    explicit SynchronizedFrameDecodeScheduler(DecodeSynchronizer* sync);
    ~SynchronizedFrameDecodeScheduler() override;

    ScheduledFrame ReleaseNextFrame();

    Timestamp LatestDecodeTime();

    absl::optional<uint32_t> ScheduledRtpTimestamp() override;
    void ScheduleFrame(uint32_t rtp,
                       FrameDecodeTiming::FrameSchedule schedule,
                       FrameReleaseCallback cb) override;
    void CancelOutstanding() override;
    void Stop() override;

   private:
    DecodeSynchronizer* sync_;
    absl::optional<ScheduledFrame> next_frame_;
    bool stopped_ = false;
  };

  void OnFrameScheduled(SynchronizedFrameDecodeScheduler* scheduler);
  void RemoveFrameScheduler(SynchronizedFrameDecodeScheduler* scheduler);

  void ScheduleNextTick();
  void OnTick();

  Clock* const clock_;
  TaskQueueBase* const worker_queue_;
  Metronome* const metronome_;

  Timestamp expected_next_tick_ = Timestamp::PlusInfinity();
  std::set<SynchronizedFrameDecodeScheduler*> schedulers_
      RTC_GUARDED_BY(worker_queue_);
  bool tick_scheduled_ = false;
  ScopedTaskSafetyDetached safety_;
};

}

#endif
