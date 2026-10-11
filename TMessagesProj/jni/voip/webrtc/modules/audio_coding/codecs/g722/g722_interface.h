/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_G722_G722_INTERFACE_H_
#define MODULES_AUDIO_CODING_CODECS_G722_G722_INTERFACE_H_

#include <stddef.h>
#include <stdint.h>

typedef struct WebRtcG722EncInst G722EncInst;
typedef struct WebRtcG722DecInst G722DecInst;

#define G722_WEBRTC_SPEECH 1
#define G722_WEBRTC_CNG 2

#ifdef __cplusplus
extern "C" {
#endif

int16_t WebRtcG722_CreateEncoder(G722EncInst** G722enc_inst);

int16_t WebRtcG722_EncoderInit(G722EncInst* G722enc_inst);

int WebRtcG722_FreeEncoder(G722EncInst* G722enc_inst);

size_t WebRtcG722_Encode(G722EncInst* G722enc_inst,
                         const int16_t* speechIn,
                         size_t len,
                         uint8_t* encoded);

int16_t WebRtcG722_CreateDecoder(G722DecInst** G722dec_inst);

void WebRtcG722_DecoderInit(G722DecInst* inst);

int WebRtcG722_FreeDecoder(G722DecInst* G722dec_inst);

size_t WebRtcG722_Decode(G722DecInst* G722dec_inst,
                         const uint8_t* encoded,
                         size_t len,
                         int16_t* decoded,
                         int16_t* speechType);

int16_t WebRtcG722_Version(char* versionStr, short len);

#ifdef __cplusplus
}
#endif

#endif
