/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_REAL_FFT_H_
#define COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_REAL_FFT_H_

#include <stdint.h>

enum { kMaxFFTOrder = 10 };

struct RealFFT;

#ifdef __cplusplus
extern "C" {
#endif

struct RealFFT* WebRtcSpl_CreateRealFFT(int order);
void WebRtcSpl_FreeRealFFT(struct RealFFT* self);

int WebRtcSpl_RealForwardFFT(struct RealFFT* self,
                             const int16_t* real_data_in,
                             int16_t* complex_data_out);

int WebRtcSpl_RealInverseFFT(struct RealFFT* self,
                             const int16_t* complex_data_in,
                             int16_t* real_data_out);

#ifdef __cplusplus
}
#endif

#endif
