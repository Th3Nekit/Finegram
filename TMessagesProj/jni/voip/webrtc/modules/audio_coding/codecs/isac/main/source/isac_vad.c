/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/isac/main/source/isac_vad.h"

#include <math.h>

void WebRtcIsac_InitPitchFilter(PitchFiltstr* pitchfiltdata) {
  int k;

  for (k = 0; k < PITCH_BUFFSIZE; k++) {
    pitchfiltdata->ubuf[k] = 0.0;
  }
  pitchfiltdata->ystate[0] = 0.0;
  for (k = 1; k < (PITCH_DAMPORDER); k++) {
    pitchfiltdata->ystate[k] = 0.0;
  }
  pitchfiltdata->oldlagp[0] = 50.0;
  pitchfiltdata->oldgainp[0] = 0.0;
}

static void WebRtcIsac_InitWeightingFilter(WeightFiltstr* wfdata) {
  int k;
  double t, dtmp, dtmp2, denum, denum2;

  for (k = 0; k < PITCH_WLPCBUFLEN; k++)
    wfdata->buffer[k] = 0.0;

  for (k = 0; k < PITCH_WLPCORDER; k++) {
    wfdata->istate[k] = 0.0;
    wfdata->weostate[k] = 0.0;
    wfdata->whostate[k] = 0.0;
  }

  t = 0.5;
  denum = 1.0 / ((double)PITCH_WLPCWINLEN);
  denum2 = denum * denum;
  for (k = 0; k < PITCH_WLPCWINLEN; k++) {
    dtmp = PITCH_WLPCASYM * t * denum + (1 - PITCH_WLPCASYM) * t * t * denum2;
    dtmp *= 3.14159265;
    dtmp2 = sin(dtmp);
    wfdata->window[k] = dtmp2 * dtmp2;
    t++;
  }
}

void WebRtcIsac_InitPitchAnalysis(PitchAnalysisStruct* State) {
  int k;

  for (k = 0; k < PITCH_CORR_LEN2 + PITCH_CORR_STEP2 + PITCH_MAX_LAG / 2 -
                      PITCH_FRAME_LEN / 2 + 2;
       k++)
    State->dec_buffer[k] = 0.0;
  for (k = 0; k < 2 * ALLPASSSECTIONS + 1; k++)
    State->decimator_state[k] = 0.0;
  for (k = 0; k < 2; k++)
    State->hp_state[k] = 0.0;
  for (k = 0; k < QLOOKAHEAD; k++)
    State->whitened_buf[k] = 0.0;
  for (k = 0; k < QLOOKAHEAD; k++)
    State->inbuf[k] = 0.0;

  WebRtcIsac_InitPitchFilter(&(State->PFstr_wght));

  WebRtcIsac_InitPitchFilter(&(State->PFstr));

  WebRtcIsac_InitWeightingFilter(&(State->Wghtstr));
}

void WebRtcIsac_InitPreFilterbank(PreFiltBankstr* prefiltdata) {
  int k;

  for (k = 0; k < QLOOKAHEAD; k++) {
    prefiltdata->INLABUF1[k] = 0;
    prefiltdata->INLABUF2[k] = 0;

    prefiltdata->INLABUF1_float[k] = 0;
    prefiltdata->INLABUF2_float[k] = 0;
  }
  for (k = 0; k < 2 * (QORDER - 1); k++) {
    prefiltdata->INSTAT1[k] = 0;
    prefiltdata->INSTAT2[k] = 0;
    prefiltdata->INSTATLA1[k] = 0;
    prefiltdata->INSTATLA2[k] = 0;

    prefiltdata->INSTAT1_float[k] = 0;
    prefiltdata->INSTAT2_float[k] = 0;
    prefiltdata->INSTATLA1_float[k] = 0;
    prefiltdata->INSTATLA2_float[k] = 0;
  }

  prefiltdata->HPstates[0] = 0.0;
  prefiltdata->HPstates[1] = 0.0;

  prefiltdata->HPstates_float[0] = 0.0f;
  prefiltdata->HPstates_float[1] = 0.0f;

  return;
}

double WebRtcIsac_LevDurb(double* a, double* k, double* r, size_t order) {
  const double LEVINSON_EPS = 1.0e-10;

  double sum, alpha;
  size_t m, m_h, i;
  alpha = 0;
  a[0] = 1.0;
  if (r[0] < LEVINSON_EPS) {
    for (i = 0; i < order; i++) {
      k[i] = 0;
      a[i + 1] = 0;
    }
  } else {
    a[1] = k[0] = -r[1] / r[0];
    alpha = r[0] + r[1] * k[0];
    for (m = 1; m < order; m++) {
      sum = r[m + 1];
      for (i = 0; i < m; i++) {
        sum += a[i + 1] * r[m - i];
      }
      k[m] = -sum / alpha;
      alpha += k[m] * sum;
      m_h = (m + 1) >> 1;
      for (i = 0; i < m_h; i++) {
        sum = a[i + 1] + k[m] * a[m - i];
        a[m - i] += k[m] * a[i + 1];
        a[i + 1] = sum;
      }
      a[m + 1] = k[m];
    }
  }
  return alpha;
}

const float WebRtcIsac_kUpperApFactorsFloat[2] = {0.03470000000000f,
                                                  0.38260000000000f};

const float WebRtcIsac_kLowerApFactorsFloat[2] = {0.15440000000000f,
                                                  0.74400000000000f};

void WebRtcIsac_AllPassFilter2Float(float* InOut,
                                    const float* APSectionFactors,
                                    int lengthInOut,
                                    int NumberOfSections,
                                    float* FilterState) {
  int n, j;
  float temp;
  for (j = 0; j < NumberOfSections; j++) {
    for (n = 0; n < lengthInOut; n++) {
      temp = FilterState[j] + APSectionFactors[j] * InOut[n];
      FilterState[j] = -APSectionFactors[j] * temp + InOut[n];
      InOut[n] = temp;
    }
  }
}

#define NUMBEROFCOMPOSITEAPSECTIONS 4

void WebRtcIsac_SplitAndFilterFloat(float* pin,
                                    float* LP,
                                    float* HP,
                                    double* LP_la,
                                    double* HP_la,
                                    PreFiltBankstr* prefiltdata) {
  int k, n;
  float CompositeAPFilterState[NUMBEROFCOMPOSITEAPSECTIONS];
  float ForTransform_CompositeAPFilterState[NUMBEROFCOMPOSITEAPSECTIONS];
  float ForTransform_CompositeAPFilterState2[NUMBEROFCOMPOSITEAPSECTIONS];
  float tempinoutvec[FRAMESAMPLES + MAX_AR_MODEL_ORDER];
  float tempin_ch1[FRAMESAMPLES + MAX_AR_MODEL_ORDER];
  float tempin_ch2[FRAMESAMPLES + MAX_AR_MODEL_ORDER];
  float in[FRAMESAMPLES];
  float ftmp;

  static const float kHpStCoefInFloat[4] = {
      -1.94895953203325f, 0.94984516000000f, -0.05101826139794f,
      0.05015484000000f};

  static const float WebRtcIsac_kCompositeApFactorsFloat[4] = {
      0.03470000000000f, 0.15440000000000f, 0.38260000000000f,
      0.74400000000000f};

  static const float WebRtcIsac_kTransform1Float[8] = {
      -0.00158678506084f, 0.00127157815343f, -0.00104805672709f,
      0.00084837248079f,  0.00134467983258f, -0.00107756549387f,
      0.00088814793277f,  -0.00071893072525f};

  static const float WebRtcIsac_kTransform2Float[8] = {
      -0.00170686041697f, 0.00136780109829f, -0.00112736532350f,
      0.00091257055385f,  0.00103094281812f, -0.00082615076557f,
      0.00068092756088f,  -0.00055119165484f};

  for (k = 0; k < FRAMESAMPLES; k++) {
    in[k] = pin[k] + kHpStCoefInFloat[2] * prefiltdata->HPstates_float[0] +
            kHpStCoefInFloat[3] * prefiltdata->HPstates_float[1];
    ftmp = pin[k] - kHpStCoefInFloat[0] * prefiltdata->HPstates_float[0] -
           kHpStCoefInFloat[1] * prefiltdata->HPstates_float[1];
    prefiltdata->HPstates_float[1] = prefiltdata->HPstates_float[0];
    prefiltdata->HPstates_float[0] = ftmp;
  }

  for (k = 0; k < NUMBEROFCOMPOSITEAPSECTIONS; k++) {
    CompositeAPFilterState[k] = 0.0;
  }

  for (k = 0; k < FRAMESAMPLES_HALF; k++) {
    tempinoutvec[k] = in[FRAMESAMPLES - 1 - 2 * k];
  }

  WebRtcIsac_AllPassFilter2Float(
      tempinoutvec, WebRtcIsac_kCompositeApFactorsFloat, FRAMESAMPLES_HALF,
      NUMBEROFCOMPOSITEAPSECTIONS, CompositeAPFilterState);

  for (k = 0; k < FRAMESAMPLES_HALF; k++) {
    tempin_ch1[FRAMESAMPLES_HALF + QLOOKAHEAD - 1 - k] = tempinoutvec[k];
  }

  for (k = 0; k < NUMBEROFCOMPOSITEAPSECTIONS; k++) {
    ForTransform_CompositeAPFilterState[k] = CompositeAPFilterState[k];
  }

  WebRtcIsac_AllPassFilter2Float(
      prefiltdata->INLABUF1_float, WebRtcIsac_kCompositeApFactorsFloat,
      QLOOKAHEAD, NUMBEROFCOMPOSITEAPSECTIONS, CompositeAPFilterState);

  for (k = 0; k < QLOOKAHEAD; k++) {
    tempin_ch1[QLOOKAHEAD - 1 - k] = prefiltdata->INLABUF1_float[k];
    prefiltdata->INLABUF1_float[k] = in[FRAMESAMPLES - 1 - 2 * k];
  }

  for (k = 0; k < NUMBEROFCOMPOSITEAPSECTIONS; k++) {
    CompositeAPFilterState[k] = 0.0;
  }

  for (k = 0; k < FRAMESAMPLES_HALF; k++) {
    tempinoutvec[k] = in[FRAMESAMPLES - 2 - 2 * k];
  }

  WebRtcIsac_AllPassFilter2Float(
      tempinoutvec, WebRtcIsac_kCompositeApFactorsFloat, FRAMESAMPLES_HALF,
      NUMBEROFCOMPOSITEAPSECTIONS, CompositeAPFilterState);

  for (k = 0; k < FRAMESAMPLES_HALF; k++) {
    tempin_ch2[FRAMESAMPLES_HALF + QLOOKAHEAD - 1 - k] = tempinoutvec[k];
  }

  for (k = 0; k < NUMBEROFCOMPOSITEAPSECTIONS; k++) {
    ForTransform_CompositeAPFilterState2[k] = CompositeAPFilterState[k];
  }

  WebRtcIsac_AllPassFilter2Float(
      prefiltdata->INLABUF2_float, WebRtcIsac_kCompositeApFactorsFloat,
      QLOOKAHEAD, NUMBEROFCOMPOSITEAPSECTIONS, CompositeAPFilterState);

  for (k = 0; k < QLOOKAHEAD; k++) {
    tempin_ch2[QLOOKAHEAD - 1 - k] = prefiltdata->INLABUF2_float[k];
    prefiltdata->INLABUF2_float[k] = in[FRAMESAMPLES - 2 - 2 * k];
  }

  for (k = 0; k < NUMBEROFCHANNELAPSECTIONS; k++) {
    for (n = 0; n < NUMBEROFCOMPOSITEAPSECTIONS;
         n++) {
      prefiltdata->INSTAT1_float[k] +=
          ForTransform_CompositeAPFilterState[n] *
          WebRtcIsac_kTransform1Float[k * NUMBEROFCHANNELAPSECTIONS + n];
      prefiltdata->INSTAT2_float[k] +=
          ForTransform_CompositeAPFilterState2[n] *
          WebRtcIsac_kTransform2Float[k * NUMBEROFCHANNELAPSECTIONS + n];
    }
  }

  WebRtcIsac_AllPassFilter2Float(tempin_ch1, WebRtcIsac_kUpperApFactorsFloat,
                                 FRAMESAMPLES_HALF, NUMBEROFCHANNELAPSECTIONS,
                                 prefiltdata->INSTAT1_float);
  WebRtcIsac_AllPassFilter2Float(tempin_ch2, WebRtcIsac_kLowerApFactorsFloat,
                                 FRAMESAMPLES_HALF, NUMBEROFCHANNELAPSECTIONS,
                                 prefiltdata->INSTAT2_float);

  for (k = 0; k < FRAMESAMPLES_HALF; k++) {
    LP[k] = 0.5f * (tempin_ch1[k] + tempin_ch2[k]);
    HP[k] = 0.5f * (tempin_ch1[k] - tempin_ch2[k]);
  }

  for (k = 0; k < FRAMESAMPLES_HALF; k++) {
    tempin_ch1[k] = in[2 * k + 1];
    tempin_ch2[k] = in[2 * k];
  }

  WebRtcIsac_AllPassFilter2Float(tempin_ch1, WebRtcIsac_kUpperApFactorsFloat,
                                 FRAMESAMPLES_HALF, NUMBEROFCHANNELAPSECTIONS,
                                 prefiltdata->INSTATLA1_float);
  WebRtcIsac_AllPassFilter2Float(tempin_ch2, WebRtcIsac_kLowerApFactorsFloat,
                                 FRAMESAMPLES_HALF, NUMBEROFCHANNELAPSECTIONS,
                                 prefiltdata->INSTATLA2_float);

  for (k = 0; k < FRAMESAMPLES_HALF; k++) {
    LP_la[k] = (float)(0.5f * (tempin_ch1[k] + tempin_ch2[k]));
    HP_la[k] = (double)(0.5f * (tempin_ch1[k] - tempin_ch2[k]));
  }
}
