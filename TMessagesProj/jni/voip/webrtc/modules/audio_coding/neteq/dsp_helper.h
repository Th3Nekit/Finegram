/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_DSP_HELPER_H_
#define MODULES_AUDIO_CODING_NETEQ_DSP_HELPER_H_

#include <stdint.h>
#include <string.h>

#include "modules/audio_coding/neteq/audio_multi_vector.h"
#include "modules/audio_coding/neteq/audio_vector.h"

namespace webrtc {

class DspHelper {
 public:

  static const int16_t kDownsample8kHzTbl[3];
  static const int16_t kDownsample16kHzTbl[5];
  static const int16_t kDownsample32kHzTbl[7];
  static const int16_t kDownsample48kHzTbl[7];

  static const int kMuteFactorStart8kHz = 27307;
  static const int kMuteFactorIncrement8kHz = -5461;
  static const int kUnmuteFactorStart8kHz = 5461;
  static const int kUnmuteFactorIncrement8kHz = 5461;
  static const int kMuteFactorStart16kHz = 29789;
  static const int kMuteFactorIncrement16kHz = -2979;
  static const int kUnmuteFactorStart16kHz = 2979;
  static const int kUnmuteFactorIncrement16kHz = 2979;
  static const int kMuteFactorStart32kHz = 31208;
  static const int kMuteFactorIncrement32kHz = -1560;
  static const int kUnmuteFactorStart32kHz = 1560;
  static const int kUnmuteFactorIncrement32kHz = 1560;
  static const int kMuteFactorStart48kHz = 31711;
  static const int kMuteFactorIncrement48kHz = -1057;
  static const int kUnmuteFactorStart48kHz = 1057;
  static const int kUnmuteFactorIncrement48kHz = 1057;

  static int RampSignal(const int16_t* input,
                        size_t length,
                        int factor,
                        int increment,
                        int16_t* output);

  static int RampSignal(int16_t* signal,
                        size_t length,
                        int factor,
                        int increment);

  static int RampSignal(AudioVector* signal,
                        size_t start_index,
                        size_t length,
                        int factor,
                        int increment);

  static int RampSignal(AudioMultiVector* signal,
                        size_t start_index,
                        size_t length,
                        int factor,
                        int increment);

  static void PeakDetection(int16_t* data,
                            size_t data_length,
                            size_t num_peaks,
                            int fs_mult,
                            size_t* peak_index,
                            int16_t* peak_value);

  static void ParabolicFit(int16_t* signal_points,
                           int fs_mult,
                           size_t* peak_index,
                           int16_t* peak_value);

  static size_t MinDistortion(const int16_t* signal,
                              size_t min_lag,
                              size_t max_lag,
                              size_t length,
                              int32_t* distortion_value);

  static void CrossFade(const int16_t* input1,
                        const int16_t* input2,
                        size_t length,
                        int16_t* mix_factor,
                        int16_t factor_decrement,
                        int16_t* output);

  static void UnmuteSignal(const int16_t* input,
                           size_t length,
                           int16_t* factor,
                           int increment,
                           int16_t* output);

  static void MuteSignal(int16_t* signal, int mute_slope, size_t length);

  static int DownsampleTo4kHz(const int16_t* input,
                              size_t input_length,
                              size_t output_length,
                              int input_rate_hz,
                              bool compensate_delay,
                              int16_t* output);

  DspHelper(const DspHelper&) = delete;
  DspHelper& operator=(const DspHelper&) = delete;

 private:

  static const int16_t kParabolaCoefficients[17][3];
};

}
#endif
