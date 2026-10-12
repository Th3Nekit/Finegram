/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/timer/task_queue_timeout.h"

#include "api/task_queue/pending_task_safety_flag.h"
#include "api/units/time_delta.h"
#include "rtc_base/logging.h"

namespace dcsctp {
using ::webrtc::TimeDelta;
using ::webrtc::Timestamp;

TaskQueueTimeoutFactory::TaskQueueTimeout::TaskQueueTimeout(
    TaskQueueTimeoutFactory& parent,
    webrtc::TaskQueueBase::DelayPrecision precision)
    : parent_(parent),
      precision_(precision),
      pending_task_safety_flag_(webrtc::PendingTaskSafetyFlag::Create()) {}

TaskQueueTimeoutFactory::TaskQueueTimeout::~TaskQueueTimeout() {
  RTC_DCHECK_RUN_ON(&parent_.thread_checker_);
  pending_task_safety_flag_->SetNotAlive();
}

void TaskQueueTimeoutFactory::TaskQueueTimeout::Start(DurationMs duration_ms,
                                                      TimeoutID timeout_id) {
  RTC_DCHECK_RUN_ON(&parent_.thread_checker_);
  RTC_DCHECK(timeout_expiration_.IsPlusInfinity());
  timeout_expiration_ = parent_.Now() + duration_ms.ToTimeDelta();
  timeout_id_ = timeout_id;

  if (timeout_expiration_ >= posted_task_expiration_) {

    return;
  }

  if (!posted_task_expiration_.IsPlusInfinity()) {
    RTC_DLOG(LS_VERBOSE) << "New timeout duration is less than scheduled - "
                            "ghosting old delayed task.";

    pending_task_safety_flag_->SetNotAlive();
    pending_task_safety_flag_ = webrtc::PendingTaskSafetyFlag::Create();
  }

  posted_task_expiration_ = timeout_expiration_;
  parent_.task_queue_.PostDelayedTaskWithPrecision(
      precision_,
      webrtc::SafeTask(
          pending_task_safety_flag_,
          [timeout_id, this]() {
            RTC_DLOG(LS_VERBOSE) << "Timout expired: " << timeout_id.value();
            RTC_DCHECK_RUN_ON(&parent_.thread_checker_);
            RTC_DCHECK(!posted_task_expiration_.IsPlusInfinity());
            posted_task_expiration_ = Timestamp::PlusInfinity();

            if (timeout_expiration_.IsPlusInfinity()) {

            } else {

              TimeDelta remaining = timeout_expiration_ - parent_.Now();
              timeout_expiration_ = Timestamp::PlusInfinity();
              if (remaining > TimeDelta::Zero()) {
                Start(DurationMs(remaining.ms()), timeout_id_);
              } else {

                RTC_DLOG(LS_VERBOSE)
                    << "Timout triggered: " << timeout_id.value();
                parent_.on_expired_(timeout_id_);
              }
            }
          }),
      webrtc::TimeDelta::Millis(duration_ms.value()));
}

void TaskQueueTimeoutFactory::TaskQueueTimeout::Stop() {

  RTC_DCHECK_RUN_ON(&parent_.thread_checker_);
  timeout_expiration_ = Timestamp::PlusInfinity();
}

}
