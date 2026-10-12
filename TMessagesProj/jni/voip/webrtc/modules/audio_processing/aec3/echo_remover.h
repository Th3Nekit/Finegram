/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_ECHO_REMOVER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_ECHO_REMOVER_H_

#include <vector>

#include "absl/types/optional.h"
#include "api/audio/echo_canceller3_config.h"
#include "api/audio/echo_control.h"
#include "modules/audio_processing/aec3/block.h"
#include "modules/audio_processing/aec3/delay_estimate.h"
#include "modules/audio_processing/aec3/echo_path_variability.h"
#include "modules/audio_processing/aec3/render_buffer.h"

namespace webrtc {

class EchoRemover {
 public:
  static EchoRemover* Create(const EchoCanceller3Config& config,
                             int sample_rate_hz,
                             size_t num_render_channels,
                             size_t num_capture_channels);
  virtual ~EchoRemover() = default;

  virtual void GetMetrics(EchoControl::Metrics* metrics) const = 0;

  virtual void ProcessCapture(
      EchoPathVariability echo_path_variability,
      bool capture_signal_saturation,
      const absl::optional<DelayEstimate>& external_delay,
      RenderBuffer* render_buffer,
      Block* linear_output,
      Block* capture) = 0;

  virtual void UpdateEchoLeakageStatus(bool leakage_detected) = 0;

  virtual void SetCaptureOutputUsage(bool capture_output_used) = 0;
};

}

#endif
