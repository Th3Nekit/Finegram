/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_ECHO_CONTROL_H_
#define API_AUDIO_ECHO_CONTROL_H_

#include <memory>

#include "rtc_base/checks.h"

namespace webrtc {

class AudioBuffer;

class EchoControl {
 public:

  virtual void AnalyzeRender(AudioBuffer* render) = 0;

  virtual void AnalyzeCapture(AudioBuffer* capture) = 0;

  virtual void ProcessCapture(AudioBuffer* capture, bool level_change) = 0;

  virtual void ProcessCapture(AudioBuffer* capture,
                              AudioBuffer* linear_output,
                              bool level_change) = 0;

  struct Metrics {
    double echo_return_loss;
    double echo_return_loss_enhancement;
    int delay_ms;
  };

  virtual Metrics GetMetrics() const = 0;

  virtual void SetAudioBufferDelay(int delay_ms) = 0;

  virtual void SetCaptureOutputUsage(bool capture_output_used) {}

  virtual bool ActiveProcessing() const = 0;

  virtual ~EchoControl() {}
};

class EchoControlFactory {
 public:
  virtual std::unique_ptr<EchoControl> Create(int sample_rate_hz,
                                              int num_render_channels,
                                              int num_capture_channels) = 0;

  virtual ~EchoControlFactory() = default;
};
}

#endif
