/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_NUMERICS_EVENT_RATE_COUNTER_H_
#define RTC_BASE_NUMERICS_EVENT_RATE_COUNTER_H_

#include "rtc_base/numerics/sample_stats.h"

namespace webrtc {

class EventRateCounter {
 public:

  void AddEvent(Timestamp event_time);

  void AddEvents(EventRateCounter other);
  bool IsEmpty() const;

  double Rate() const;
  SampleStats<TimeDelta>& interval() { return interval_; }
  TimeDelta TotalDuration() const;
  int Count() const { return event_count_; }

 private:
  Timestamp first_time_ = Timestamp::PlusInfinity();
  Timestamp last_time_ = Timestamp::MinusInfinity();
  int64_t event_count_ = 0;
  SampleStats<TimeDelta> interval_;
};
}
#endif
