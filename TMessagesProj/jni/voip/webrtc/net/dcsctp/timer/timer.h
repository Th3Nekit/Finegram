/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_TIMER_TIMER_H_
#define NET_DCSCTP_TIMER_TIMER_H_

#include <stdint.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "net/dcsctp/public/timeout.h"
#include "rtc_base/strong_alias.h"

namespace dcsctp {

using TimerID = webrtc::StrongAlias<class TimerIDTag, uint32_t>;
using TimerGeneration = webrtc::StrongAlias<class TimerGenerationTag, uint32_t>;

enum class TimerBackoffAlgorithm {

  kFixed,

  kExponential,
};

struct TimerOptions {
  explicit TimerOptions(webrtc::TimeDelta duration)
      : TimerOptions(duration, TimerBackoffAlgorithm::kExponential) {}
  TimerOptions(webrtc::TimeDelta duration,
               TimerBackoffAlgorithm backoff_algorithm)
      : TimerOptions(duration, backoff_algorithm, absl::nullopt) {}
  TimerOptions(webrtc::TimeDelta duration,
               TimerBackoffAlgorithm backoff_algorithm,
               absl::optional<int> max_restarts)
      : TimerOptions(duration,
                     backoff_algorithm,
                     max_restarts,
                     webrtc::TimeDelta::PlusInfinity()) {}
  TimerOptions(webrtc::TimeDelta duration,
               TimerBackoffAlgorithm backoff_algorithm,
               absl::optional<int> max_restarts,
               webrtc::TimeDelta max_backoff_duration)
      : TimerOptions(duration,
                     backoff_algorithm,
                     max_restarts,
                     max_backoff_duration,
                     webrtc::TaskQueueBase::DelayPrecision::kLow) {}
  TimerOptions(webrtc::TimeDelta duration,
               TimerBackoffAlgorithm backoff_algorithm,
               absl::optional<int> max_restarts,
               webrtc::TimeDelta max_backoff_duration,
               webrtc::TaskQueueBase::DelayPrecision precision)
      : duration(duration),
        backoff_algorithm(backoff_algorithm),
        max_restarts(max_restarts),
        max_backoff_duration(max_backoff_duration),
        precision(precision) {}

  const webrtc::TimeDelta duration;

  const TimerBackoffAlgorithm backoff_algorithm;

  const absl::optional<int> max_restarts;

  const webrtc::TimeDelta max_backoff_duration;

  const webrtc::TaskQueueBase::DelayPrecision precision;
};

class Timer {
 public:

  static constexpr webrtc::TimeDelta kMaxTimerDuration =
      webrtc::TimeDelta::Seconds(24 * 3600);

  using OnExpired = std::function<webrtc::TimeDelta()>;

  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;

  ~Timer();

  void Start();

  void Stop();

  void set_duration(webrtc::TimeDelta duration) {
    duration_ = std::min(duration, kMaxTimerDuration);
  }

  webrtc::TimeDelta duration() const { return duration_; }

  int expiration_count() const { return expiration_count_; }

  const TimerOptions& options() const { return options_; }

  absl::string_view name() const { return name_; }

  bool is_running() const { return is_running_; }

 private:
  friend class TimerManager;
  using UnregisterHandler = std::function<void()>;
  Timer(TimerID id,
        absl::string_view name,
        OnExpired on_expired,
        UnregisterHandler unregister,
        std::unique_ptr<Timeout> timeout,
        const TimerOptions& options);

  void Trigger(TimerGeneration generation);

  const TimerID id_;
  const std::string name_;
  const TimerOptions options_;
  const OnExpired on_expired_;
  const UnregisterHandler unregister_handler_;
  const std::unique_ptr<Timeout> timeout_;

  webrtc::TimeDelta duration_;

  TimerGeneration generation_ = TimerGeneration(0);
  bool is_running_ = false;

  int expiration_count_ = 0;
};

class TimerManager {
 public:
  explicit TimerManager(
      std::function<std::unique_ptr<Timeout>(
          webrtc::TaskQueueBase::DelayPrecision)> create_timeout)
      : create_timeout_(std::move(create_timeout)) {}

  std::unique_ptr<Timer> CreateTimer(absl::string_view name,
                                     Timer::OnExpired on_expired,
                                     const TimerOptions& options);

  void HandleTimeout(TimeoutID timeout_id);

 private:
  const std::function<std::unique_ptr<Timeout>(
      webrtc::TaskQueueBase::DelayPrecision)>
      create_timeout_;
  std::map<TimerID, Timer*> timers_;
  TimerID next_id_ = TimerID(0);
};

}

#endif
