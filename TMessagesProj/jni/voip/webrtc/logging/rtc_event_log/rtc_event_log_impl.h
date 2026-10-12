/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_IMPL_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_IMPL_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/rtc_event_log/rtc_event.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/rtc_event_log_output.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RtcEventLogImpl final : public RtcEventLog {
 public:

  static constexpr size_t kMaxEventsInHistory = 10000;

  static constexpr size_t kMaxEventsInConfigHistory = 1000;

  explicit RtcEventLogImpl(const Environment& env);
  RtcEventLogImpl(
      std::unique_ptr<RtcEventLogEncoder> encoder,
      TaskQueueFactory* task_queue_factory,
      size_t max_events_in_history = kMaxEventsInHistory,
      size_t max_config_events_in_history = kMaxEventsInConfigHistory);
  RtcEventLogImpl(const RtcEventLogImpl&) = delete;
  RtcEventLogImpl& operator=(const RtcEventLogImpl&) = delete;

  ~RtcEventLogImpl() override;

  bool StartLogging(std::unique_ptr<RtcEventLogOutput> output,
                    int64_t output_period_ms) override;
  void StopLogging() override;
  void StopLogging(std::function<void()> callback) override;

  void Log(std::unique_ptr<RtcEvent> event) override;

 private:
  using EventDeque = std::deque<std::unique_ptr<RtcEvent>>;

  struct EventHistories {
    EventDeque config_history;
    EventDeque history;
  };

  EventHistories ExtractRecentHistories() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void LogToMemory(std::unique_ptr<RtcEvent> event)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void LogEventsToOutput(EventHistories histories) RTC_RUN_ON(task_queue_);

  void StopOutput() RTC_RUN_ON(task_queue_);

  void WriteConfigsAndHistoryToOutput(absl::string_view encoded_configs,
                                      absl::string_view encoded_history)
      RTC_RUN_ON(task_queue_);
  void WriteToOutput(absl::string_view output_string) RTC_RUN_ON(task_queue_);

  void StopLoggingInternal() RTC_RUN_ON(task_queue_);

  bool ShouldOutputImmediately() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void ScheduleOutput() RTC_RUN_ON(task_queue_);

  const size_t max_events_in_history_;

  const size_t max_config_events_in_history_;

  EventDeque all_config_history_ RTC_GUARDED_BY(task_queue_);

  EventHistories recent_ RTC_GUARDED_BY(mutex_);

  std::unique_ptr<RtcEventLogEncoder> event_encoder_
      RTC_GUARDED_BY(task_queue_);
  std::unique_ptr<RtcEventLogOutput> event_output_ RTC_GUARDED_BY(task_queue_);

  int64_t output_period_ms_ RTC_GUARDED_BY(task_queue_);
  int64_t last_output_ms_ RTC_GUARDED_BY(task_queue_);

  RTC_NO_UNIQUE_ADDRESS SequenceChecker logging_state_checker_;
  bool logging_state_started_ RTC_GUARDED_BY(mutex_) = false;
  bool immediately_output_mode_ RTC_GUARDED_BY(mutex_) = false;
  bool need_schedule_output_ RTC_GUARDED_BY(mutex_) = false;

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue_;

  Mutex mutex_;
};

}

#endif
