/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_TRANSPARENT_MODE_H_
#define MODULES_AUDIO_PROCESSING_AEC3_TRANSPARENT_MODE_H_

#include <memory>

#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

class TransparentMode {
 public:
  static std::unique_ptr<TransparentMode> Create(
      const EchoCanceller3Config& config);

  virtual ~TransparentMode() {}

  virtual bool Active() const = 0;

  virtual void Reset() = 0;

  virtual void Update(int filter_delay_blocks,
                      bool any_filter_consistent,
                      bool any_filter_converged,
                      bool any_coarse_filter_converged,
                      bool all_filters_diverged,
                      bool active_render,
                      bool saturated_capture) = 0;
};

}
#endif
