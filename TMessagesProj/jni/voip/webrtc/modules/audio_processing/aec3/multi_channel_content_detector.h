/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_MULTI_CHANNEL_CONTENT_DETECTOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_MULTI_CHANNEL_CONTENT_DETECTOR_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "absl/types/optional.h"

namespace webrtc {

class MultiChannelContentDetector {
 public:

  MultiChannelContentDetector(bool detect_stereo_content,
                              int num_render_input_channels,
                              float detection_threshold,
                              int stereo_detection_timeout_threshold_seconds,
                              float stereo_detection_hysteresis_seconds);

  bool UpdateDetection(
      const std::vector<std::vector<std::vector<float>>>& frame);

  bool IsProperMultiChannelContentDetected() const {
    return persistent_multichannel_content_detected_;
  }

  bool IsTemporaryMultiChannelContentDetected() const {
    return temporary_multichannel_content_detected_;
  }

 private:

  class MetricsLogger {
   public:
    MetricsLogger();

    ~MetricsLogger();

    void Update(bool persistent_multichannel_content_detected);

   private:
    int frame_counter_ = 0;

    int persistent_multichannel_frame_counter_ = 0;

    bool any_multichannel_content_detected_ = false;
  };

  const bool detect_stereo_content_;
  const float detection_threshold_;
  const absl::optional<int> detection_timeout_threshold_frames_;
  const int stereo_detection_hysteresis_frames_;

  const std::unique_ptr<MetricsLogger> metrics_logger_;

  bool persistent_multichannel_content_detected_;
  bool temporary_multichannel_content_detected_ = false;
  int64_t frames_since_stereo_detected_last_ = 0;
  int64_t consecutive_frames_with_stereo_ = 0;
};

}

#endif
