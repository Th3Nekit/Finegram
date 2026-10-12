/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_STATISTICS_CALCULATOR_H_
#define MODULES_AUDIO_CODING_NETEQ_STATISTICS_CALCULATOR_H_

#include <deque>
#include <string>

#include "absl/strings/string_view.h"
#include "api/neteq/neteq.h"

namespace webrtc {

class DelayManager;

class StatisticsCalculator {
 public:
  StatisticsCalculator();

  virtual ~StatisticsCalculator();

  StatisticsCalculator(const StatisticsCalculator&) = delete;
  StatisticsCalculator& operator=(const StatisticsCalculator&) = delete;

  void Reset();

  void ResetMcu();

  void ExpandedVoiceSamples(size_t num_samples, bool is_new_concealment_event);

  void ExpandedNoiseSamples(size_t num_samples, bool is_new_concealment_event);

  void ExpandedVoiceSamplesCorrection(int num_samples);

  void ExpandedNoiseSamplesCorrection(int num_samples);

  void DecodedOutputPlayed();

  void EndExpandEvent(int fs_hz);

  void PreemptiveExpandedSamples(size_t num_samples);

  void AcceleratedSamples(size_t num_samples);

  void GeneratedNoiseSamples(size_t num_samples);

  virtual void PacketsDiscarded(size_t num_packets);

  virtual void SecondaryPacketsDiscarded(size_t num_packets);

  virtual void SecondaryPacketsReceived(size_t num_packets);

  void IncreaseCounter(size_t num_samples, int fs_hz);

  void JitterBufferDelay(size_t num_samples,
                         uint64_t waiting_time_ms,
                         uint64_t target_delay_ms,
                         uint64_t unlimited_target_delay_ms);

  void StoreWaitingTime(int waiting_time_ms);

  void SecondaryDecodedSamples(int num_samples);

  void FlushedPacketBuffer();

  void ReceivedPacket();

  virtual void RelativePacketArrivalDelay(size_t delay_ms);

  virtual void LogDelayedPacketOutageEvent(int num_samples, int fs_hz);

  void GetNetworkStatistics(size_t samples_per_packet,
                            NetEqNetworkStatistics* stats);

  NetEqLifetimeStatistics GetLifetimeStatistics() const;

  NetEqOperationsAndState GetOperationsAndState() const;

 private:
  static const int kMaxReportPeriod = 60;
  static const size_t kLenWaitingTimes = 100;

  class PeriodicUmaLogger {
   public:
    PeriodicUmaLogger(absl::string_view uma_name,
                      int report_interval_ms,
                      int max_value);
    virtual ~PeriodicUmaLogger();
    void AdvanceClock(int step_ms);

   protected:
    void LogToUma(int value) const;
    virtual int Metric() const = 0;
    virtual void Reset() = 0;

    const std::string uma_name_;
    const int report_interval_ms_;
    const int max_value_;
    int timer_ = 0;
  };

  class PeriodicUmaCount final : public PeriodicUmaLogger {
   public:
    PeriodicUmaCount(absl::string_view uma_name,
                     int report_interval_ms,
                     int max_value);
    ~PeriodicUmaCount() override;
    void RegisterSample();

   protected:
    int Metric() const override;
    void Reset() override;

   private:
    int counter_ = 0;
  };

  class PeriodicUmaAverage final : public PeriodicUmaLogger {
   public:
    PeriodicUmaAverage(absl::string_view uma_name,
                       int report_interval_ms,
                       int max_value);
    ~PeriodicUmaAverage() override;
    void RegisterSample(int value);

   protected:
    int Metric() const override;
    void Reset() override;

   private:
    double sum_ = 0.0;
    int counter_ = 0;
  };

  void ConcealedSamplesCorrection(int num_samples, bool is_voice);

  static uint16_t CalculateQ14Ratio(size_t numerator, uint32_t denominator);

  NetEqLifetimeStatistics lifetime_stats_;
  NetEqOperationsAndState operations_and_state_;
  size_t concealed_samples_correction_ = 0;
  size_t silent_concealed_samples_correction_ = 0;
  size_t preemptive_samples_;
  size_t accelerate_samples_;
  size_t expanded_speech_samples_;
  size_t expanded_noise_samples_;
  size_t concealed_samples_at_event_end_ = 0;
  uint32_t timestamps_since_last_report_;
  std::deque<int> waiting_times_;
  uint32_t secondary_decoded_samples_;
  size_t discarded_secondary_packets_;
  PeriodicUmaCount delayed_packet_outage_counter_;
  PeriodicUmaAverage excess_buffer_delay_;
  PeriodicUmaCount buffer_full_counter_;
  bool decoded_output_played_ = false;
};

}
#endif
