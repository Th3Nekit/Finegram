/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_CONFIG_SELECTOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_CONFIG_SELECTOR_H_

#include "absl/types/optional.h"
#include "api/audio/echo_canceller3_config.h"

namespace webrtc {

class ConfigSelector {
 public:
  ConfigSelector(
      const EchoCanceller3Config& config,
      const absl::optional<EchoCanceller3Config>& multichannel_config,
      int num_render_input_channels);

  void Update(bool multichannel_content);

  const EchoCanceller3Config& active_config() const { return *active_config_; }

 private:
  const EchoCanceller3Config config_;
  const absl::optional<EchoCanceller3Config> multichannel_config_;
  const EchoCanceller3Config* active_config_ = nullptr;
};

}

#endif
