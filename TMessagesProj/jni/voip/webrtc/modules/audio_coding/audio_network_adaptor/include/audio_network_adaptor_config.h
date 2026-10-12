/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_INCLUDE_AUDIO_NETWORK_ADAPTOR_CONFIG_H_
#define MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_INCLUDE_AUDIO_NETWORK_ADAPTOR_CONFIG_H_

#include <stddef.h>

#include "absl/types/optional.h"

namespace webrtc {

struct AudioEncoderRuntimeConfig {
  AudioEncoderRuntimeConfig();
  AudioEncoderRuntimeConfig(const AudioEncoderRuntimeConfig& other);
  ~AudioEncoderRuntimeConfig();
  AudioEncoderRuntimeConfig& operator=(const AudioEncoderRuntimeConfig& other);
  bool operator==(const AudioEncoderRuntimeConfig& other) const;
  absl::optional<int> bitrate_bps;
  absl::optional<int> frame_length_ms;

  absl::optional<float> uplink_packet_loss_fraction;
  absl::optional<bool> enable_fec;
  absl::optional<bool> enable_dtx;

  absl::optional<size_t> num_channels;

  bool last_fl_change_increase = false;
};

}

#endif
