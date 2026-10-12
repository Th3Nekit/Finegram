/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <memory.h>
#include <stdlib.h>

#include "modules/audio_coding/codecs/isac/main/source/pitch_estimator.h"
#include "modules/audio_coding/codecs/isac/main/source/os_specific_inline.h"
#include "rtc_base/compile_assert_c.h"

static const double kDampFilter[PITCH_DAMPORDER] = {-0.07, 0.25, 0.64, 0.25,
    -0.07};

static const double kIntrpCoef[PITCH_FRACS][PITCH_FRACORDER] = {
    {-0.02239172458614,  0.06653315052934, -0.16515880017569,  0.60701333734125,
     0.64671399919202, -0.20249000396417,  0.09926548334755, -0.04765933793109,
     0.01754159521746},
    {-0.01985640750434,  0.05816126837866, -0.13991265473714,  0.44560418147643,
     0.79117042386876, -0.20266133815188,  0.09585268418555, -0.04533310458084,
     0.01654127246314},
    {-0.01463300534216,  0.04229888475060, -0.09897034715253,  0.28284326017787,
     0.90385267956632, -0.16976950138649,  0.07704272393639, -0.03584218578311,
     0.01295781500709},
    {-0.00764851320885,  0.02184035544377, -0.04985561057281,  0.13083306574393,
     0.97545011664662, -0.10177807997561,  0.04400901776474, -0.02010737175166,
     0.00719783432422},
    {-0.00000000000000,  0.00000000000000, -0.00000000000001,  0.00000000000001,
     0.99999999999999,  0.00000000000001, -0.00000000000001,  0.00000000000000,
     -0.00000000000000},
    {0.00719783432422, -0.02010737175166,  0.04400901776474, -0.10177807997562,
     0.97545011664663,  0.13083306574393, -0.04985561057280,  0.02184035544377,
     -0.00764851320885},
    {0.01295781500710, -0.03584218578312,  0.07704272393640, -0.16976950138650,
     0.90385267956634,  0.28284326017785, -0.09897034715252,  0.04229888475059,
     -0.01463300534216},
    {0.01654127246315, -0.04533310458085,  0.09585268418557, -0.20266133815190,
     0.79117042386878,  0.44560418147640, -0.13991265473712,  0.05816126837865,
     -0.01985640750433}
};

typedef enum {
  kPitchFilterPre, kPitchFilterPost, kPitchFilterPreLa, kPitchFilterPreGain
} PitchFilterOperation;

typedef struct {
  double buffer[PITCH_INTBUFFSIZE + QLOOKAHEAD];
  double damper_state[PITCH_DAMPORDER];
  const double *interpol_coeff;
  double gain;
  double lag;
  int lag_offset;

  int sub_frame;
  PitchFilterOperation mode;
  int num_samples;
  int index;

  double damper_state_dg[4][PITCH_DAMPORDER];
  double gain_mult[4];
} PitchFilterParam;

static void FilterSegment(const double* in_data, PitchFilterParam* parameters,
                          double* out_data,
                          double out_dg[][PITCH_FRAME_LEN + QLOOKAHEAD]) {
  int n;
  int m;
  int j;
  double sum;
  double sum2;

  int pos = parameters->index + PITCH_BUFFSIZE;

  int pos_lag = pos - parameters->lag_offset;

  for (n = 0; n < parameters->num_samples; ++n) {

    for (m = PITCH_DAMPORDER - 1; m > 0; --m) {
      parameters->damper_state[m] = parameters->damper_state[m - 1];
    }

    sum = 0.0;
    for (m = 0; m < PITCH_FRACORDER; ++m) {
      sum += parameters->buffer[pos_lag + m] * parameters->interpol_coeff[m];
    }

    parameters->damper_state[0] = parameters->gain * sum;

    if (parameters->mode == kPitchFilterPreGain) {
      int lag_index = parameters->index - parameters->lag_offset;
      int m_tmp = (lag_index < 0) ? -lag_index : 0;

      for (m = PITCH_DAMPORDER - 1; m > 0; --m) {
        for (j = 0; j < 4; ++j) {
          parameters->damper_state_dg[j][m] =
              parameters->damper_state_dg[j][m - 1];
        }
      }

      for (j = 0; j < parameters->sub_frame + 1; ++j) {

        sum2 = 0.0;
        for (m = PITCH_FRACORDER-1; m >= m_tmp; --m) {

          sum2 += out_dg[j][lag_index + m] * parameters->interpol_coeff[m];
        }

        parameters->damper_state_dg[j][0] = parameters->gain_mult[j] * sum +
            parameters->gain * sum2;
      }

      for (j = 0; j < parameters->sub_frame + 1; ++j) {
        sum = 0.0;
        for (m = 0; m < PITCH_DAMPORDER; ++m) {
          sum -= parameters->damper_state_dg[j][m] * kDampFilter[m];
        }
        out_dg[j][parameters->index] = sum;
      }
    }

    sum = 0.0;
    for (m = 0; m < PITCH_DAMPORDER; ++m) {
      sum += parameters->damper_state[m] * kDampFilter[m];
    }

    out_data[parameters->index] = in_data[parameters->index] - sum;
    parameters->buffer[pos] = in_data[parameters->index] +
        out_data[parameters->index];

    ++parameters->index;
    ++pos;
    ++pos_lag;
  }
  return;
}

static void Update(PitchFilterParam* parameters) {
  double fraction;
  int fraction_index;

  parameters->lag_offset = WebRtcIsac_lrint(parameters->lag + PITCH_FILTDELAY +
                                            0.5);

  fraction = parameters->lag_offset - (parameters->lag + PITCH_FILTDELAY);
  fraction_index = WebRtcIsac_lrint(PITCH_FRACS * fraction - 0.5);
  parameters->interpol_coeff = kIntrpCoef[fraction_index];

  if (parameters->mode == kPitchFilterPreGain) {

    parameters->gain_mult[parameters->sub_frame] += 0.2;
    if (parameters->gain_mult[parameters->sub_frame] > 1.0) {
      parameters->gain_mult[parameters->sub_frame] = 1.0;
    }
    if (parameters->sub_frame > 0) {
      parameters->gain_mult[parameters->sub_frame - 1] -= 0.2;
    }
  }
}

static void FilterFrame(const double* in_data, PitchFiltstr* filter_state,
                        double* lags, double* gains, PitchFilterOperation mode,
                        double* out_data,
                        double out_dg[][PITCH_FRAME_LEN + QLOOKAHEAD]) {
  PitchFilterParam filter_parameters;
  double gain_delta, lag_delta;
  double old_lag, old_gain;
  int n;
  int m;
  const double kEnhancer = 1.3;

  filter_parameters.index = 0;
  filter_parameters.lag_offset = 0;
  filter_parameters.mode = mode;

  memcpy(filter_parameters.buffer, filter_state->ubuf,
         sizeof(filter_state->ubuf));
  RTC_COMPILE_ASSERT(sizeof(filter_parameters.buffer) >=
                 sizeof(filter_state->ubuf));
  memset(filter_parameters.buffer +
             sizeof(filter_state->ubuf) / sizeof(filter_state->ubuf[0]),
         0, sizeof(filter_parameters.buffer) - sizeof(filter_state->ubuf));
  memcpy(filter_parameters.damper_state, filter_state->ystate,
         sizeof(filter_state->ystate));

  if (mode == kPitchFilterPreGain) {

    memset(filter_parameters.gain_mult, 0, sizeof(filter_parameters.gain_mult));
    memset(filter_parameters.damper_state_dg, 0,
           sizeof(filter_parameters.damper_state_dg));
    for (n = 0; n < PITCH_SUBFRAMES; ++n) {

      memset(out_dg[n], 0, sizeof(out_dg[n]));
    }
  } else if (mode == kPitchFilterPost) {

    for (n = 0; n < PITCH_SUBFRAMES; ++n) {
      gains[n] *= -kEnhancer;
    }
  }

  old_lag = *filter_state->oldlagp;
  old_gain = *filter_state->oldgainp;

  if ((lags[0] > (PITCH_UPSTEP * old_lag)) ||
      (lags[0] < (PITCH_DOWNSTEP * old_lag))) {
    old_lag = lags[0];
    old_gain = gains[0];

    if (mode == kPitchFilterPreGain) {
      filter_parameters.gain_mult[0] = 1.0;
    }
  }

  filter_parameters.num_samples = PITCH_UPDATE;
  for (m = 0; m < PITCH_SUBFRAMES; ++m) {

    filter_parameters.sub_frame = m;

    lag_delta = (lags[m] - old_lag) / PITCH_GRAN_PER_SUBFRAME;
    filter_parameters.lag = old_lag;
    gain_delta = (gains[m] - old_gain) / PITCH_GRAN_PER_SUBFRAME;
    filter_parameters.gain = old_gain;

    old_lag = lags[m];
    old_gain = gains[m];

    for (n = 0; n < PITCH_GRAN_PER_SUBFRAME; ++n) {

      filter_parameters.gain += gain_delta;
      filter_parameters.lag += lag_delta;

      Update(&filter_parameters);

      FilterSegment(in_data, &filter_parameters, out_data, out_dg);
    }
  }

  if (mode != kPitchFilterPreGain) {

    memcpy(filter_state->ubuf, &filter_parameters.buffer[PITCH_FRAME_LEN],
           sizeof(filter_state->ubuf));
    memcpy(filter_state->ystate, filter_parameters.damper_state,
           sizeof(filter_state->ystate));

    *filter_state->oldlagp = old_lag;
    *filter_state->oldgainp = old_gain;
  }

  if ((mode == kPitchFilterPreGain) || (mode == kPitchFilterPreLa)) {

    filter_parameters.sub_frame = PITCH_SUBFRAMES - 1;
    filter_parameters.num_samples = QLOOKAHEAD;
    FilterSegment(in_data, &filter_parameters, out_data, out_dg);
  }
}

void WebRtcIsac_PitchfilterPre(double* in_data, double* out_data,
                               PitchFiltstr* pf_state, double* lags,
                               double* gains) {
  FilterFrame(in_data, pf_state, lags, gains, kPitchFilterPre, out_data, NULL);
}

void WebRtcIsac_PitchfilterPre_la(double* in_data, double* out_data,
                                  PitchFiltstr* pf_state, double* lags,
                                  double* gains) {
  FilterFrame(in_data, pf_state, lags, gains, kPitchFilterPreLa, out_data,
              NULL);
}

void WebRtcIsac_PitchfilterPre_gains(
    double* in_data, double* out_data,
    double out_dg[][PITCH_FRAME_LEN + QLOOKAHEAD], PitchFiltstr *pf_state,
    double* lags, double* gains) {
  FilterFrame(in_data, pf_state, lags, gains, kPitchFilterPreGain, out_data,
              out_dg);
}

void WebRtcIsac_PitchfilterPost(double* in_data, double* out_data,
                                PitchFiltstr* pf_state, double* lags,
                                double* gains) {
  FilterFrame(in_data, pf_state, lags, gains, kPitchFilterPost, out_data, NULL);
}
