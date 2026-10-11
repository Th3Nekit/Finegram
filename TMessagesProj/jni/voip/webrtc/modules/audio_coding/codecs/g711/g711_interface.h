/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_G711_G711_INTERFACE_H_
#define MODULES_AUDIO_CODING_CODECS_G711_G711_INTERFACE_H_

#include <stddef.h>
#include <stdint.h>

#define G711_WEBRTC_SPEECH 1
#define G711_WEBRTC_CNG 2

#ifdef __cplusplus
extern "C" {
#endif

size_t WebRtcG711_EncodeA(const int16_t* speechIn,
                          size_t len,
                          uint8_t* encoded);

size_t WebRtcG711_EncodeU(const int16_t* speechIn,
                          size_t len,
                          uint8_t* encoded);

size_t WebRtcG711_DecodeA(const uint8_t* encoded,
                          size_t len,
                          int16_t* decoded,
                          int16_t* speechType);

size_t WebRtcG711_DecodeU(const uint8_t* encoded,
                          size_t len,
                          int16_t* decoded,
                          int16_t* speechType);

int16_t WebRtcG711_Version(char* version, int16_t lenBytes);

#ifdef __cplusplus
}
#endif

#endif
