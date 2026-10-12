/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_METRICS_METRICS_LOGGER_H_
#define API_TEST_METRICS_METRICS_LOGGER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/test/metrics/metric.h"
#include "api/test/metrics/metrics_accumulator.h"
#include "rtc_base/synchronization/mutex.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace test {

class MetricsLogger {
 public:
  virtual ~MetricsLogger() = default;

  virtual void LogSingleValueMetric(
      absl::string_view name,
      absl::string_view test_case_name,
      double value,
      Unit unit,
      ImprovementDirection improvement_direction,
      std::map<std::string, std::string> metadata = {}) = 0;

  virtual void LogMetric(absl::string_view name,
                         absl::string_view test_case_name,
                         const SamplesStatsCounter& values,
                         Unit unit,
                         ImprovementDirection improvement_direction,
                         std::map<std::string, std::string> metadata = {}) = 0;

  virtual void LogMetric(absl::string_view name,
                         absl::string_view test_case_name,
                         const Metric::Stats& metric_stats,
                         Unit unit,
                         ImprovementDirection improvement_direction,
                         std::map<std::string, std::string> metadata = {}) = 0;

  virtual std::vector<Metric> GetCollectedMetrics() const = 0;
};

class DefaultMetricsLogger : public MetricsLogger {
 public:
  explicit DefaultMetricsLogger(webrtc::Clock* clock) : clock_(clock) {}
  ~DefaultMetricsLogger() override = default;

  void LogSingleValueMetric(
      absl::string_view name,
      absl::string_view test_case_name,
      double value,
      Unit unit,
      ImprovementDirection improvement_direction,
      std::map<std::string, std::string> metadata = {}) override;

  void LogMetric(absl::string_view name,
                 absl::string_view test_case_name,
                 const SamplesStatsCounter& values,
                 Unit unit,
                 ImprovementDirection improvement_direction,
                 std::map<std::string, std::string> metadata = {}) override;

  void LogMetric(absl::string_view name,
                 absl::string_view test_case_name,
                 const Metric::Stats& metric_stats,
                 Unit unit,
                 ImprovementDirection improvement_direction,
                 std::map<std::string, std::string> metadata = {}) override;

  std::vector<Metric> GetCollectedMetrics() const override;

  MetricsAccumulator* GetMetricsAccumulator() { return &metrics_accumulator_; }

 private:
  webrtc::Timestamp Now();

  webrtc::Clock* const clock_;
  MetricsAccumulator metrics_accumulator_;

  mutable Mutex mutex_;
  std::vector<Metric> metrics_ RTC_GUARDED_BY(mutex_);
};

}
}

#endif
