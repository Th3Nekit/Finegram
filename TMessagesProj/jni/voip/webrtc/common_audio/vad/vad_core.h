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
 * This header file includes the descriptions of the core VAD calls.
 */

#ifndef COMMON_AUDIO_VAD_VAD_CORE_H_
#define COMMON_AUDIO_VAD_VAD_CORE_H_

#include "common_audio/signal_processing/include/signal_processing_library.h"

#if defined(__cplusplus)
#define CONSTEXPR_INT(x) constexpr int x
#else
#define CONSTEXPR_INT(x) enum { x }
#endif

CONSTEXPR_INT(kNumChannels = 6);
CONSTEXPR_INT(
    kNumGaussians = 2);
CONSTEXPR_INT(kTableSize = kNumChannels * kNumGaussians);
CONSTEXPR_INT(
    kMinEnergy = 10);

typedef struct VadInstT_ {
  int vad;
  int32_t downsampling_filter_states[4];
  WebRtcSpl_State48khzTo8khz state_48_to_8;
  int16_t noise_means[kTableSize];
  int16_t speech_means[kTableSize];
  int16_t noise_stds[kTableSize];
  int16_t speech_stds[kTableSize];

  int32_t frame_counter;
  int16_t over_hang;
  int16_t num_of_speech;

  int16_t index_vector[16 * kNumChannels];
  int16_t low_value_vector[16 * kNumChannels];

  int16_t mean_value[kNumChannels];
  int16_t upper_state[5];
  int16_t lower_state[5];
  int16_t hp_filter_state[4];
  int16_t over_hang_max_1[3];
  int16_t over_hang_max_2[3];
  int16_t individual[3];
  int16_t total[3];

  int init_flag;
} VadInstT;

int WebRtcVad_InitCore(VadInstT* self);

int WebRtcVad_set_mode_core(VadInstT* self, int mode);

int WebRtcVad_CalcVad48khz(VadInstT* inst,
                           const int16_t* speech_frame,
                           size_t frame_length);
int WebRtcVad_CalcVad32khz(VadInstT* inst,
                           const int16_t* speech_frame,
                           size_t frame_length);
int WebRtcVad_CalcVad16khz(VadInstT* inst,
                           const int16_t* speech_frame,
                           size_t frame_length);
int WebRtcVad_CalcVad8khz(VadInstT* inst,
                          const int16_t* speech_frame,
                          size_t frame_length);

#endif
