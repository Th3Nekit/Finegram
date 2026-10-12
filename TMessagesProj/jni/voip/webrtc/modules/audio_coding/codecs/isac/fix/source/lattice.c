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
 * lattice.c
 *
 * Contains the normalized lattice filter routines (MA and AR) for iSAC codec
 *
 */

#include "modules/audio_coding/codecs/isac/fix/source/codec.h"
#include "modules/audio_coding/codecs/isac/fix/source/settings.h"
#include "rtc_base/sanitizer.h"

#define LATTICE_MUL_32_32_RSFT16(a32a, a32b, b32)                  \
  ((int32_t)(WEBRTC_SPL_MUL(a32a, b32) + (WEBRTC_SPL_MUL_16_32_RSFT16(a32b, b32))))

void WebRtcIsacfix_FilterArLoop(int16_t* ar_g_Q0,
                                int16_t* ar_f_Q0,
                                int16_t* cth_Q15,
                                int16_t* sth_Q15,
                                size_t order_coef);

void WebRtcIsacfix_FilterMaLoopC(int16_t input0,
                                 int16_t input1,
                                 int32_t input2,
                                 int32_t* ptr0,
                                 int32_t* ptr1,
                                 int32_t* ptr2) {
  int n = 0;

  int16_t t16a = (int16_t)(input2 >> 16);
  int16_t t16b = (int16_t)input2;
  if (t16b < 0) t16a++;

  for(n = 0; n < HALF_SUBFRAMELEN - 1; n++, ptr0++, ptr1++, ptr2++) {
    int32_t tmp32a = 0;
    int32_t tmp32b = 0;

    tmp32a = WEBRTC_SPL_MUL_16_32_RSFT15(input0, *ptr0);
    tmp32b = *ptr2 + tmp32a;
    *ptr2 = LATTICE_MUL_32_32_RSFT16(t16a, t16b, tmp32b);

    tmp32a = WEBRTC_SPL_MUL_16_32_RSFT15(input1, *ptr0);
    tmp32b = WEBRTC_SPL_MUL_16_32_RSFT15(input0, *ptr2);
    *ptr1 = tmp32a + tmp32b;
  }
}

void WebRtcIsacfix_NormLatticeFilterMa(size_t orderCoef,
                                       int32_t *stateGQ15,
                                       int16_t *lat_inQ0,
                                       int16_t *filt_coefQ15,
                                       int32_t *gain_lo_hiQ17,
                                       int16_t lo_hi,
                                       int16_t *lat_outQ9)
{
  int16_t sthQ15[MAX_AR_MODEL_ORDER];
  int16_t cthQ15[MAX_AR_MODEL_ORDER];

  int u, n;
  size_t i, k;
  int16_t temp2,temp3;
  size_t ord_1 = orderCoef+1;
  int32_t inv_cthQ16[MAX_AR_MODEL_ORDER];

  int32_t gain32, fQtmp;
  int16_t gain16;
  int16_t gain_sh;

  int32_t tmp32, tmp32b;
  int32_t fQ15vec[HALF_SUBFRAMELEN];
  int32_t gQ15[MAX_AR_MODEL_ORDER+1][HALF_SUBFRAMELEN];
  int16_t sh;
  int16_t t16a;
  int16_t t16b;

  for (u=0;u<SUBFRAMES;u++)
  {
    int32_t temp1 = u * HALF_SUBFRAMELEN;

    temp2 = (int16_t)(u * orderCoef);
    temp3 = (int16_t)(2 * u + lo_hi);

    memcpy(sthQ15, &filt_coefQ15[temp2], orderCoef * sizeof(int16_t));

    WebRtcSpl_SqrtOfOneMinusXSquared(sthQ15, orderCoef, cthQ15);

    gain32 = gain_lo_hiQ17[temp3];
    gain_sh = WebRtcSpl_NormW32(gain32);
    gain32 <<= gain_sh;

    for (k=0;k<orderCoef;k++)
    {
      gain32 = WEBRTC_SPL_MUL_16_32_RSFT15(cthQ15[k], gain32);
      inv_cthQ16[k] = WebRtcSpl_DivW32W16((int32_t)2147483647, cthQ15[k]);
    }
    gain16 = (int16_t)(gain32 >> 16);

    for (i=0;i<HALF_SUBFRAMELEN;i++)
    {
      fQ15vec[i] = lat_inQ0[i + temp1] << 15;
      gQ15[0][i] = lat_inQ0[i + temp1] << 15;
    }

    fQtmp = fQ15vec[0];

    for (i=1;i<ord_1;i++)
    {

      tmp32 = WEBRTC_SPL_MUL_16_32_RSFT15(sthQ15[i-1], stateGQ15[i-1]);
      tmp32b= fQtmp + tmp32;
      tmp32 = inv_cthQ16[i-1];
      t16a = (int16_t)(tmp32 >> 16);
      t16b = (int16_t)(tmp32 - (t16a << 16));
      if (t16b<0) t16a++;
      tmp32 = LATTICE_MUL_32_32_RSFT16(t16a, t16b, tmp32b);
      fQtmp = tmp32;

      tmp32  = WEBRTC_SPL_MUL_16_32_RSFT15(cthQ15[i-1], stateGQ15[i-1]);
      tmp32b = WEBRTC_SPL_MUL_16_32_RSFT15(sthQ15[i-1], fQtmp);
      tmp32  = tmp32 + tmp32b;
      gQ15[i][0] = tmp32;
    }

    for(k=0;k<orderCoef;k++)
    {

      WebRtcIsacfix_FilterMaLoopFix(sthQ15[k], cthQ15[k], inv_cthQ16[k],
                                    &gQ15[k][0], &gQ15[k+1][1], &fQ15vec[1]);
    }

    fQ15vec[0] = fQtmp;

    for(n=0;n<HALF_SUBFRAMELEN;n++)
    {

      tmp32 = WEBRTC_SPL_MUL_16_32_RSFT16(gain16, fQ15vec[n]);
      sh = 9-gain_sh;
      t16a = (int16_t) WEBRTC_SPL_SHIFT_W32(tmp32, sh);
      lat_outQ9[n + temp1] = t16a;
    }

    for (i=0;i<ord_1;i++)
    {
      stateGQ15[i] = gQ15[i][HALF_SUBFRAMELEN-1];
    }

  }

  return;
}

static inline int32_t RTC_NO_SANITIZE("shift")
    OverflowingLShiftS32(int32_t x, int shift) {
  return x << shift;
}

void WebRtcIsacfix_NormLatticeFilterAr(size_t orderCoef,
                                       int16_t *stateGQ0,
                                       int32_t *lat_inQ25,
                                       int16_t *filt_coefQ15,
                                       int32_t *gain_lo_hiQ17,
                                       int16_t lo_hi,
                                       int16_t *lat_outQ0)
{
  size_t ii, k, i;
  int n, u;
  int16_t sthQ15[MAX_AR_MODEL_ORDER];
  int16_t cthQ15[MAX_AR_MODEL_ORDER];
  int32_t tmp32;

  int16_t tmpAR;
  int16_t ARfQ0vec[HALF_SUBFRAMELEN];
  int16_t ARgQ0vec[MAX_AR_MODEL_ORDER+1];

  int32_t inv_gain32;
  int16_t inv_gain16;
  int16_t den16;
  int16_t sh;

  int16_t temp2,temp3;
  size_t ord_1 = orderCoef+1;

  for (u=0;u<SUBFRAMES;u++)
  {
    int32_t temp1 = u * HALF_SUBFRAMELEN;

    temp2 = (int16_t)(u * orderCoef);
    temp3 = (int16_t)(2 * u + lo_hi);

    for (ii=0; ii<orderCoef; ii++) {
      sthQ15[ii] = filt_coefQ15[temp2+ii];
    }

    WebRtcSpl_SqrtOfOneMinusXSquared(sthQ15, orderCoef, cthQ15);

    tmp32 = OverflowingLShiftS32(gain_lo_hiQ17[temp3], 10);

    for (k=0;k<orderCoef;k++) {
      tmp32 = WEBRTC_SPL_MUL_16_32_RSFT15(cthQ15[k], tmp32);
    }

    sh = WebRtcSpl_NormW32(tmp32);
    den16 = (int16_t) WEBRTC_SPL_SHIFT_W32(tmp32, sh-16);
    inv_gain32 = WebRtcSpl_DivW32W16((int32_t)2147483647, den16);

    inv_gain16 = (int16_t)(inv_gain32 >> 2);

    for (i=0;i<HALF_SUBFRAMELEN;i++)
    {
      tmp32 = OverflowingLShiftS32(lat_inQ25[i + temp1], 1);
      tmp32 = WEBRTC_SPL_MUL_16_32_RSFT16(inv_gain16, tmp32);
      tmp32 = WEBRTC_SPL_SHIFT_W32(tmp32, -(28-sh));

      ARfQ0vec[i] = (int16_t)WebRtcSpl_SatW32ToW16(tmp32);
    }

    for (i = orderCoef; i > 0; i--)
    {
      tmp32 = (cthQ15[i - 1] * ARfQ0vec[0] - sthQ15[i - 1] * stateGQ0[i - 1] +
               16384) >> 15;
      tmpAR = (int16_t)WebRtcSpl_SatW32ToW16(tmp32);

      tmp32 = (sthQ15[i - 1] * ARfQ0vec[0] + cthQ15[i - 1] * stateGQ0[i - 1] +
               16384) >> 15;
      ARgQ0vec[i] = (int16_t)WebRtcSpl_SatW32ToW16(tmp32);
      ARfQ0vec[0] = tmpAR;
    }
    ARgQ0vec[0] = ARfQ0vec[0];

    WebRtcIsacfix_FilterArLoop(ARgQ0vec, ARfQ0vec, cthQ15, sthQ15, orderCoef);

    for(n=0;n<HALF_SUBFRAMELEN;n++)
    {
      lat_outQ0[n + temp1] = ARfQ0vec[n];
    }

    for (i=0;i<ord_1;i++)
    {
      stateGQ0[i] = ARgQ0vec[i];
    }
  }

  return;
}
