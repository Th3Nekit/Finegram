/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_OPTIONALLY_BUILT_SUBMODULE_CREATORS_H_
#define MODULES_AUDIO_PROCESSING_OPTIONALLY_BUILT_SUBMODULE_CREATORS_H_

#include <memory>

#include "modules/audio_processing/transient/transient_suppressor.h"

namespace webrtc {

struct ApmSubmoduleCreationOverrides {
  bool transient_suppression = false;
};

std::unique_ptr<TransientSuppressor> CreateTransientSuppressor(
    const ApmSubmoduleCreationOverrides& overrides,
    TransientSuppressor::VadMode vad_mode,
    int sample_rate_hz,
    int detection_rate_hz,
    int num_channels);

}

#endif
