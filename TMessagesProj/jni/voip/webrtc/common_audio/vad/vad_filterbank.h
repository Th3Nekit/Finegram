/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file includes feature calculating functionality used in vad_core.c.
 */

#ifndef COMMON_AUDIO_VAD_VAD_FILTERBANK_H_
#define COMMON_AUDIO_VAD_VAD_FILTERBANK_H_

#include "common_audio/vad/vad_core.h"

int16_t WebRtcVad_CalculateFeatures(VadInstT* self,
                                    const int16_t* data_in,
                                    size_t data_length,
                                    int16_t* features);

#endif
