/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_RENDER_DELAY_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_RENDER_DELAY_BUFFER_H_

#include <stddef.h>

#include <vector>

#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/block.h"
#include "modules/audio_processing/aec3/downsampled_render_buffer.h"
#include "modules/audio_processing/aec3/render_buffer.h"

namespace webrtc {

class RenderDelayBuffer {
 public:
  enum class BufferingEvent {
    kNone,
    kRenderUnderrun,
    kRenderOverrun,
    kApiCallSkew
  };

  static RenderDelayBuffer* Create(const EchoCanceller3Config& config,
                                   int sample_rate_hz,
                                   size_t num_render_channels);
  virtual ~RenderDelayBuffer() = default;

  virtual void Reset() = 0;

  virtual BufferingEvent Insert(const Block& block) = 0;

  virtual BufferingEvent PrepareCaptureProcessing() = 0;

  virtual void HandleSkippedCaptureProcessing() = 0;

  virtual bool AlignFromDelay(size_t delay) = 0;

  virtual void AlignFromExternalDelay() = 0;

  virtual size_t Delay() const = 0;

  virtual size_t MaxDelay() const = 0;

  virtual RenderBuffer* GetRenderBuffer() = 0;

  virtual const DownsampledRenderBuffer& GetDownsampledRenderBuffer() const = 0;

  static int DelayEstimatorOffset(const EchoCanceller3Config& config);

  virtual void SetAudioBufferDelay(int delay_ms) = 0;

  virtual bool HasReceivedBufferDelay() = 0;
};

}

#endif
