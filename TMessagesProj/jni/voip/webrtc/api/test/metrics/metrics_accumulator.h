/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_METRICS_METRICS_ACCUMULATOR_H_
#define API_TEST_METRICS_METRICS_ACCUMULATOR_H_

#include <map>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/test/metrics/metric.h"
#include "api/units/timestamp.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
namespace test {

class MetricsAccumulator {
 public:
  MetricsAccumulator() = default;

  bool AddSample(absl::string_view metric_name,
                 absl::string_view test_case_name,
                 double value,
                 Timestamp timestamp,
                 std::map<std::string, std::string> point_metadata = {});

  bool AddMetricMetadata(
      absl::string_view metric_name,
      absl::string_view test_case_name,
      Unit unit,
      ImprovementDirection improvement_direction,
      std::map<std::string, std::string> metric_metadata = {});

  std::vector<Metric> GetCollectedMetrics() const;

 private:
  struct MetricKey {
    MetricKey(absl::string_view metric_name, absl::string_view test_case_name)
        : metric_name(metric_name), test_case_name(test_case_name) {}

    std::string metric_name;
    std::string test_case_name;
  };
  friend bool operator<(const MetricKey& a, const MetricKey& b);

  struct MetricValue {
    SamplesStatsCounter counter;
    Metric metric;
  };

  MetricValue* GetOrCreateMetric(absl::string_view metric_name,
                                 absl::string_view test_case_name,
                                 bool* created)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable Mutex mutex_;
  std::map<MetricKey, MetricValue> metrics_ RTC_GUARDED_BY(mutex_);
};

}
}

#endif
