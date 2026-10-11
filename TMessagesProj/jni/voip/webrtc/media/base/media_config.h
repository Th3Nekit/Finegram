/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_MEDIA_CONFIG_H_
#define MEDIA_BASE_MEDIA_CONFIG_H_

namespace cricket {

struct MediaConfig {

  bool enable_dscp = true;

  struct Video {

    bool enable_cpu_adaptation = true;

    bool suspend_below_min_bitrate = false;

    bool enable_prerenderer_smoothing = true;

    bool periodic_alr_bandwidth_probing = false;

    bool experiment_cpu_load_estimator = false;

    int rtcp_report_interval_ms = 1000;

    bool enable_send_packet_batching = false;
  } video;

  struct Audio {

    int rtcp_report_interval_ms = 5000;
  } audio;

  bool operator==(const MediaConfig& o) const {
    return enable_dscp == o.enable_dscp &&
           video.enable_cpu_adaptation == o.video.enable_cpu_adaptation &&
           video.suspend_below_min_bitrate ==
               o.video.suspend_below_min_bitrate &&
           video.enable_prerenderer_smoothing ==
               o.video.enable_prerenderer_smoothing &&
           video.periodic_alr_bandwidth_probing ==
               o.video.periodic_alr_bandwidth_probing &&
           video.experiment_cpu_load_estimator ==
               o.video.experiment_cpu_load_estimator &&
           video.rtcp_report_interval_ms == o.video.rtcp_report_interval_ms &&
           video.enable_send_packet_batching ==
               o.video.enable_send_packet_batching &&
           audio.rtcp_report_interval_ms == o.audio.rtcp_report_interval_ms;
  }

  bool operator!=(const MediaConfig& o) const { return !(*this == o); }
};

}

#endif
