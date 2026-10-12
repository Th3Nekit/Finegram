/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_MIXER_AUDIO_FRAME_MANIPULATOR_H_
#define MODULES_AUDIO_MIXER_AUDIO_FRAME_MANIPULATOR_H_

#include <stddef.h>
#include <stdint.h>

#include "api/audio/audio_frame.h"

namespace webrtc {

uint32_t AudioMixerCalculateEnergy(const AudioFrame& audio_frame);

void Ramp(float start_gain, float target_gain, AudioFrame* audio_frame);

void RemixFrame(size_t target_number_of_channels, AudioFrame* frame);

}

#endif
