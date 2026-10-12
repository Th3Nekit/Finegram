/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_VIDEO_QUALITY_OBSERVER2_H_
#define VIDEO_VIDEO_QUALITY_OBSERVER2_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "absl/types/optional.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_content_type.h"
#include "rtc_base/numerics/moving_average.h"
#include "rtc_base/numerics/sample_counter.h"

namespace webrtc {
namespace internal {

struct VideoFrameMetaData;

class VideoQualityObserver {
 public:

  VideoQualityObserver();
  ~VideoQualityObserver() = default;

  void OnDecodedFrame(uint32_t rtp_frame_timestamp,
                      absl::optional<uint8_t> qp,
                      VideoCodecType codec);

  void OnRenderedFrame(const VideoFrameMetaData& frame_meta);

  void OnStreamInactive();

  uint32_t NumFreezes() const;
  uint32_t NumPauses() const;
  uint32_t TotalFreezesDurationMs() const;
  uint32_t TotalPausesDurationMs() const;
  uint32_t TotalFramesDurationMs() const;
  double SumSquaredFrameDurationsSec() const;

  void UpdateHistograms(bool screenshare);

  static const uint32_t kMinFrameSamplesToDetectFreeze;
  static const uint32_t kMinIncreaseForFreezeMs;
  static const uint32_t kAvgInterframeDelaysWindowSizeFrames;

 private:
  enum Resolution {
    Low = 0,
    Medium = 1,
    High = 2,
  };

  int64_t last_frame_rendered_ms_;
  int64_t num_frames_rendered_;
  int64_t first_frame_rendered_ms_;
  int64_t last_frame_pixels_;
  bool is_last_frame_blocky_;

  int64_t last_unfreeze_time_ms_;
  rtc::MovingAverage render_interframe_delays_;
  double sum_squared_interframe_delays_secs_;

  rtc::SampleCounter freezes_durations_;
  rtc::SampleCounter pauses_durations_;

  rtc::SampleCounter smooth_playback_durations_;

  std::vector<int64_t> time_in_resolution_ms_;

  Resolution current_resolution_;
  int num_resolution_downgrades_;

  int64_t time_in_blocky_video_ms_;
  bool is_paused_;

  std::set<int64_t> blocky_frames_;
};

}
}

#endif
