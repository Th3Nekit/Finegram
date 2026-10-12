/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TASK_QUEUE_TASK_QUEUE_BASE_H_
#define API_TASK_QUEUE_TASK_QUEUE_BASE_H_

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/location.h"
#include "api/units/time_delta.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RTC_LOCKABLE RTC_EXPORT TaskQueueBase {
 public:
  enum class DelayPrecision {

    kLow,

    kHigh,
  };

  virtual void Delete() = 0;

  void PostTask(absl::AnyInvocable<void() &&> task,
                const Location& location = Location::Current()) {
    PostTaskImpl(std::move(task), PostTaskTraits{}, location);
  }

  void PostDelayedTask(absl::AnyInvocable<void() &&> task,
                       TimeDelta delay,
                       const Location& location = Location::Current()) {
    PostDelayedTaskImpl(std::move(task), delay, PostDelayedTaskTraits{},
                        location);
  }

  void PostDelayedHighPrecisionTask(
      absl::AnyInvocable<void() &&> task,
      TimeDelta delay,
      const Location& location = Location::Current()) {
    PostDelayedTaskTraits traits;
    traits.high_precision = true;
    PostDelayedTaskImpl(std::move(task), delay, traits, location);
  }

  void PostDelayedTaskWithPrecision(
      DelayPrecision precision,
      absl::AnyInvocable<void() &&> task,
      TimeDelta delay,
      const Location& location = Location::Current()) {
    switch (precision) {
      case DelayPrecision::kLow:
        PostDelayedTask(std::move(task), delay, location);
        break;
      case DelayPrecision::kHigh:
        PostDelayedHighPrecisionTask(std::move(task), delay, location);
        break;
    }
  }

  static TaskQueueBase* Current();
  bool IsCurrent() const { return Current() == this; }

 protected:

  struct PostTaskTraits {};

  struct PostDelayedTaskTraits {

    bool high_precision = false;
  };

  class RTC_EXPORT CurrentTaskQueueSetter {
   public:
    explicit CurrentTaskQueueSetter(TaskQueueBase* task_queue);
    CurrentTaskQueueSetter(const CurrentTaskQueueSetter&) = delete;
    CurrentTaskQueueSetter& operator=(const CurrentTaskQueueSetter&) = delete;
    ~CurrentTaskQueueSetter();

   private:
    TaskQueueBase* const previous_;
  };

  virtual void PostTaskImpl(absl::AnyInvocable<void() &&> task,
                            const PostTaskTraits& traits,
                            const Location& location) = 0;

  virtual void PostDelayedTaskImpl(absl::AnyInvocable<void() &&> task,
                                   TimeDelta delay,
                                   const PostDelayedTaskTraits& traits,
                                   const Location& location) = 0;

  virtual ~TaskQueueBase() = default;
};

struct TaskQueueDeleter {
  void operator()(TaskQueueBase* task_queue) const { task_queue->Delete(); }
};

}

#endif
