/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_EXPERIMENTS_KEYFRAME_INTERVAL_SETTINGS_H_
#define RTC_BASE_EXPERIMENTS_KEYFRAME_INTERVAL_SETTINGS_H_

#include "absl/types/optional.h"
#include "api/field_trials_view.h"
#include "rtc_base/experiments/field_trial_parser.h"

namespace webrtc {

class KeyframeIntervalSettings final {
 public:
  static KeyframeIntervalSettings ParseFromFieldTrials();

  absl::optional<int> MinKeyframeSendIntervalMs() const;

 private:
  explicit KeyframeIntervalSettings(const FieldTrialsView* key_value_config);

  FieldTrialOptional<int> min_keyframe_send_interval_ms_;
};

}

#endif
