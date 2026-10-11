/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_VIDEO_SOURCE_SINK_CONTROLLER_H_
#define VIDEO_VIDEO_SOURCE_SINK_CONTROLLER_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/sequence_checker.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "call/adaptation/video_source_restrictions.h"
#include "rtc_base/system/no_unique_address.h"

namespace webrtc {

class VideoSourceSinkController {
 public:
  VideoSourceSinkController(rtc::VideoSinkInterface<VideoFrame>* sink,
                            rtc::VideoSourceInterface<VideoFrame>* source);

  ~VideoSourceSinkController();

  void SetSource(rtc::VideoSourceInterface<VideoFrame>* source);
  bool HasSource() const;

  void RequestRefreshFrame();

  void PushSourceSinkSettings();

  VideoSourceRestrictions restrictions() const;
  absl::optional<size_t> pixels_per_frame_upper_limit() const;
  absl::optional<double> frame_rate_upper_limit() const;
  bool rotation_applied() const;
  int resolution_alignment() const;
  const std::vector<rtc::VideoSinkWants::FrameSize>& resolutions() const;
  bool active() const;
  absl::optional<rtc::VideoSinkWants::FrameSize> requested_resolution() const;

  void SetRestrictions(VideoSourceRestrictions restrictions);
  void SetPixelsPerFrameUpperLimit(
      absl::optional<size_t> pixels_per_frame_upper_limit);
  void SetFrameRateUpperLimit(absl::optional<double> frame_rate_upper_limit);
  void SetRotationApplied(bool rotation_applied);
  void SetResolutionAlignment(int resolution_alignment);
  void SetResolutions(std::vector<rtc::VideoSinkWants::FrameSize> resolutions);
  void SetActive(bool active);
  void SetRequestedResolution(
      absl::optional<rtc::VideoSinkWants::FrameSize> requested_resolution);

 private:
  rtc::VideoSinkWants CurrentSettingsToSinkWants() const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(sequence_checker_);

  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
 int a;
  rtc::VideoSinkInterface<VideoFrame>* const sink_;
  rtc::VideoSourceInterface<VideoFrame>* source_
      RTC_GUARDED_BY(&sequence_checker_);

  VideoSourceRestrictions restrictions_ RTC_GUARDED_BY(&sequence_checker_);

  absl::optional<size_t> pixels_per_frame_upper_limit_
      RTC_GUARDED_BY(&sequence_checker_);
  absl::optional<double> frame_rate_upper_limit_
      RTC_GUARDED_BY(&sequence_checker_);
  bool rotation_applied_ RTC_GUARDED_BY(&sequence_checker_) = false;
  int resolution_alignment_ RTC_GUARDED_BY(&sequence_checker_) = 1;
  std::vector<rtc::VideoSinkWants::FrameSize> resolutions_
      RTC_GUARDED_BY(&sequence_checker_);
  bool active_ RTC_GUARDED_BY(&sequence_checker_) = true;
  absl::optional<rtc::VideoSinkWants::FrameSize> requested_resolution_
      RTC_GUARDED_BY(&sequence_checker_);
};

}

#endif
