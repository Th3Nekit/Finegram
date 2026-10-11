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
 * This header file includes the VAD API calls. Specific function calls are
 * given below.
 */

#ifndef COMMON_AUDIO_VAD_INCLUDE_WEBRTC_VAD_H_
#define COMMON_AUDIO_VAD_INCLUDE_WEBRTC_VAD_H_

#include <stddef.h>
#include <stdint.h>

typedef struct WebRtcVadInst VadInst;

#ifdef __cplusplus
extern "C" {
#endif

VadInst* WebRtcVad_Create(void);

void WebRtcVad_Free(VadInst* handle);

int WebRtcVad_Init(VadInst* handle);

int WebRtcVad_set_mode(VadInst* handle, int mode);

int WebRtcVad_Process(VadInst* handle,
                      int fs,
                      const int16_t* audio_frame,
                      size_t frame_length);

int WebRtcVad_ValidRateAndFrameLength(int rate, size_t frame_length);

#ifdef __cplusplus
}
#endif

#endif
