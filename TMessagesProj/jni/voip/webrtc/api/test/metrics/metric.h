/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_METRICS_METRIC_H_
#define API_TEST_METRICS_METRIC_H_

#include <map>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/units/timestamp.h"

namespace webrtc {
namespace test {

enum class Unit {
  kMilliseconds,
  kPercent,
  kBytes,
  kKilobitsPerSecond,
  kHertz,

  kUnitless,
  kCount
};

absl::string_view ToString(Unit unit);

enum class ImprovementDirection {
  kBiggerIsBetter,
  kNeitherIsBetter,
  kSmallerIsBetter
};

absl::string_view ToString(ImprovementDirection direction);

struct Metric {
  struct TimeSeries {
    struct Sample {

      webrtc::Timestamp timestamp;
      double value;

      std::map<std::string, std::string> sample_metadata;
    };

    std::vector<Sample> samples;
  };

  struct Stats {

    absl::optional<double> mean;

    absl::optional<double> stddev;
    absl::optional<double> min;
    absl::optional<double> max;
  };

  std::string name;
  Unit unit;
  ImprovementDirection improvement_direction;

  std::string test_case;

  std::map<std::string, std::string> metric_metadata;

  TimeSeries time_series;
  Stats stats;
};

}
}

#endif
