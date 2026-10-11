/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AECM_ECHO_CONTROL_MOBILE_H_
#define MODULES_AUDIO_PROCESSING_AECM_ECHO_CONTROL_MOBILE_H_

#include <stddef.h>
#include <stdint.h>

namespace webrtc {

enum { AecmFalse = 0, AecmTrue };

#define AECM_UNSPECIFIED_ERROR 12000
#define AECM_UNSUPPORTED_FUNCTION_ERROR 12001
#define AECM_UNINITIALIZED_ERROR 12002
#define AECM_NULL_POINTER_ERROR 12003
#define AECM_BAD_PARAMETER_ERROR 12004

#define AECM_BAD_PARAMETER_WARNING 12100

typedef struct {
  int16_t cngMode;
  int16_t echoMode;
} AecmConfig;

#ifdef __cplusplus
extern "C" {
#endif

void* WebRtcAecm_Create();

void WebRtcAecm_Free(void* aecmInst);

int32_t WebRtcAecm_Init(void* aecmInst, int32_t sampFreq);

int32_t WebRtcAecm_BufferFarend(void* aecmInst,
                                const int16_t* farend,
                                size_t nrOfSamples);

int32_t WebRtcAecm_GetBufferFarendError(void* aecmInst,
                                        const int16_t* farend,
                                        size_t nrOfSamples);

int32_t WebRtcAecm_Process(void* aecmInst,
                           const int16_t* nearendNoisy,
                           const int16_t* nearendClean,
                           int16_t* out,
                           size_t nrOfSamples,
                           int16_t msInSndCardBuf);

int32_t WebRtcAecm_set_config(void* aecmInst, AecmConfig config);

int32_t WebRtcAecm_InitEchoPath(void* aecmInst,
                                const void* echo_path,
                                size_t size_bytes);

int32_t WebRtcAecm_GetEchoPath(void* aecmInst,
                               void* echo_path,
                               size_t size_bytes);

size_t WebRtcAecm_echo_path_size_bytes();

#ifdef __cplusplus
}
#endif

}

#endif
