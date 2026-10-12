/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_TIMER_TASK_QUEUE_TIMEOUT_H_
#define NET_DCSCTP_TIMER_TASK_QUEUE_TIMEOUT_H_

#include <memory>
#include <utility>

#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/timestamp.h"
#include "net/dcsctp/public/timeout.h"

namespace dcsctp {

class TaskQueueTimeoutFactory {
 public:

  TaskQueueTimeoutFactory(webrtc::TaskQueueBase& task_queue,
                          std::function<TimeMs()> get_time,
                          std::function<void(TimeoutID timeout_id)> on_expired)
      : task_queue_(task_queue),
        get_time_(std::move(get_time)),
        on_expired_(std::move(on_expired)) {}

  std::unique_ptr<Timeout> CreateTimeout(
      webrtc::TaskQueueBase::DelayPrecision precision =
          webrtc::TaskQueueBase::DelayPrecision::kLow) {
    return std::make_unique<TaskQueueTimeout>(*this, precision);
  }

 private:
  class TaskQueueTimeout : public Timeout {
   public:
    TaskQueueTimeout(TaskQueueTimeoutFactory& parent,
                     webrtc::TaskQueueBase::DelayPrecision precision);
    ~TaskQueueTimeout();

    void Start(DurationMs duration_ms, TimeoutID timeout_id) override;
    void Stop() override;

   private:
    TaskQueueTimeoutFactory& parent_;
    const webrtc::TaskQueueBase::DelayPrecision precision_;

    rtc::scoped_refptr<webrtc::PendingTaskSafetyFlag> pending_task_safety_flag_;

    webrtc::Timestamp posted_task_expiration_ =
        webrtc::Timestamp::PlusInfinity();

    webrtc::Timestamp timeout_expiration_ = webrtc::Timestamp::PlusInfinity();

    TimeoutID timeout_id_ = TimeoutID(0);
  };

  webrtc::Timestamp Now() { return webrtc::Timestamp::Millis(*get_time_()); }

  RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker thread_checker_;
  webrtc::TaskQueueBase& task_queue_;
  const std::function<TimeMs()> get_time_;
  const std::function<void(TimeoutID)> on_expired_;
};
}

#endif
