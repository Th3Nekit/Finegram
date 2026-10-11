/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file includes specific signal processing tools used in vad_core.c.

#ifndef COMMON_AUDIO_VAD_VAD_SP_H_
#define COMMON_AUDIO_VAD_VAD_SP_H_

#include "common_audio/vad/vad_core.h"

void WebRtcVad_Downsampling(const int16_t* signal_in,
                            int16_t* signal_out,
                            int32_t* filter_state,
                            size_t in_length);

int16_t WebRtcVad_FindMinimum(VadInstT* handle,
                              int16_t feature_value,
                              int channel);

#endif
