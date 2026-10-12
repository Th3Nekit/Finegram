/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC_LEGACY_GAIN_CONTROL_H_
#define MODULES_AUDIO_PROCESSING_AGC_LEGACY_GAIN_CONTROL_H_

#include <stddef.h>
#include <stdint.h>

namespace webrtc {

enum {
  kAgcModeUnchanged,
  kAgcModeAdaptiveAnalog,
  kAgcModeAdaptiveDigital,
  kAgcModeFixedDigital
};

enum { kAgcFalse = 0, kAgcTrue };

typedef struct {
  int16_t targetLevelDbfs;
  int16_t compressionGaindB;
  uint8_t limiterEnable;
} WebRtcAgcConfig;

int WebRtcAgc_GetAddFarendError(void* state, size_t samples);

int WebRtcAgc_AddFarend(void* agcInst, const int16_t* inFar, size_t samples);

int WebRtcAgc_AddMic(void* agcInst,
                     int16_t* const* inMic,
                     size_t num_bands,
                     size_t samples);

int WebRtcAgc_VirtualMic(void* agcInst,
                         int16_t* const* inMic,
                         size_t num_bands,
                         size_t samples,
                         int32_t micLevelIn,
                         int32_t* micLevelOut);

int WebRtcAgc_Analyze(void* agcInst,
                      const int16_t* const* inNear,
                      size_t num_bands,
                      size_t samples,
                      int32_t inMicLevel,
                      int32_t* outMicLevel,
                      int16_t echo,
                      uint8_t* saturationWarning,
                      int32_t gains[11]);

int WebRtcAgc_Process(const void* agcInst,
                      const int32_t gains[11],
                      const int16_t* const* in_near,
                      size_t num_bands,
                      int16_t* const* out);

int WebRtcAgc_set_config(void* agcInst, WebRtcAgcConfig config);

int WebRtcAgc_get_config(void* agcInst, WebRtcAgcConfig* config);

void* WebRtcAgc_Create(void);

void WebRtcAgc_Free(void* agcInst);

int WebRtcAgc_Init(void* agcInst,
                   int32_t minLevel,
                   int32_t maxLevel,
                   int16_t agcMode,
                   uint32_t fs);

}

#endif
