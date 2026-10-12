/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_PROCESSING_STATISTICS_H_
#define MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_PROCESSING_STATISTICS_H_

#include <stdint.h>

#include "absl/types/optional.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct RTC_EXPORT AudioProcessingStats {
  AudioProcessingStats();
  AudioProcessingStats(const AudioProcessingStats& other);
  ~AudioProcessingStats();

  absl::optional<bool> voice_detected;

  absl::optional<double> echo_return_loss;

  absl::optional<double> echo_return_loss_enhancement;

  absl::optional<double> divergent_filter_fraction;

  absl::optional<int32_t> delay_median_ms;
  absl::optional<int32_t> delay_standard_deviation_ms;

  absl::optional<double> residual_echo_likelihood;

  absl::optional<double> residual_echo_likelihood_recent_max;

  absl::optional<int32_t> delay_ms;
};

}

#endif
