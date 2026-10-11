/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_TIMING_TIMING_H_
#define MODULES_VIDEO_CODING_TIMING_TIMING_H_

#include <memory>

#include "absl/types/optional.h"
#include "api/field_trials_view.h"
#include "api/units/time_delta.h"
#include "api/video/video_frame.h"
#include "api/video/video_timing.h"
#include "modules/video_coding/timing/decode_time_percentile_filter.h"
#include "modules/video_coding/timing/timestamp_extrapolator.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class VCMTiming {
 public:
  struct VideoDelayTimings {
    size_t num_decoded_frames;

    TimeDelta minimum_delay;

    TimeDelta estimated_max_decode_time;

    TimeDelta render_delay;

    TimeDelta min_playout_delay;

    TimeDelta max_playout_delay;

    TimeDelta target_delay;

    TimeDelta current_delay;
  };

  static constexpr TimeDelta kDefaultRenderDelay = TimeDelta::Millis(10);
  static constexpr int kDelayMaxChangeMsPerS = 100;

  VCMTiming(Clock* clock, const FieldTrialsView& field_trials);
  virtual ~VCMTiming() = default;

  void Reset();

  void set_render_delay(TimeDelta render_delay);

  void SetJitterDelay(TimeDelta required_delay);

  TimeDelta min_playout_delay() const;
  void set_min_playout_delay(TimeDelta min_playout_delay);

  void set_max_playout_delay(TimeDelta max_playout_delay);

  void UpdateCurrentDelay(uint32_t frame_timestamp);

  void UpdateCurrentDelay(Timestamp render_time, Timestamp actual_decode_time);

  void StopDecodeTimer(TimeDelta decode_time, Timestamp now);

  virtual void IncomingTimestamp(uint32_t rtp_timestamp,
                                 Timestamp last_packet_time);

  virtual Timestamp RenderTime(uint32_t frame_timestamp, Timestamp now) const;

  virtual TimeDelta MaxWaitingTime(Timestamp render_time,
                                   Timestamp now,
                                   bool too_many_frames_queued) const;

  TimeDelta TargetVideoDelay() const;

  VideoDelayTimings GetTimings() const;

  void SetTimingFrameInfo(const TimingFrameInfo& info);
  absl::optional<TimingFrameInfo> GetTimingFrameInfo();

  void SetMaxCompositionDelayInFrames(
      absl::optional<int> max_composition_delay_in_frames);

  VideoFrame::RenderParameters RenderParameters() const;

  void SetLastDecodeScheduledTimestamp(Timestamp last_decode_scheduled);

 private:
  TimeDelta EstimatedMaxDecodeTime() const RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  Timestamp RenderTimeInternal(uint32_t frame_timestamp, Timestamp now) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  TimeDelta TargetDelayInternal() const RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  TimeDelta StatsTargetDelayInternal() const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool UseLowLatencyRendering() const RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable Mutex mutex_;
  Clock* const clock_;
  const std::unique_ptr<TimestampExtrapolator> ts_extrapolator_
      RTC_PT_GUARDED_BY(mutex_);
  std::unique_ptr<DecodeTimePercentileFilter> decode_time_filter_
      RTC_GUARDED_BY(mutex_) RTC_PT_GUARDED_BY(mutex_);
  TimeDelta render_delay_ RTC_GUARDED_BY(mutex_);

  TimeDelta min_playout_delay_ RTC_GUARDED_BY(mutex_);
  TimeDelta max_playout_delay_ RTC_GUARDED_BY(mutex_);
  TimeDelta jitter_delay_ RTC_GUARDED_BY(mutex_);
  TimeDelta current_delay_ RTC_GUARDED_BY(mutex_);
  uint32_t prev_frame_timestamp_ RTC_GUARDED_BY(mutex_);
  absl::optional<TimingFrameInfo> timing_frame_info_ RTC_GUARDED_BY(mutex_);
  size_t num_decoded_frames_ RTC_GUARDED_BY(mutex_);
  absl::optional<int> max_composition_delay_in_frames_ RTC_GUARDED_BY(mutex_);

  FieldTrialParameter<TimeDelta> zero_playout_delay_min_pacing_
      RTC_GUARDED_BY(mutex_);

  Timestamp last_decode_scheduled_ RTC_GUARDED_BY(mutex_);
};
}

#endif
