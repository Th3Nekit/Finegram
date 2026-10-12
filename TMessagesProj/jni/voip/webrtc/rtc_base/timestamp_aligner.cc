/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/timestamp_aligner.h"

#include <cstdlib>
#include <limits>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

namespace rtc {

TimestampAligner::TimestampAligner()
    : frames_seen_(0),
      offset_us_(0),
      clip_bias_us_(0),
      prev_translated_time_us_(std::numeric_limits<int64_t>::min()),
      prev_time_offset_us_(0) {}

TimestampAligner::~TimestampAligner() {}

int64_t TimestampAligner::TranslateTimestamp(int64_t capturer_time_us,
                                             int64_t system_time_us) {
  const int64_t translated_timestamp = ClipTimestamp(
      capturer_time_us + UpdateOffset(capturer_time_us, system_time_us),
      system_time_us);
  prev_time_offset_us_ = translated_timestamp - capturer_time_us;
  return translated_timestamp;
}

int64_t TimestampAligner::TranslateTimestamp(int64_t capturer_time_us) const {
  return capturer_time_us + prev_time_offset_us_;
}

int64_t TimestampAligner::UpdateOffset(int64_t capturer_time_us,
                                       int64_t system_time_us) {

  int64_t diff_us = system_time_us - capturer_time_us;

  int64_t error_us = diff_us - offset_us_;

  static const int64_t kResetThresholdUs = 300000;
  if (std::abs(error_us) > kResetThresholdUs) {
    RTC_LOG(LS_INFO) << "Resetting timestamp translation after averaging "
                     << frames_seen_ << " frames. Old offset: " << offset_us_
                     << ", new offset: " << diff_us;
    frames_seen_ = 0;
    clip_bias_us_ = 0;
  }

  static const int kWindowSize = 100;
  if (frames_seen_ < kWindowSize) {
    ++frames_seen_;
  }
  offset_us_ += error_us / frames_seen_;
  return offset_us_;
}

int64_t TimestampAligner::ClipTimestamp(int64_t filtered_time_us,
                                        int64_t system_time_us) {

  int64_t time_us = filtered_time_us - clip_bias_us_;
  if (time_us > system_time_us) {
    clip_bias_us_ += time_us - system_time_us;
    time_us = system_time_us;
  }

  else if (time_us < prev_translated_time_us_ + kMinFrameIntervalUs) {
    time_us = prev_translated_time_us_ + kMinFrameIntervalUs;
    if (time_us > system_time_us) {

      RTC_LOG(LS_WARNING) << "too short translated timestamp interval: "
                             "system time (us) = "
                          << system_time_us << ", interval (us) = "
                          << system_time_us - prev_translated_time_us_;
      time_us = system_time_us;
    }
  }
  RTC_DCHECK_GE(time_us, prev_translated_time_us_);
  RTC_DCHECK_LE(time_us, system_time_us);
  prev_translated_time_us_ = time_us;
  return time_us;
}

}
