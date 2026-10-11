/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_REMIX_RESAMPLE_H_
#define AUDIO_REMIX_RESAMPLE_H_

#include "api/audio/audio_frame.h"
#include "common_audio/resampler/include/push_resampler.h"

namespace webrtc {
namespace voe {

void RemixAndResample(const AudioFrame& src_frame,
                      PushResampler<int16_t>* resampler,
                      AudioFrame* dst_frame);

void RemixAndResample(const int16_t* src_data,
                      size_t samples_per_channel,
                      size_t num_channels,
                      int sample_rate_hz,
                      PushResampler<int16_t>* resampler,
                      AudioFrame* dst_frame);

}
}

#endif
