/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ADAPTATION_OVERUSE_FRAME_DETECTOR_H_
#define VIDEO_ADAPTATION_OVERUSE_FRAME_DETECTOR_H_

#include <list>
#include <memory>

#include "absl/types/optional.h"
#include "api/field_trials_view.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/numerics/exp_filter.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/thread_annotations.h"
#include "video/video_stream_encoder_observer.h"

namespace webrtc {

class VideoFrame;

struct CpuOveruseOptions {

  int high_encode_usage_threshold_percent = 85;

  int low_encode_usage_threshold_percent =
      (high_encode_usage_threshold_percent - 1) / 2;

  int frame_timeout_interval_ms = 1500;

  int min_frame_samples = 120;

  int min_process_count = 3;

  int high_threshold_consecutive_count = 2;

  int filter_time_ms = 0;
};

class OveruseFrameDetectorObserverInterface {
 public:

  virtual void AdaptUp() = 0;

  virtual void AdaptDown() = 0;

 protected:
  virtual ~OveruseFrameDetectorObserverInterface() {}
};

class OveruseFrameDetector {
 public:
  explicit OveruseFrameDetector(CpuOveruseMetricsObserver* metrics_observer);
  virtual ~OveruseFrameDetector();

  OveruseFrameDetector(const OveruseFrameDetector&) = delete;
  OveruseFrameDetector& operator=(const OveruseFrameDetector&) = delete;

  void StartCheckForOveruse(
      TaskQueueBase* task_queue_base,
      const CpuOveruseOptions& options,
      OveruseFrameDetectorObserverInterface* overuse_observer);

  void StopCheckForOveruse();

  virtual void OnTargetFramerateUpdated(int framerate_fps);

  void FrameCaptured(const VideoFrame& frame, int64_t time_when_first_seen_us);

  void FrameSent(uint32_t timestamp,
                 int64_t time_sent_in_us,
                 int64_t capture_time_us,
                 absl::optional<int> encode_duration_us);

  class ProcessingUsage {
   public:
    virtual void Reset() = 0;
    virtual void SetMaxSampleDiffMs(float diff_ms) = 0;
    virtual void FrameCaptured(const VideoFrame& frame,
                               int64_t time_when_first_seen_us,
                               int64_t last_capture_time_us) = 0;

    virtual absl::optional<int> FrameSent(

        uint32_t timestamp,
        int64_t time_sent_in_us,

        int64_t capture_time_us,
        absl::optional<int> encode_duration_us) = 0;

    virtual int Value() = 0;
    virtual ~ProcessingUsage() = default;
  };

 protected:

  void CheckForOveruse(OveruseFrameDetectorObserverInterface* overuse_observer);
  void SetOptions(const CpuOveruseOptions& options);

  CpuOveruseOptions options_;

 private:
  void EncodedFrameTimeMeasured(int encode_duration_ms);
  bool IsOverusing(int encode_usage_percent);
  bool IsUnderusing(int encode_usage_percent, int64_t time_now);

  bool FrameTimeoutDetected(int64_t now) const;
  bool FrameSizeChanged(int num_pixels) const;

  void ResetAll(int num_pixels);

  static std::unique_ptr<ProcessingUsage> CreateProcessingUsage(
      const CpuOveruseOptions& options);

  RTC_NO_UNIQUE_ADDRESS SequenceChecker task_checker_;

  RepeatingTaskHandle check_overuse_task_ RTC_GUARDED_BY(task_checker_);

  CpuOveruseMetricsObserver* const metrics_observer_;
  absl::optional<int> encode_usage_percent_ RTC_GUARDED_BY(task_checker_);

  int64_t num_process_times_ RTC_GUARDED_BY(task_checker_);

  int64_t last_capture_time_us_ RTC_GUARDED_BY(task_checker_);

  int num_pixels_ RTC_GUARDED_BY(task_checker_);
  int max_framerate_ RTC_GUARDED_BY(task_checker_);
  int64_t last_overuse_time_ms_ RTC_GUARDED_BY(task_checker_);
  int checks_above_threshold_ RTC_GUARDED_BY(task_checker_);
  int num_overuse_detections_ RTC_GUARDED_BY(task_checker_);
  int64_t last_rampup_time_ms_ RTC_GUARDED_BY(task_checker_);
  bool in_quick_rampup_ RTC_GUARDED_BY(task_checker_);
  int current_rampup_delay_ms_ RTC_GUARDED_BY(task_checker_);

  std::unique_ptr<ProcessingUsage> usage_ RTC_PT_GUARDED_BY(task_checker_);

  FieldTrialOptional<TimeDelta> filter_time_constant_{"tau"};
};

}

#endif
