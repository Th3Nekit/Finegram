/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TASK_QUEUE_PENDING_TASK_SAFETY_FLAG_H_
#define API_TASK_QUEUE_PENDING_TASK_SAFETY_FLAG_H_

#include <utility>

#include "absl/base/nullability.h"
#include "absl/functional/any_invocable.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT PendingTaskSafetyFlag final
    : public rtc::RefCountedNonVirtual<PendingTaskSafetyFlag> {
 public:
  static rtc::scoped_refptr<PendingTaskSafetyFlag> Create();

  static rtc::scoped_refptr<PendingTaskSafetyFlag> CreateDetached();

  static rtc::scoped_refptr<PendingTaskSafetyFlag> CreateAttachedToTaskQueue(
      bool alive,
      absl::Nonnull<TaskQueueBase*> attached_queue);

  static rtc::scoped_refptr<PendingTaskSafetyFlag> CreateDetachedInactive();

  ~PendingTaskSafetyFlag() = default;

  void SetNotAlive();

  void SetAlive();
  bool alive() const;

 protected:
  explicit PendingTaskSafetyFlag(bool alive) : alive_(alive) {}
  PendingTaskSafetyFlag(bool alive,
                        absl::Nonnull<TaskQueueBase*> attached_queue)
      : alive_(alive), main_sequence_(attached_queue) {}

 private:
  static rtc::scoped_refptr<PendingTaskSafetyFlag> CreateInternal(bool alive);

  bool alive_ = true;
  RTC_NO_UNIQUE_ADDRESS SequenceChecker main_sequence_;
};

class RTC_EXPORT ScopedTaskSafety final {
 public:
  ScopedTaskSafety() = default;
  explicit ScopedTaskSafety(rtc::scoped_refptr<PendingTaskSafetyFlag> flag)
      : flag_(std::move(flag)) {}
  ~ScopedTaskSafety() { flag_->SetNotAlive(); }

  rtc::scoped_refptr<PendingTaskSafetyFlag> flag() const { return flag_; }

  void reset(rtc::scoped_refptr<PendingTaskSafetyFlag> new_flag =
                 PendingTaskSafetyFlag::Create()) {
    flag_->SetNotAlive();
    flag_ = std::move(new_flag);
  }

 private:
  rtc::scoped_refptr<PendingTaskSafetyFlag> flag_ =
      PendingTaskSafetyFlag::Create();
};

class RTC_EXPORT ScopedTaskSafetyDetached final {
 public:
  ScopedTaskSafetyDetached() = default;
  ~ScopedTaskSafetyDetached() { flag_->SetNotAlive(); }

  rtc::scoped_refptr<PendingTaskSafetyFlag> flag() const { return flag_; }

 private:
  rtc::scoped_refptr<PendingTaskSafetyFlag> flag_ =
      PendingTaskSafetyFlag::CreateDetached();
};

inline absl::AnyInvocable<void() &&> SafeTask(
    rtc::scoped_refptr<PendingTaskSafetyFlag> flag,
    absl::AnyInvocable<void() &&> task) {
  return [flag = std::move(flag), task = std::move(task)]() mutable {
    if (flag->alive()) {
      std::move(task)();
    }
  };
}

}

#endif
