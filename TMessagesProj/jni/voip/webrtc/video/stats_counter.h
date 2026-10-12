/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_STATS_COUNTER_H_
#define VIDEO_STATS_COUNTER_H_

#include <memory>
#include <string>

namespace webrtc {

class AggregatedCounter;
class Clock;
class Samples;

class StatsCounterObserver {
 public:
  virtual void OnMetricUpdated(int sample) = 0;

  virtual ~StatsCounterObserver() {}
};

struct AggregatedStats {
  std::string ToString() const;
  std::string ToStringWithMultiplier(int multiplier) const;

  int64_t num_samples = 0;
  int min = -1;
  int max = -1;
  int average = -1;

};

class StatsCounter {
 public:
  virtual ~StatsCounter();

  virtual bool GetMetric(int* metric) const = 0;

  virtual int GetValueForEmptyInterval() const = 0;

  AggregatedStats GetStats();

  AggregatedStats ProcessAndGetStats();

  void ProcessAndPause();

  void ProcessAndPauseForDuration(int64_t min_pause_time_ms);

  void ProcessAndStopPause();

  bool HasSample() const;

 protected:
  StatsCounter(Clock* clock,
               int64_t process_intervals_ms,
               bool include_empty_intervals,
               StatsCounterObserver* observer);

  void Add(int sample);
  void Set(int64_t sample, uint32_t stream_id);
  void SetLast(int64_t sample, uint32_t stream_id);

  const bool include_empty_intervals_;
  const int64_t process_intervals_ms_;
  const std::unique_ptr<AggregatedCounter> aggregated_counter_;
  const std::unique_ptr<Samples> samples_;

 private:
  bool TimeToProcess(int* num_elapsed_intervals);
  void TryProcess();
  void ReportMetricToAggregatedCounter(int value, int num_values_to_add) const;
  bool IncludeEmptyIntervals() const;
  void Resume();
  void ResumeIfMinTimePassed();

  Clock* const clock_;
  const std::unique_ptr<StatsCounterObserver> observer_;
  int64_t last_process_time_ms_;
  bool paused_;
  int64_t pause_time_ms_;
  int64_t min_pause_time_ms_;
};

class AvgCounter : public StatsCounter {
 public:
  AvgCounter(Clock* clock,
             StatsCounterObserver* observer,
             bool include_empty_intervals);
  ~AvgCounter() override {}

  AvgCounter(const AvgCounter&) = delete;
  AvgCounter& operator=(const AvgCounter&) = delete;

  void Add(int sample);

 private:
  bool GetMetric(int* metric) const override;

  int GetValueForEmptyInterval() const override;
};

class MaxCounter : public StatsCounter {
 public:
  MaxCounter(Clock* clock,
             StatsCounterObserver* observer,
             int64_t process_intervals_ms);
  ~MaxCounter() override {}

  MaxCounter(const MaxCounter&) = delete;
  MaxCounter& operator=(const MaxCounter&) = delete;

  void Add(int sample);

 private:
  bool GetMetric(int* metric) const override;
  int GetValueForEmptyInterval() const override;
};

class PercentCounter : public StatsCounter {
 public:
  PercentCounter(Clock* clock, StatsCounterObserver* observer);
  ~PercentCounter() override {}

  PercentCounter(const PercentCounter&) = delete;
  PercentCounter& operator=(const PercentCounter&) = delete;

  void Add(bool sample);

 private:
  bool GetMetric(int* metric) const override;
  int GetValueForEmptyInterval() const override;
};

class PermilleCounter : public StatsCounter {
 public:
  PermilleCounter(Clock* clock, StatsCounterObserver* observer);
  ~PermilleCounter() override {}

  PermilleCounter(const PermilleCounter&) = delete;
  PermilleCounter& operator=(const PermilleCounter&) = delete;

  void Add(bool sample);

 private:
  bool GetMetric(int* metric) const override;
  int GetValueForEmptyInterval() const override;
};

class RateCounter : public StatsCounter {
 public:
  RateCounter(Clock* clock,
              StatsCounterObserver* observer,
              bool include_empty_intervals);
  ~RateCounter() override {}

  RateCounter(const RateCounter&) = delete;
  RateCounter& operator=(const RateCounter&) = delete;

  void Add(int sample);

 private:
  bool GetMetric(int* metric) const override;
  int GetValueForEmptyInterval() const override;
};

class RateAccCounter : public StatsCounter {
 public:
  RateAccCounter(Clock* clock,
                 StatsCounterObserver* observer,
                 bool include_empty_intervals);
  ~RateAccCounter() override {}

  RateAccCounter(const RateAccCounter&) = delete;
  RateAccCounter& operator=(const RateAccCounter&) = delete;

  void Set(int64_t sample, uint32_t stream_id);

  void SetLast(int64_t sample, uint32_t stream_id);

 private:
  bool GetMetric(int* metric) const override;
  int GetValueForEmptyInterval() const override;
};

}

#endif
