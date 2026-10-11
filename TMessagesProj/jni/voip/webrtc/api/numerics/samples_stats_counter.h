/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_NUMERICS_SAMPLES_STATS_COUNTER_H_
#define API_NUMERICS_SAMPLES_STATS_COUNTER_H_

#include <map>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/running_statistics.h"

namespace webrtc {

class SamplesStatsCounter {
 public:
  struct StatsSample {
    double value;
    Timestamp time;

    std::map<std::string, std::string> metadata;
  };

  SamplesStatsCounter();
  explicit SamplesStatsCounter(size_t expected_samples_count);
  ~SamplesStatsCounter();
  SamplesStatsCounter(const SamplesStatsCounter&);
  SamplesStatsCounter& operator=(const SamplesStatsCounter&);
  SamplesStatsCounter(SamplesStatsCounter&&);
  SamplesStatsCounter& operator=(SamplesStatsCounter&&);

  void AddSample(double value);
  void AddSample(StatsSample sample);

  void AddSamples(const SamplesStatsCounter& other);

  bool IsEmpty() const { return samples_.empty(); }

  int64_t NumSamples() const { return stats_.Size(); }

  double GetMin() const {
    RTC_DCHECK(!IsEmpty());
    return *stats_.GetMin();
  }

  double GetMax() const {
    RTC_DCHECK(!IsEmpty());
    return *stats_.GetMax();
  }

  double GetSum() const {
    RTC_DCHECK(!IsEmpty());
    return *stats_.GetSum();
  }

  double GetAverage() const {
    RTC_DCHECK(!IsEmpty());
    return *stats_.GetMean();
  }

  double GetVariance() const {
    RTC_DCHECK(!IsEmpty());
    return *stats_.GetVariance();
  }

  double GetStandardDeviation() const {
    RTC_DCHECK(!IsEmpty());
    return *stats_.GetStandardDeviation();
  }

  double GetPercentile(double percentile);

  rtc::ArrayView<const StatsSample> GetTimedSamples() const { return samples_; }
  std::vector<double> GetSamples() const {
    std::vector<double> out;
    out.reserve(samples_.size());
    for (const auto& sample : samples_) {
      out.push_back(sample.value);
    }
    return out;
  }

 private:
  webrtc_impl::RunningStatistics<double> stats_;
  std::vector<StatsSample> samples_;
  bool sorted_ = false;
};

SamplesStatsCounter operator*(const SamplesStatsCounter& counter, double value);
inline SamplesStatsCounter operator*(double value,
                                     const SamplesStatsCounter& counter) {
  return counter * value;
}

SamplesStatsCounter operator/(const SamplesStatsCounter& counter, double value);

}

#endif
