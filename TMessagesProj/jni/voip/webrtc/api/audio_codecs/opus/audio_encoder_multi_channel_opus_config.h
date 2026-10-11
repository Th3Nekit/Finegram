/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_MULTI_CHANNEL_OPUS_CONFIG_H_
#define API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_MULTI_CHANNEL_OPUS_CONFIG_H_

#include <stddef.h>

#include <vector>

#include "absl/types/optional.h"
#include "api/audio_codecs/opus/audio_encoder_opus_config.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct RTC_EXPORT AudioEncoderMultiChannelOpusConfig {
  static constexpr int kDefaultFrameSizeMs = 20;

  static constexpr int kMinBitrateBps = 6000;
  static constexpr int kMaxBitrateBps = 510000;

  AudioEncoderMultiChannelOpusConfig();
  AudioEncoderMultiChannelOpusConfig(const AudioEncoderMultiChannelOpusConfig&);
  ~AudioEncoderMultiChannelOpusConfig();
  AudioEncoderMultiChannelOpusConfig& operator=(
      const AudioEncoderMultiChannelOpusConfig&);

  int frame_size_ms;
  size_t num_channels;
  enum class ApplicationMode { kVoip, kAudio };
  ApplicationMode application;
  int bitrate_bps;
  bool fec_enabled;
  bool cbr_enabled;
  bool dtx_enabled;
  int max_playback_rate_hz;
  std::vector<int> supported_frame_lengths_ms;

  int complexity;

  int num_streams;

  int coupled_streams;

  std::vector<unsigned char> channel_mapping;

  bool IsOk() const;
};

}
#endif
