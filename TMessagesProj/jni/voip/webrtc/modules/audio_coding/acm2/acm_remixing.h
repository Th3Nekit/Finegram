/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_ACM2_ACM_REMIXING_H_
#define MODULES_AUDIO_CODING_ACM2_ACM_REMIXING_H_

#include <vector>

#include "api/audio/audio_frame.h"

namespace webrtc {

void DownMixFrame(const AudioFrame& input, rtc::ArrayView<int16_t> output);

void ReMixFrame(const AudioFrame& input,
                size_t num_output_channels,
                std::vector<int16_t>* output);

}

#endif
