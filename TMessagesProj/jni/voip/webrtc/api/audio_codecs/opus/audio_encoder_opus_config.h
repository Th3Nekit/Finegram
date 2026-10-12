/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_OPUS_CONFIG_H_
#define API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_OPUS_CONFIG_H_

#include <stddef.h>

#include <vector>

#include "absl/types/optional.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct RTC_EXPORT AudioEncoderOpusConfig {
  static constexpr int kDefaultFrameSizeMs = 20;

  static constexpr int kMinBitrateBps = 6000;
  static constexpr int kMaxBitrateBps = 510000;

  AudioEncoderOpusConfig();
  AudioEncoderOpusConfig(const AudioEncoderOpusConfig&);
  ~AudioEncoderOpusConfig();
  AudioEncoderOpusConfig& operator=(const AudioEncoderOpusConfig&);

  bool IsOk() const;

  int frame_size_ms;
  int sample_rate_hz;
  size_t num_channels;
  enum class ApplicationMode { kVoip, kAudio };
  ApplicationMode application;

  absl::optional<int> bitrate_bps;

  bool fec_enabled;
  bool cbr_enabled;
  int max_playback_rate_hz;

  int complexity;
  int low_rate_complexity;
  int complexity_threshold_bps;
  int complexity_threshold_window_bps;

  bool dtx_enabled;
  std::vector<int> supported_frame_lengths_ms;
  int uplink_bandwidth_update_interval_ms;

  int payload_type;
};

}

#endif
