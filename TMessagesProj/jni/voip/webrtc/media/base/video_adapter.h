/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_VIDEO_ADAPTER_H_
#define MEDIA_BASE_VIDEO_ADAPTER_H_

#include <stdint.h>

#include <string>
#include <utility>

#include "absl/types/optional.h"
#include "api/video/video_source_interface.h"
#include "common_video/framerate_controller.h"
#include "media/base/video_common.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

namespace cricket {

class RTC_EXPORT VideoAdapter {
 public:
  VideoAdapter();

  explicit VideoAdapter(int source_resolution_alignment);
  virtual ~VideoAdapter();

  VideoAdapter(const VideoAdapter&) = delete;
  VideoAdapter& operator=(const VideoAdapter&) = delete;

  bool AdaptFrameResolution(int in_width,
                            int in_height,
                            int64_t in_timestamp_ns,
                            int* cropped_width,
                            int* cropped_height,
                            int* out_width,
                            int* out_height) RTC_LOCKS_EXCLUDED(mutex_);

  void OnOutputFormatRequest(const absl::optional<VideoFormat>& format)
      RTC_LOCKS_EXCLUDED(mutex_);

  void OnOutputFormatRequest(
      const absl::optional<std::pair<int, int>>& target_aspect_ratio,
      const absl::optional<int>& max_pixel_count,
      const absl::optional<int>& max_fps) RTC_LOCKS_EXCLUDED(mutex_);

  void OnOutputFormatRequest(
      const absl::optional<std::pair<int, int>>& target_landscape_aspect_ratio,
      const absl::optional<int>& max_landscape_pixel_count,
      const absl::optional<std::pair<int, int>>& target_portrait_aspect_ratio,
      const absl::optional<int>& max_portrait_pixel_count,
      const absl::optional<int>& max_fps) RTC_LOCKS_EXCLUDED(mutex_);

  void OnSinkWants(const rtc::VideoSinkWants& sink_wants)
      RTC_LOCKS_EXCLUDED(mutex_);

  int GetTargetPixels() const;

  float GetMaxFramerate() const;

 private:

  bool DropFrame(int64_t in_timestamp_ns) RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  int frames_in_ RTC_GUARDED_BY(mutex_);
  int frames_out_ RTC_GUARDED_BY(mutex_);
  int frames_scaled_ RTC_GUARDED_BY(mutex_);
  int adaption_changes_
      RTC_GUARDED_BY(mutex_);
  int previous_width_ RTC_GUARDED_BY(mutex_);
  int previous_height_
      RTC_GUARDED_BY(mutex_);
  const bool variable_start_scale_factor_;

  const int source_resolution_alignment_;

  int resolution_alignment_ RTC_GUARDED_BY(mutex_);

  struct OutputFormatRequest {
    absl::optional<std::pair<int, int>> target_landscape_aspect_ratio;
    absl::optional<int> max_landscape_pixel_count;
    absl::optional<std::pair<int, int>> target_portrait_aspect_ratio;
    absl::optional<int> max_portrait_pixel_count;
    absl::optional<int> max_fps;

    std::string ToString() const;
  };

  OutputFormatRequest output_format_request_ RTC_GUARDED_BY(mutex_);
  int resolution_request_target_pixel_count_ RTC_GUARDED_BY(mutex_);
  int resolution_request_max_pixel_count_ RTC_GUARDED_BY(mutex_);
  int max_framerate_request_ RTC_GUARDED_BY(mutex_);

  absl::optional<OutputFormatRequest> stashed_output_format_request_
      RTC_GUARDED_BY(mutex_);

  webrtc::FramerateController framerate_controller_ RTC_GUARDED_BY(mutex_);

  mutable webrtc::Mutex mutex_;
};

}

#endif
