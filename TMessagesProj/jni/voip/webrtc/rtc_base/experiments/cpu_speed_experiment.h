/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_EXPERIMENTS_CPU_SPEED_EXPERIMENT_H_
#define RTC_BASE_EXPERIMENTS_CPU_SPEED_EXPERIMENT_H_

#include <vector>

#include "absl/types/optional.h"
#include "rtc_base/experiments/field_trial_parser.h"

namespace webrtc {

class CpuSpeedExperiment {
 public:
  CpuSpeedExperiment();
  ~CpuSpeedExperiment();

  struct Config {
    int pixels = 0;
    int cpu_speed = 0;

    int cpu_speed_le_cores = 0;

  };

  absl::optional<int> GetValue(int pixels, int num_cores) const;

 private:
  std::vector<Config> configs_;

  FieldTrialOptional<int> cores_;
};

}

#endif
