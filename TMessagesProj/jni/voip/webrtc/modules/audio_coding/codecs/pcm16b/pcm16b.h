/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_PCM16B_PCM16B_H_
#define MODULES_AUDIO_CODING_CODECS_PCM16B_PCM16B_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t WebRtcPcm16b_Encode(const int16_t* speech, size_t len, uint8_t* encoded);

size_t WebRtcPcm16b_Decode(const uint8_t* encoded, size_t len, int16_t* speech);

#ifdef __cplusplus
}
#endif

#endif
