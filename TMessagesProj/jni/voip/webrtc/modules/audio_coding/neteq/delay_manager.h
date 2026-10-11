/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_DELAY_MANAGER_H_
#define MODULES_AUDIO_CODING_NETEQ_DELAY_MANAGER_H_

#include <string.h>

#include <deque>
#include <memory>

#include "absl/types/optional.h"
#include "api/neteq/tick_timer.h"
#include "modules/audio_coding/neteq/histogram.h"
#include "modules/audio_coding/neteq/reorder_optimizer.h"
#include "modules/audio_coding/neteq/underrun_optimizer.h"

namespace webrtc {

class DelayManager {
 public:
  struct Config {
    Config();
    void Log();

    double quantile = 0.95;
    double forget_factor = 0.983;
    absl::optional<double> start_forget_weight = 2;
    absl::optional<int> resample_interval_ms = 500;

    bool use_reorder_optimizer = true;
    double reorder_forget_factor = 0.9993;
    int ms_per_loss_percent = 20;

    int max_packets_in_buffer = 200;
    int base_minimum_delay_ms = 0;
  };

  DelayManager(const Config& config, const TickTimer* tick_timer);

  virtual ~DelayManager();

  DelayManager(const DelayManager&) = delete;
  DelayManager& operator=(const DelayManager&) = delete;

  virtual void Update(int arrival_delay_ms, bool reordered);

  virtual void Reset();

  virtual int TargetDelayMs() const;

  virtual int UnlimitedTargetLevelMs() const;

  virtual int SetPacketAudioLength(int length_ms);

  virtual bool SetMinimumDelay(int delay_ms);
  virtual bool SetMaximumDelay(int delay_ms);
  virtual bool SetBaseMinimumDelay(int delay_ms);
  virtual int GetBaseMinimumDelay() const;

  int effective_minimum_delay_ms_for_test() const {
    return effective_minimum_delay_ms_;
  }

 private:

  int MinimumDelayUpperBound() const;

  void UpdateEffectiveMinimumDelay();

  bool IsValidMinimumDelay(int delay_ms) const;

  bool IsValidBaseMinimumDelay(int delay_ms) const;

  const int max_packets_in_buffer_;
  UnderrunOptimizer underrun_optimizer_;
  std::unique_ptr<ReorderOptimizer> reorder_optimizer_;

  int base_minimum_delay_ms_;
  int effective_minimum_delay_ms_;
  int minimum_delay_ms_;
  int maximum_delay_ms_;

  int packet_len_ms_ = 0;
  int target_level_ms_ = 0;
  int unlimited_target_level_ms_ = 0;
};

}
#endif
