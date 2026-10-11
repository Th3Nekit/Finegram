/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_NOISE_LEVEL_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AGC2_NOISE_LEVEL_ESTIMATOR_H_

#include <memory>

#include "modules/audio_processing/include/audio_frame_view.h"

namespace webrtc {
class ApmDataDumper;

class NoiseLevelEstimator {
 public:
  virtual ~NoiseLevelEstimator() = default;

  virtual float Analyze(const AudioFrameView<const float>& frame) = 0;
};

std::unique_ptr<NoiseLevelEstimator> CreateNoiseFloorEstimator(
    ApmDataDumper* data_dumper);

}

#endif
