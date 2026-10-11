/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Performs delay estimation on block by block basis.
// The return value is  0 - OK and -1 - Error, unless otherwise stated.

#ifndef MODULES_AUDIO_PROCESSING_UTILITY_DELAY_ESTIMATOR_WRAPPER_H_
#define MODULES_AUDIO_PROCESSING_UTILITY_DELAY_ESTIMATOR_WRAPPER_H_

#include <stdint.h>

namespace webrtc {

void WebRtc_FreeDelayEstimatorFarend(void* handle);

void* WebRtc_CreateDelayEstimatorFarend(int spectrum_size, int history_size);

int WebRtc_InitDelayEstimatorFarend(void* handle);

void WebRtc_SoftResetDelayEstimatorFarend(void* handle, int delay_shift);

int WebRtc_AddFarSpectrumFix(void* handle,
                             const uint16_t* far_spectrum,
                             int spectrum_size,
                             int far_q);

int WebRtc_AddFarSpectrumFloat(void* handle,
                               const float* far_spectrum,
                               int spectrum_size);

void WebRtc_FreeDelayEstimator(void* handle);

void* WebRtc_CreateDelayEstimator(void* farend_handle, int max_lookahead);

int WebRtc_InitDelayEstimator(void* handle);

int WebRtc_SoftResetDelayEstimator(void* handle, int delay_shift);

int WebRtc_set_history_size(void* handle, int history_size);

int WebRtc_history_size(const void* handle);

int WebRtc_set_lookahead(void* handle, int lookahead);

int WebRtc_lookahead(void* handle);

int WebRtc_set_allowed_offset(void* handle, int allowed_offset);

int WebRtc_get_allowed_offset(const void* handle);

int WebRtc_enable_robust_validation(void* handle, int enable);

int WebRtc_is_robust_validation_enabled(const void* handle);

int WebRtc_DelayEstimatorProcessFix(void* handle,
                                    const uint16_t* near_spectrum,
                                    int spectrum_size,
                                    int near_q);

int WebRtc_DelayEstimatorProcessFloat(void* handle,
                                      const float* near_spectrum,
                                      int spectrum_size);

int WebRtc_last_delay(void* handle);

float WebRtc_last_delay_quality(void* handle);

}

#endif
