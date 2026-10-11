/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ENCODER_OVERSHOOT_DETECTOR_H_
#define VIDEO_ENCODER_OVERSHOOT_DETECTOR_H_

#include <deque>

#include "absl/types/optional.h"
#include "api/units/data_rate.h"
#include "api/video_codecs/video_codec.h"

namespace webrtc {

class EncoderOvershootDetector {
 public:
  explicit EncoderOvershootDetector(int64_t window_size_ms,
                                    VideoCodecType codec,
                                    bool is_screenshare);
  ~EncoderOvershootDetector();

  void SetTargetRate(DataRate target_bitrate,
                     double target_framerate_fps,
                     int64_t time_ms);

  void OnEncodedFrame(size_t bytes, int64_t time_ms);

  absl::optional<double> GetNetworkRateUtilizationFactor(int64_t time_ms);

  absl::optional<double> GetMediaRateUtilizationFactor(int64_t time_ms);
  void Reset();

 private:
  int64_t IdealFrameSizeBits() const;
  void LeakBits(int64_t time_ms);
  void CullOldUpdates(int64_t time_ms);

  double HandleEncodedFrame(size_t frame_size_bits,
                            int64_t ideal_frame_size_bits,
                            int64_t time_ms,
                            int64_t* buffer_level_bits) const;

  const int64_t window_size_ms_;
  int64_t time_last_update_ms_;
  struct BitrateUpdate {
    BitrateUpdate(double network_utilization_factor,
                  double media_utilization_factor,
                  int64_t update_time_ms)
        : network_utilization_factor(network_utilization_factor),
          media_utilization_factor(media_utilization_factor),
          update_time_ms(update_time_ms) {}

    double network_utilization_factor;

    double media_utilization_factor;
    int64_t update_time_ms;
  };
  void UpdateHistograms();
  std::deque<BitrateUpdate> utilization_factors_;
  double sum_network_utilization_factors_;
  double sum_media_utilization_factors_;
  DataRate target_bitrate_;
  double target_framerate_fps_;
  int64_t network_buffer_level_bits_;
  int64_t media_buffer_level_bits_;
  VideoCodecType codec_;
  bool is_screenshare_;
  int64_t frame_count_;
  int64_t sum_diff_kbps_squared_;
  int64_t sum_overshoot_percent_;
};

}

#endif
