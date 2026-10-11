/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TASK_QUEUE_H_
#define RTC_BASE_TASK_QUEUE_H_

#include <stdint.h>

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/memory/memory.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

namespace rtc {

class RTC_LOCKABLE RTC_EXPORT TaskQueue {
 public:

  using Priority = ::webrtc::TaskQueueFactory::Priority;

  explicit TaskQueue(std::unique_ptr<webrtc::TaskQueueBase,
                                     webrtc::TaskQueueDeleter> task_queue);
  ~TaskQueue();

  TaskQueue(const TaskQueue&) = delete;
  TaskQueue& operator=(const TaskQueue&) = delete;

  bool IsCurrent() const;

  webrtc::TaskQueueBase* Get() { return impl_; }

  void PostTask(
      absl::AnyInvocable<void() &&> task,
      const webrtc::Location& location = webrtc::Location::Current()) {
    impl_->PostTask(std::move(task), location);
  }
  void PostDelayedTask(
      absl::AnyInvocable<void() &&> task,
      webrtc::TimeDelta delay,
      const webrtc::Location& location = webrtc::Location::Current()) {
    impl_->PostDelayedTask(std::move(task), delay, location);
  }
  void PostDelayedHighPrecisionTask(
      absl::AnyInvocable<void() &&> task,
      webrtc::TimeDelta delay,
      const webrtc::Location& location = webrtc::Location::Current()) {
    impl_->PostDelayedHighPrecisionTask(std::move(task), delay, location);
  }

 private:
  webrtc::TaskQueueBase* const impl_;
};

}

#endif
