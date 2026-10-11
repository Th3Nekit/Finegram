/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * filterbanks.c
 *
 * This file contains function WebRtcIsac_AllPassFilter2Float,
 * WebRtcIsac_SplitAndFilter, and WebRtcIsac_FilterAndCombine
 * which implement filterbanks that produce decimated lowpass and
 * highpass versions of a signal, and performs reconstruction.
 *
 */

#include "modules/audio_coding/codecs/isac/main/source/settings.h"
#include "modules/audio_coding/codecs/isac/main/source/codec.h"
#include "modules/audio_coding/codecs/isac/main/source/isac_vad.h"

static const float kHpStCoefOut1Float[4] =
{-1.99701049409000f, 0.99714204490000f, 0.01701049409000f, -0.01704204490000f};

static const float kHpStCoefOut2Float[4] =
{-1.98645294509837f, 0.98672435560000f, 0.00645294509837f, -0.00662435560000f};

void WebRtcIsac_FilterAndCombineFloat(float *InLP,
                                      float *InHP,
                                      float *Out,
                                      PostFiltBankstr *postfiltdata)
{
  int k;
  float tempin_ch1[FRAMESAMPLES+MAX_AR_MODEL_ORDER];
  float tempin_ch2[FRAMESAMPLES+MAX_AR_MODEL_ORDER];
  float ftmp, ftmp2;

  for (k=0;k<FRAMESAMPLES_HALF;k++) {
    tempin_ch1[k]=InLP[k]+InHP[k];
    tempin_ch2[k]=InLP[k]-InHP[k];
  }

  WebRtcIsac_AllPassFilter2Float(tempin_ch1, WebRtcIsac_kLowerApFactorsFloat,
                                 FRAMESAMPLES_HALF, NUMBEROFCHANNELAPSECTIONS,postfiltdata->STATE_0_UPPER_float);

  WebRtcIsac_AllPassFilter2Float(tempin_ch2, WebRtcIsac_kUpperApFactorsFloat,
                                 FRAMESAMPLES_HALF, NUMBEROFCHANNELAPSECTIONS,postfiltdata->STATE_0_LOWER_float);

  for (k=0;k<FRAMESAMPLES_HALF;k++) {
    Out[2*k]=tempin_ch2[k];
    Out[2*k+1]=tempin_ch1[k];
  }

  for (k=0;k<FRAMESAMPLES;k++) {
    ftmp2 = Out[k] + kHpStCoefOut1Float[2] * postfiltdata->HPstates1_float[0] +
        kHpStCoefOut1Float[3] * postfiltdata->HPstates1_float[1];
    ftmp = Out[k] - kHpStCoefOut1Float[0] * postfiltdata->HPstates1_float[0] -
        kHpStCoefOut1Float[1] * postfiltdata->HPstates1_float[1];
    postfiltdata->HPstates1_float[1] = postfiltdata->HPstates1_float[0];
    postfiltdata->HPstates1_float[0] = ftmp;
    Out[k] = ftmp2;
  }

  for (k=0;k<FRAMESAMPLES;k++) {
    ftmp2 = Out[k] + kHpStCoefOut2Float[2] * postfiltdata->HPstates2_float[0] +
        kHpStCoefOut2Float[3] * postfiltdata->HPstates2_float[1];
    ftmp = Out[k] - kHpStCoefOut2Float[0] * postfiltdata->HPstates2_float[0] -
        kHpStCoefOut2Float[1] * postfiltdata->HPstates2_float[1];
    postfiltdata->HPstates2_float[1] = postfiltdata->HPstates2_float[0];
    postfiltdata->HPstates2_float[0] = ftmp;
    Out[k] = ftmp2;
  }
}
