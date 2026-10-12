/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_OPTIONS_H_
#define API_AUDIO_OPTIONS_H_

#include <stdint.h>

#include <string>

#include "absl/types/optional.h"
#include "rtc_base/system/rtc_export.h"

namespace cricket {

struct RTC_EXPORT AudioOptions {
  AudioOptions();
  ~AudioOptions();
  void SetAll(const AudioOptions& change);

  bool operator==(const AudioOptions& o) const;
  bool operator!=(const AudioOptions& o) const { return !(*this == o); }

  std::string ToString() const;

  absl::optional<bool> echo_cancellation;
#if defined(WEBRTC_IOS)

  absl::optional<bool> ios_force_software_aec_HACK;
#endif

  absl::optional<bool> auto_gain_control;

  absl::optional<bool> noise_suppression;

  absl::optional<bool> highpass_filter;

  absl::optional<bool> stereo_swapping;

  absl::optional<int> audio_jitter_buffer_max_packets;

  absl::optional<bool> audio_jitter_buffer_fast_accelerate;

  absl::optional<int> audio_jitter_buffer_min_delay_ms;

  absl::optional<bool> audio_network_adaptor;

  absl::optional<std::string> audio_network_adaptor_config;

  absl::optional<bool> init_recording_on_send;
};

}

#endif
