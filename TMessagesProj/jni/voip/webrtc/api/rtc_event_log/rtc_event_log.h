/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_RTC_EVENT_LOG_RTC_EVENT_LOG_H_
#define API_RTC_EVENT_LOG_RTC_EVENT_LOG_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

#include "api/rtc_event_log/rtc_event.h"
#include "api/rtc_event_log_output.h"
#include "api/task_queue/task_queue_factory.h"

namespace webrtc {

class RtcEventLog {
 public:
  enum : size_t { kUnlimitedOutput = 0 };
  enum : int64_t { kImmediateOutput = 0 };

  enum class EncodingType { Legacy, NewFormat, ProtoFree };

  virtual ~RtcEventLog() = default;

  virtual bool StartLogging(std::unique_ptr<RtcEventLogOutput> output,
                            int64_t output_period_ms) = 0;

  virtual void StopLogging() = 0;

  virtual void StopLogging(std::function<void()> callback) {
    StopLogging();
    callback();
  }

  virtual void Log(std::unique_ptr<RtcEvent> event) = 0;
};

class RtcEventLogNull final : public RtcEventLog {
 public:
  bool StartLogging(std::unique_ptr<RtcEventLogOutput> output,
                    int64_t output_period_ms) override;
  void StopLogging() override {}
  void Log(std::unique_ptr<RtcEvent> event) override {}
};

}

#endif
