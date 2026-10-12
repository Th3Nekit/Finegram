/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_TIME_CONTROLLER_H_
#define API_TEST_TIME_CONTROLLER_H_

#include <functional>
#include <memory>
#include <string>

#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/synchronization/yield_policy.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class TimeController {
 public:
  virtual ~TimeController() = default;

  virtual Clock* GetClock() = 0;

  virtual TaskQueueFactory* GetTaskQueueFactory() = 0;

  std::unique_ptr<TaskQueueFactory> CreateTaskQueueFactory();

  virtual std::unique_ptr<rtc::Thread> CreateThread(
      const std::string& name,
      std::unique_ptr<rtc::SocketServer> socket_server = nullptr) = 0;

  virtual rtc::Thread* GetMainThread() = 0;

  virtual void AdvanceTime(TimeDelta duration) = 0;

  bool Wait(const std::function<bool()>& condition,
            TimeDelta max_duration = TimeDelta::Seconds(5));
};

class ControlledAlarmClock {
 public:
  virtual ~ControlledAlarmClock() = default;

  virtual Clock* GetClock() = 0;

  virtual bool ScheduleAlarmAt(Timestamp deadline) = 0;

  virtual void SetCallback(std::function<void()> callback) = 0;

  virtual void Sleep(TimeDelta duration) = 0;
};

}
#endif
