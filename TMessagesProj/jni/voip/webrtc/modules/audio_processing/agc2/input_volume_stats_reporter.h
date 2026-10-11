/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_INPUT_VOLUME_STATS_REPORTER_H_
#define MODULES_AUDIO_PROCESSING_AGC2_INPUT_VOLUME_STATS_REPORTER_H_

#include "absl/types/optional.h"
#include "rtc_base/gtest_prod_util.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

class InputVolumeStatsReporter {
 public:
  enum class InputVolumeType {
    kApplied = 0,
    kRecommended = 1,
  };

  explicit InputVolumeStatsReporter(InputVolumeType input_volume_type);
  InputVolumeStatsReporter(const InputVolumeStatsReporter&) = delete;
  InputVolumeStatsReporter operator=(const InputVolumeStatsReporter&) = delete;
  ~InputVolumeStatsReporter();

  void UpdateStatistics(int input_volume);

 private:
  FRIEND_TEST_ALL_PREFIXES(InputVolumeStatsReporterTest,
                           CheckVolumeUpdateStatsForEmptyStats);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeStatsReporterTest,
                           CheckVolumeUpdateStatsAfterNoVolumeChange);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeStatsReporterTest,
                           CheckVolumeUpdateStatsAfterVolumeIncrease);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeStatsReporterTest,
                           CheckVolumeUpdateStatsAfterVolumeDecrease);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeStatsReporterTest,
                           CheckVolumeUpdateStatsAfterReset);

  struct VolumeUpdateStats {
    int num_decreases = 0;
    int num_increases = 0;
    int sum_decreases = 0;
    int sum_increases = 0;
  } volume_update_stats_;

  VolumeUpdateStats volume_update_stats() const { return volume_update_stats_; }

  void LogVolumeUpdateStats() const;

  struct Histograms {
    metrics::Histogram* const on_volume_change;
    metrics::Histogram* const decrease_rate;
    metrics::Histogram* const decrease_average;
    metrics::Histogram* const increase_rate;
    metrics::Histogram* const increase_average;
    metrics::Histogram* const update_rate;
    metrics::Histogram* const update_average;
    bool AllPointersSet() const {
      return !!on_volume_change && !!decrease_rate && !!decrease_average &&
             !!increase_rate && !!increase_average && !!update_rate &&
             !!update_average;
    }
  } histograms_;

  const bool cannot_log_stats_;

  int log_volume_update_stats_counter_ = 0;
  absl::optional<int> previous_input_volume_ = absl::nullopt;
};

void UpdateHistogramOnRecommendedInputVolumeChangeToMatchTarget(int volume);

}

#endif
