/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Performs delay estimation on binary converted spectra.
// The return value is  0 - OK and -1 - Error, unless otherwise stated.

#ifndef MODULES_AUDIO_PROCESSING_UTILITY_DELAY_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_UTILITY_DELAY_ESTIMATOR_H_

#include <stdint.h>

namespace webrtc {

static const int32_t kMaxBitCountsQ9 = (32 << 9);

typedef struct {

  int* far_bit_counts;

  uint32_t* binary_far_history;
  int history_size;
} BinaryDelayEstimatorFarend;

typedef struct {

  int32_t* mean_bit_counts;

  int32_t* bit_counts;

  uint32_t* binary_near_history;
  int near_history_size;
  int history_size;

  int32_t minimum_probability;
  int last_delay_probability;

  int last_delay;

  int robust_validation_enabled;
  int allowed_offset;
  int last_candidate_delay;
  int compare_delay;
  int candidate_hits;
  float* histogram;
  float last_delay_histogram;

  int lookahead;

  BinaryDelayEstimatorFarend* farend;
} BinaryDelayEstimator;

void WebRtc_FreeBinaryDelayEstimatorFarend(BinaryDelayEstimatorFarend* self);

BinaryDelayEstimatorFarend* WebRtc_CreateBinaryDelayEstimatorFarend(
    int history_size);

int WebRtc_AllocateFarendBufferMemory(BinaryDelayEstimatorFarend* self,
                                      int history_size);

void WebRtc_InitBinaryDelayEstimatorFarend(BinaryDelayEstimatorFarend* self);

void WebRtc_SoftResetBinaryDelayEstimatorFarend(
    BinaryDelayEstimatorFarend* self,
    int delay_shift);

void WebRtc_AddBinaryFarSpectrum(BinaryDelayEstimatorFarend* self,
                                 uint32_t binary_far_spectrum);

void WebRtc_FreeBinaryDelayEstimator(BinaryDelayEstimator* self);

BinaryDelayEstimator* WebRtc_CreateBinaryDelayEstimator(
    BinaryDelayEstimatorFarend* farend,
    int max_lookahead);

int WebRtc_AllocateHistoryBufferMemory(BinaryDelayEstimator* self,
                                       int history_size);

void WebRtc_InitBinaryDelayEstimator(BinaryDelayEstimator* self);

int WebRtc_SoftResetBinaryDelayEstimator(BinaryDelayEstimator* self,
                                         int delay_shift);

int WebRtc_ProcessBinarySpectrum(BinaryDelayEstimator* self,
                                 uint32_t binary_near_spectrum);

int WebRtc_binary_last_delay(BinaryDelayEstimator* self);

float WebRtc_binary_last_delay_quality(BinaryDelayEstimator* self);

void WebRtc_MeanEstimatorFix(int32_t new_value,
                             int factor,
                             int32_t* mean_value);

}

#endif
