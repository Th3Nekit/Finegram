/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This header file provides wrapper for common WebRTC utilities.
// Different application may need different implementations of these utilities,
// Moreover, single application may need to use WebRTC for multiple purposes,
// and thus would need to provide different utilities implementations for
// different peer connections.
// The main purpose of the `Environment` class below is to propagate references
// to those utilities to all WebRTC classes that need them.

#ifndef API_ENVIRONMENT_ENVIRONMENT_H_
#define API_ENVIRONMENT_ENVIRONMENT_H_

#include <utility>

#include "absl/base/nullability.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class Clock;
class TaskQueueFactory;
class FieldTrialsView;
class RtcEventLog;

class RTC_EXPORT Environment final {
 public:

  Environment() = delete;

  Environment(const Environment&) = default;
  Environment(Environment&&) = default;
  Environment& operator=(const Environment&) = default;
  Environment& operator=(Environment&&) = default;

  ~Environment() = default;

  const FieldTrialsView& field_trials() const;

  Clock& clock() const;

  TaskQueueFactory& task_queue_factory() const;

  RtcEventLog& event_log() const;

 private:
  friend class EnvironmentFactory;
  Environment(scoped_refptr<const rtc::RefCountedBase> storage,
              absl::Nonnull<const FieldTrialsView*> field_trials,
              absl::Nonnull<Clock*> clock,
              absl::Nonnull<TaskQueueFactory*> task_queue_factory,
              absl::Nonnull<RtcEventLog*> event_log)
      : storage_(std::move(storage)),
        field_trials_(field_trials),
        clock_(clock),
        task_queue_factory_(task_queue_factory),
        event_log_(event_log) {}

  scoped_refptr<const rtc::RefCountedBase> storage_;

  absl::Nonnull<const FieldTrialsView*> field_trials_;
  absl::Nonnull<Clock*> clock_;
  absl::Nonnull<TaskQueueFactory*> task_queue_factory_;
  absl::Nonnull<RtcEventLog*> event_log_;
};

inline const FieldTrialsView& Environment::field_trials() const {
  return *field_trials_;
}

inline Clock& Environment::clock() const {
  return *clock_;
}

inline TaskQueueFactory& Environment::task_queue_factory() const {
  return *task_queue_factory_;
}

inline RtcEventLog& Environment::event_log() const {
  return *event_log_;
}

}

#endif
