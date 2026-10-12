/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/vad/pitch_internal.h"

#include <cmath>

namespace webrtc {

static void PitchInterpolation(double old_val, const double* in, double* out) {
  out[0] = 1. / 6. * old_val + 5. / 6. * in[0];
  out[1] = 5. / 6. * in[1] + 1. / 6. * in[2];
  out[2] = 0.5 * in[2] + 0.5 * in[3];
}

void GetSubframesPitchParameters(int sampling_rate_hz,
                                 double* gains,
                                 double* lags,
                                 int num_in_frames,
                                 int num_out_frames,
                                 double* log_old_gain,
                                 double* old_lag,
                                 double* log_pitch_gain,
                                 double* pitch_lag_hz) {

  for (int n = 0; n < num_in_frames; n++)
    gains[n] = log(gains[n] + 1e-12);

  PitchInterpolation(*log_old_gain, gains, log_pitch_gain);
  *log_old_gain = gains[num_in_frames - 1];
  PitchInterpolation(*old_lag, lags, pitch_lag_hz);
  *old_lag = lags[num_in_frames - 1];

  for (int n = 0; n < num_out_frames; n++) {
    pitch_lag_hz[n] = (sampling_rate_hz) / (pitch_lag_hz[n]);
  }
}

}
