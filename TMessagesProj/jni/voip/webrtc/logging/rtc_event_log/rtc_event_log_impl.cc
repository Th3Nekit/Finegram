/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/rtc_event_log_impl.h"

#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/field_trials_view.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_legacy.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_new_format.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace {

std::unique_ptr<RtcEventLogEncoder> CreateEncoder(const Environment& env) {
  if (env.field_trials().IsDisabled("WebRTC-RtcEventLogNewFormat")) {
    RTC_DLOG(LS_INFO) << "Creating legacy encoder for RTC event log.";
    return std::make_unique<RtcEventLogEncoderLegacy>();
  } else {
    RTC_DLOG(LS_INFO) << "Creating new format encoder for RTC event log.";
    return std::make_unique<RtcEventLogEncoderNewFormat>(env.field_trials());
  }
}

}

RtcEventLogImpl::RtcEventLogImpl(const Environment& env)
    : RtcEventLogImpl(CreateEncoder(env), &env.task_queue_factory()) {}

RtcEventLogImpl::RtcEventLogImpl(std::unique_ptr<RtcEventLogEncoder> encoder,
                                 TaskQueueFactory* task_queue_factory,
                                 size_t max_events_in_history,
                                 size_t max_config_events_in_history)
    : max_events_in_history_(max_events_in_history),
      max_config_events_in_history_(max_config_events_in_history),
      event_encoder_(std::move(encoder)),
      last_output_ms_(rtc::TimeMillis()),
      task_queue_(task_queue_factory->CreateTaskQueue(
          "rtc_event_log",
          TaskQueueFactory::Priority::NORMAL)) {}

RtcEventLogImpl::~RtcEventLogImpl() {

  mutex_.Lock();
  bool started = logging_state_started_;
  mutex_.Unlock();

  if (started) {
    logging_state_checker_.Detach();
    StopLogging();
  }

  task_queue_.get_deleter()(task_queue_.get());
  task_queue_.release();
}

bool RtcEventLogImpl::StartLogging(std::unique_ptr<RtcEventLogOutput> output,
                                   int64_t output_period_ms) {
  RTC_DCHECK(output);
  RTC_DCHECK(output_period_ms == kImmediateOutput || output_period_ms > 0);

  if (!output->IsActive()) {

    return false;
  }

  const int64_t timestamp_us = rtc::TimeMillis() * 1000;
  const int64_t utc_time_us = rtc::TimeUTCMillis() * 1000;
  RTC_LOG(LS_INFO) << "Starting WebRTC event log. (Timestamp, UTC) = ("
                   << timestamp_us << ", " << utc_time_us << ").";

  RTC_DCHECK_RUN_ON(&logging_state_checker_);
  MutexLock lock(&mutex_);
  logging_state_started_ = true;
  immediately_output_mode_ = (output_period_ms == kImmediateOutput);
  need_schedule_output_ = (output_period_ms != kImmediateOutput);

  task_queue_->PostTask([this, output_period_ms, timestamp_us, utc_time_us,
                         output = std::move(output),
                         histories = ExtractRecentHistories()]() mutable {
    RTC_DCHECK_RUN_ON(task_queue_.get());
    RTC_DCHECK(output);
    RTC_DCHECK(output->IsActive());
    output_period_ms_ = output_period_ms;
    event_output_ = std::move(output);

    WriteToOutput(event_encoder_->EncodeLogStart(timestamp_us, utc_time_us));

    if (!all_config_history_.empty()) {
      EventDeque& history = histories.config_history;
      history.insert(history.begin(),
                     std::make_move_iterator(all_config_history_.begin()),
                     std::make_move_iterator(all_config_history_.end()));
      all_config_history_.clear();

      if (history.size() > max_config_events_in_history_) {
        RTC_LOG(LS_WARNING)
            << "Dropping config events: " << history.size()
            << " exceeds maximum " << max_config_events_in_history_;
        history.erase(history.begin(), history.begin() + history.size() -
                                           max_config_events_in_history_);
      }
    }
    LogEventsToOutput(std::move(histories));
  });

  return true;
}

void RtcEventLogImpl::StopLogging() {
  RTC_DLOG(LS_INFO) << "Stopping WebRTC event log.";

  rtc::Event output_stopped;
  StopLogging([&output_stopped]() { output_stopped.Set(); });
  output_stopped.Wait(rtc::Event::kForever);

  RTC_DLOG(LS_INFO) << "WebRTC event log successfully stopped.";
}

void RtcEventLogImpl::StopLogging(std::function<void()> callback) {
  RTC_DCHECK_RUN_ON(&logging_state_checker_);
  MutexLock lock(&mutex_);
  logging_state_started_ = false;
  task_queue_->PostTask(
      [this, callback, histories = ExtractRecentHistories()]() mutable {
        RTC_DCHECK_RUN_ON(task_queue_.get());
        if (event_output_) {
          RTC_DCHECK(event_output_->IsActive());
          LogEventsToOutput(std::move(histories));
        }
        StopLoggingInternal();
        callback();
      });
}

RtcEventLogImpl::EventHistories RtcEventLogImpl::ExtractRecentHistories() {
  EventHistories histories;
  std::swap(histories, recent_);
  return histories;
}

void RtcEventLogImpl::Log(std::unique_ptr<RtcEvent> event) {
  RTC_CHECK(event);
  MutexLock lock(&mutex_);

  LogToMemory(std::move(event));
  if (logging_state_started_) {
    if (ShouldOutputImmediately()) {

      task_queue_->PostTask(
          [this, histories = ExtractRecentHistories()]() mutable {
            RTC_DCHECK_RUN_ON(task_queue_.get());
            if (event_output_) {
              RTC_DCHECK(event_output_->IsActive());
              LogEventsToOutput(std::move(histories));
            }
          });
    } else if (need_schedule_output_) {
      need_schedule_output_ = false;

      task_queue_->PostTask([this]() mutable {
        RTC_DCHECK_RUN_ON(task_queue_.get());
        if (event_output_) {
          RTC_DCHECK(event_output_->IsActive());
          ScheduleOutput();
        }
      });
    }
  }
}

bool RtcEventLogImpl::ShouldOutputImmediately() {
  if (recent_.history.size() >= max_events_in_history_) {

    return true;
  }

  return immediately_output_mode_;
}

void RtcEventLogImpl::ScheduleOutput() {
  RTC_DCHECK(output_period_ms_ != kImmediateOutput);

  auto output_task = [this]() {
    RTC_DCHECK_RUN_ON(task_queue_.get());

    if (event_output_) {
      RTC_DCHECK(event_output_->IsActive());
      mutex_.Lock();
      RTC_DCHECK(!need_schedule_output_);

      need_schedule_output_ = true;
      EventHistories histories = ExtractRecentHistories();
      mutex_.Unlock();
      LogEventsToOutput(std::move(histories));
    }
  };
  const int64_t now_ms = rtc::TimeMillis();
  const int64_t time_since_output_ms = now_ms - last_output_ms_;
  const int32_t delay = rtc::SafeClamp(output_period_ms_ - time_since_output_ms,
                                       0, output_period_ms_);
  task_queue_->PostDelayedTask(std::move(output_task),
                               TimeDelta::Millis(delay));
}

void RtcEventLogImpl::LogToMemory(std::unique_ptr<RtcEvent> event) {
  EventDeque& container =
      event->IsConfigEvent() ? recent_.config_history : recent_.history;
  const size_t container_max_size = event->IsConfigEvent()
                                        ? max_config_events_in_history_
                                        : max_events_in_history_;

  if (container.size() >= container_max_size && !logging_state_started_) {
    container.pop_front();
  }
  container.push_back(std::move(event));
}

void RtcEventLogImpl::LogEventsToOutput(EventHistories histories) {
  last_output_ms_ = rtc::TimeMillis();

  std::string encoded_configs = event_encoder_->EncodeBatch(
      histories.config_history.begin(), histories.config_history.end());

  std::string encoded_history = event_encoder_->EncodeBatch(
      histories.history.begin(), histories.history.end());

  WriteConfigsAndHistoryToOutput(encoded_configs, encoded_history);

  all_config_history_.insert(
      all_config_history_.end(),
      std::make_move_iterator(histories.config_history.begin()),
      std::make_move_iterator(histories.config_history.end()));
  if (all_config_history_.size() > max_config_events_in_history_) {
    RTC_LOG(LS_WARNING) << "Dropping config events: "
                        << all_config_history_.size() << " exceeds maximum "
                        << max_config_events_in_history_;
    all_config_history_.erase(all_config_history_.begin(),
                              all_config_history_.begin() +
                                  all_config_history_.size() -
                                  max_config_events_in_history_);
  }
}

void RtcEventLogImpl::WriteConfigsAndHistoryToOutput(
    absl::string_view encoded_configs,
    absl::string_view encoded_history) {

  if (encoded_configs.empty()) {
    WriteToOutput(encoded_history);
  } else if (encoded_history.empty()) {
    WriteToOutput(encoded_configs);
  } else {
    std::string s;
    s.reserve(encoded_configs.size() + encoded_history.size());
    s.append(encoded_configs.data(), encoded_configs.size());
    s.append(encoded_history.data(), encoded_history.size());
    WriteToOutput(s);
  }
}

void RtcEventLogImpl::StopOutput() {
  event_output_.reset();
}

void RtcEventLogImpl::StopLoggingInternal() {
  if (event_output_) {
    RTC_DCHECK(event_output_->IsActive());
    const int64_t timestamp_us = rtc::TimeMillis() * 1000;
    event_output_->Write(event_encoder_->EncodeLogEnd(timestamp_us));
  }
  StopOutput();
}

void RtcEventLogImpl::WriteToOutput(absl::string_view output_string) {
  if (event_output_) {
    RTC_DCHECK(event_output_->IsActive());
    if (!event_output_->Write(output_string)) {
      RTC_LOG(LS_ERROR) << "Failed to write RTC event to output.";

      RTC_DCHECK(!event_output_->IsActive());
      StopOutput();
    }
  }
}

}
