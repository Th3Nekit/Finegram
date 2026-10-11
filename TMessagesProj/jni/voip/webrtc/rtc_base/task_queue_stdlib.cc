/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_queue_stdlib.h"

#include <string.h>

#include <algorithm>
#include <map>
#include <memory>
#include <queue>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/divide_round.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace {

rtc::ThreadPriority TaskQueuePriorityToThreadPriority(
    TaskQueueFactory::Priority priority) {
  switch (priority) {
    case TaskQueueFactory::Priority::HIGH:
      return rtc::ThreadPriority::kRealtime;
    case TaskQueueFactory::Priority::LOW:
      return rtc::ThreadPriority::kLow;
    case TaskQueueFactory::Priority::NORMAL:
      return rtc::ThreadPriority::kNormal;
  }
}

class TaskQueueStdlib final : public TaskQueueBase {
 public:
  TaskQueueStdlib(absl::string_view queue_name, rtc::ThreadPriority priority);
  ~TaskQueueStdlib() override = default;

  void Delete() override;

 protected:
  void PostTaskImpl(absl::AnyInvocable<void() &&> task,
                    const PostTaskTraits& traits,
                    const Location& location) override;
  void PostDelayedTaskImpl(absl::AnyInvocable<void() &&> task,
                           TimeDelta delay,
                           const PostDelayedTaskTraits& traits,
                           const Location& location) override;

 private:
  using OrderId = uint64_t;

  struct DelayedEntryTimeout {

    int64_t next_fire_at_us{};
    OrderId order{};

    bool operator<(const DelayedEntryTimeout& o) const {
      return std::tie(next_fire_at_us, order) <
             std::tie(o.next_fire_at_us, o.order);
    }
  };

  struct NextTask {
    bool final_task = false;
    absl::AnyInvocable<void() &&> run_task;
    TimeDelta sleep_time = rtc::Event::kForever;
  };

  static rtc::PlatformThread InitializeThread(TaskQueueStdlib* me,
                                              absl::string_view queue_name,
                                              rtc::ThreadPriority priority);

  NextTask GetNextTask();

  void ProcessTasks();

  void NotifyWake();

  rtc::Event flag_notify_;

  Mutex pending_lock_;

  bool thread_should_quit_ RTC_GUARDED_BY(pending_lock_) = false;

  OrderId thread_posting_order_ RTC_GUARDED_BY(pending_lock_) = 0;

  std::queue<std::pair<OrderId, absl::AnyInvocable<void() &&>>> pending_queue_
      RTC_GUARDED_BY(pending_lock_);

  std::map<DelayedEntryTimeout, absl::AnyInvocable<void() &&>> delayed_queue_
      RTC_GUARDED_BY(pending_lock_);

  rtc::PlatformThread thread_;
};

TaskQueueStdlib::TaskQueueStdlib(absl::string_view queue_name,
                                 rtc::ThreadPriority priority)
    : flag_notify_(                 false,                        false),
      thread_(InitializeThread(this, queue_name, priority)) {}

rtc::PlatformThread TaskQueueStdlib::InitializeThread(
    TaskQueueStdlib* me,
    absl::string_view queue_name,
    rtc::ThreadPriority priority) {
  rtc::Event started;
  auto thread = rtc::PlatformThread::SpawnJoinable(
      [&started, me] {
        CurrentTaskQueueSetter set_current(me);
        started.Set();
        me->ProcessTasks();
      },
      queue_name, rtc::ThreadAttributes().SetPriority(priority));
  started.Wait(rtc::Event::kForever);
  return thread;
}

void TaskQueueStdlib::Delete() {
  RTC_DCHECK(!IsCurrent());

  {
    MutexLock lock(&pending_lock_);
    thread_should_quit_ = true;
  }

  NotifyWake();

  delete this;
}

void TaskQueueStdlib::PostTaskImpl(absl::AnyInvocable<void() &&> task,
                                   const PostTaskTraits& traits,
                                   const Location& location) {
  {
    MutexLock lock(&pending_lock_);
    pending_queue_.push(
        std::make_pair(++thread_posting_order_, std::move(task)));
  }

  NotifyWake();
}

void TaskQueueStdlib::PostDelayedTaskImpl(absl::AnyInvocable<void() &&> task,
                                          TimeDelta delay,
                                          const PostDelayedTaskTraits& traits,
                                          const Location& location) {
  DelayedEntryTimeout delayed_entry;
  delayed_entry.next_fire_at_us = rtc::TimeMicros() + delay.us();

  {
    MutexLock lock(&pending_lock_);
    delayed_entry.order = ++thread_posting_order_;
    delayed_queue_[delayed_entry] = std::move(task);
  }

  NotifyWake();
}

TaskQueueStdlib::NextTask TaskQueueStdlib::GetNextTask() {
  NextTask result;

  const int64_t tick_us = rtc::TimeMicros();

  MutexLock lock(&pending_lock_);

  if (thread_should_quit_) {
    result.final_task = true;
    return result;
  }

  if (delayed_queue_.size() > 0) {
    auto delayed_entry = delayed_queue_.begin();
    const auto& delay_info = delayed_entry->first;
    auto& delay_run = delayed_entry->second;
    if (tick_us >= delay_info.next_fire_at_us) {
      if (pending_queue_.size() > 0) {
        auto& entry = pending_queue_.front();
        auto& entry_order = entry.first;
        auto& entry_run = entry.second;
        if (entry_order < delay_info.order) {
          result.run_task = std::move(entry_run);
          pending_queue_.pop();
          return result;
        }
      }

      result.run_task = std::move(delay_run);
      delayed_queue_.erase(delayed_entry);
      return result;
    }

    result.sleep_time = TimeDelta::Millis(
        DivideRoundUp(delay_info.next_fire_at_us - tick_us, 1'000));
  }

  if (pending_queue_.size() > 0) {
    auto& entry = pending_queue_.front();
    result.run_task = std::move(entry.second);
    pending_queue_.pop();
  }

  return result;
}

void TaskQueueStdlib::ProcessTasks() {
  while (true) {
    auto task = GetNextTask();

    if (task.final_task)
      break;

    if (task.run_task) {

      std::move(task.run_task)();

      continue;
    }

    flag_notify_.Wait(task.sleep_time);
  }

  std::queue<std::pair<OrderId, absl::AnyInvocable<void() &&>>> pending_queue;
  {
    MutexLock lock(&pending_lock_);
    pending_queue_.swap(pending_queue);
  }
  pending_queue = {};
#if RTC_DCHECK_IS_ON
  MutexLock lock(&pending_lock_);
  RTC_DCHECK(pending_queue_.empty());
#endif
}

void TaskQueueStdlib::NotifyWake() {

  flag_notify_.Set();
}

class TaskQueueStdlibFactory final : public TaskQueueFactory {
 public:
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      Priority priority) const override {
    return std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(
        new TaskQueueStdlib(name, TaskQueuePriorityToThreadPriority(priority)));
  }
};

}

std::unique_ptr<TaskQueueFactory> CreateTaskQueueStdlibFactory() {
  return std::make_unique<TaskQueueStdlibFactory>();
}

}
