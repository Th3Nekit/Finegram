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
 * lpc_masking_model.c
 *
 * LPC analysis and filtering functions
 *
 */

#include "lpc_masking_model.h"

#include <limits.h>

#include "modules/audio_coding/codecs/isac/fix/source/codec.h"
#include "modules/audio_coding/codecs/isac/fix/source/entropy_coding.h"
#include "modules/audio_coding/codecs/isac/fix/source/settings.h"

void WebRtcSpl_AToK_JSK(
    int16_t *a16,
    int16_t useOrder,
    int16_t *k16
                        )
{
  int m, k;
  int32_t tmp32[MAX_AR_MODEL_ORDER];
  int32_t tmp32b;
  int32_t tmp_inv_denum32;
  int16_t tmp_inv_denum16;

  k16[useOrder-1] = a16[useOrder] << 4;

  for (m=useOrder-1; m>0; m--) {

    tmp_inv_denum32 = 1073741823 - k16[m] * k16[m];
    tmp_inv_denum16 = (int16_t)(tmp_inv_denum32 >> 15);

    for (k=1; k<=m; k++) {
      tmp32b = (a16[k] << 16) - ((k16[m] * a16[m - k + 1]) << 1);

      tmp32[k] = WebRtcSpl_DivW32W16(tmp32b, tmp_inv_denum16);
    }

    for (k=1; k<m; k++) {
      a16[k] = (int16_t)(tmp32[k] >> 1);
    }

    tmp32[m] = WEBRTC_SPL_SAT(4092, tmp32[m], -4092);
    k16[m - 1] = (int16_t)(tmp32[m] << 3);
  }

  return;
}

int16_t WebRtcSpl_LevinsonW32_JSK(
    int32_t *R,
    int16_t *A,
    int16_t *K,
    int16_t order
                                        ) {
  int16_t i, j;
  int16_t R_hi[LEVINSON_MAX_ORDER+1], R_low[LEVINSON_MAX_ORDER+1];

  int16_t A_hi[LEVINSON_MAX_ORDER+1], A_low[LEVINSON_MAX_ORDER+1];

  int16_t A_upd_hi[LEVINSON_MAX_ORDER+1], A_upd_low[LEVINSON_MAX_ORDER+1];

  int16_t K_hi, K_low;
  int16_t Alpha_hi, Alpha_low, Alpha_exp;

  int16_t tmp_hi, tmp_low;
  int32_t temp1W32, temp2W32, temp3W32;
  int16_t norm;

  norm = WebRtcSpl_NormW32(R[0]);

  for (i=order;i>=0;i--) {
    temp1W32 = R[i] << norm;

    R_hi[i] = (int16_t)(temp1W32 >> 16);
    R_low[i] = (int16_t)((temp1W32 - ((int32_t)R_hi[i] << 16)) >> 1);
  }

  temp2W32 = (R_hi[1] << 16) + (R_low[1] << 1);
  temp3W32  = WEBRTC_SPL_ABS_W32(temp2W32);
  temp1W32  = WebRtcSpl_DivW32HiLow(temp3W32, R_hi[0], R_low[0]);

  if (temp2W32 > 0) {
    temp1W32 = -temp1W32;
  }

  K_hi = (int16_t)(temp1W32 >> 16);
  K_low = (int16_t)((temp1W32 - ((int32_t)K_hi << 16)) >> 1);

  K[0] = K_hi;

  temp1W32 >>= 4;

  A_hi[1] = (int16_t)(temp1W32 >> 16);
  A_low[1] = (int16_t)((temp1W32 - ((int32_t)A_hi[1] << 16)) >> 1);

  temp1W32  = (((K_hi * K_low) >> 14) + K_hi * K_hi) << 1;

  temp1W32 = WEBRTC_SPL_ABS_W32(temp1W32);
  temp1W32 = (int32_t)0x7fffffffL - temp1W32;

  tmp_hi = (int16_t)(temp1W32 >> 16);
  tmp_low = (int16_t)((temp1W32 - ((int32_t)tmp_hi << 16)) >> 1);

  temp1W32 = (R_hi[0] * tmp_hi + ((R_hi[0] * tmp_low) >> 15) +
      ((R_low[0] * tmp_hi) >> 15)) << 1;

  Alpha_exp = WebRtcSpl_NormW32(temp1W32);
  temp1W32 <<= Alpha_exp;
  Alpha_hi = (int16_t)(temp1W32 >> 16);
  Alpha_low = (int16_t)((temp1W32 - ((int32_t)Alpha_hi<< 16)) >> 1);

  for (i=2; i<=order; i++)
  {

    temp1W32 = 0;

    for(j=1; j<i; j++) {

      temp1W32 += ((R_hi[j] * A_hi[i - j]) << 1) +
          ((((R_hi[j] * A_low[i - j]) >> 15) +
              ((R_low[j] * A_hi[i - j]) >> 15)) << 1);
    }

    temp1W32 <<= 4;
    temp1W32 += (R_hi[i] << 16) + (R_low[i] << 1);

    temp2W32 = WEBRTC_SPL_ABS_W32(temp1W32);
    temp3W32 = WebRtcSpl_DivW32HiLow(temp2W32, Alpha_hi, Alpha_low);

    if (temp1W32 > 0) {
      temp3W32 = -temp3W32;
    }

    norm = WebRtcSpl_NormW32(temp3W32);
    if ((Alpha_exp <= norm)||(temp3W32==0)) {
      temp3W32 <<= Alpha_exp;
    } else {
      if (temp3W32 > 0)
      {
        temp3W32 = (int32_t)0x7fffffffL;
      } else
      {
        temp3W32 = (int32_t)0x80000000L;
      }
    }

    K_hi = (int16_t)(temp3W32 >> 16);
    K_low = (int16_t)((temp3W32 - ((int32_t)K_hi << 16)) >> 1);

    K[i-1] = K_hi;

    if ((int32_t)WEBRTC_SPL_ABS_W16(K_hi) > (int32_t)32740) {
      return(-i);
    }

    for(j=1; j<i; j++)
    {
      temp1W32 = (A_hi[j] << 16) + (A_low[j] << 1);

      temp1W32 += (K_hi * A_hi[i - j] + ((K_hi * A_low[i - j]) >> 15) +
          ((K_low * A_hi[i - j]) >> 15)) << 1;

      A_upd_hi[j] = (int16_t)(temp1W32 >> 16);
      A_upd_low[j] = (int16_t)((temp1W32 - ((int32_t)A_upd_hi[j] << 16)) >> 1);
    }

    temp3W32 >>= 4;

    A_upd_hi[i] = (int16_t)(temp3W32 >> 16);
    A_upd_low[i] = (int16_t)((temp3W32 - ((int32_t)A_upd_hi[i] << 16)) >> 1);

    temp1W32 = (((K_hi * K_low) >> 14) + K_hi * K_hi) << 1;

    temp1W32 = WEBRTC_SPL_ABS_W32(temp1W32);
    temp1W32 = (int32_t)0x7fffffffL - temp1W32;

    tmp_hi = (int16_t)(temp1W32 >> 16);
    tmp_low = (int16_t)((temp1W32 - ((int32_t)tmp_hi << 16)) >> 1);

    temp1W32 = (Alpha_hi * tmp_hi + ((Alpha_hi * tmp_low) >> 15) +
        ((Alpha_low * tmp_hi) >> 15)) << 1;

    norm = WebRtcSpl_NormW32(temp1W32);
    temp1W32 <<= norm;

    Alpha_hi = (int16_t)(temp1W32 >> 16);
    Alpha_low = (int16_t)((temp1W32 - ((int32_t)Alpha_hi << 16)) >> 1);

    Alpha_exp = Alpha_exp + norm;

    for(j=1; j<=i; j++)
    {
      A_hi[j] =A_upd_hi[j];
      A_low[j] =A_upd_low[j];
    }
  }

  A[0] = 2048;

  for(i=1; i<=order; i++) {

    temp1W32 = (A_hi[i] << 16) + (A_low[i] << 1);

    A[i] = (int16_t)((temp1W32 + 32768) >> 16);
  }
  return(1);
}

static const int16_t kWindowAutocorr[WINLEN] = {
  0,     0,     0,     0,     0,     1,     1,     2,     2,     3,     5,     6,
  8,    10,    12,    14,    17,    20,    24,    28,    33,    38,    43,    49,
  56,    63,    71,    79,    88,    98,   108,   119,   131,   143,   157,   171,
  186,   202,   219,   237,   256,   275,   296,   318,   341,   365,   390,   416,
  444,   472,   502,   533,   566,   600,   635,   671,   709,   748,   789,   831,
  875,   920,   967,  1015,  1065,  1116,  1170,  1224,  1281,  1339,  1399,  1461,
  1525,  1590,  1657,  1726,  1797,  1870,  1945,  2021,  2100,  2181,  2263,  2348,
  2434,  2523,  2614,  2706,  2801,  2898,  2997,  3099,  3202,  3307,  3415,  3525,
  3637,  3751,  3867,  3986,  4106,  4229,  4354,  4481,  4611,  4742,  4876,  5012,
  5150,  5291,  5433,  5578,  5725,  5874,  6025,  6178,  6333,  6490,  6650,  6811,
  6974,  7140,  7307,  7476,  7647,  7820,  7995,  8171,  8349,  8529,  8711,  8894,
  9079,  9265,  9453,  9642,  9833, 10024, 10217, 10412, 10607, 10803, 11000, 11199,
  11398, 11597, 11797, 11998, 12200, 12401, 12603, 12805, 13008, 13210, 13412, 13614,
  13815, 14016, 14216, 14416, 14615, 14813, 15009, 15205, 15399, 15591, 15782, 15971,
  16157, 16342, 16524, 16704, 16881, 17056, 17227, 17395, 17559, 17720, 17877, 18030,
  18179, 18323, 18462, 18597, 18727, 18851, 18970, 19082, 19189, 19290, 19384, 19471,
  19551, 19623, 19689, 19746, 19795, 19835, 19867, 19890, 19904, 19908, 19902, 19886,
  19860, 19823, 19775, 19715, 19644, 19561, 19465, 19357, 19237, 19102, 18955, 18793,
  18618, 18428, 18223, 18004, 17769, 17518, 17252, 16970, 16672, 16357, 16025, 15677,
  15311, 14929, 14529, 14111, 13677, 13225, 12755, 12268, 11764, 11243, 10706, 10152,
  9583,  8998,  8399,  7787,  7162,  6527,  5883,  5231,  4576,  3919,  3265,  2620,
  1990,  1386,   825,   333
};

static const int16_t kPolyVecLo[12] = {
  29491, 26542, 23888, 21499, 19349, 17414, 15673, 14106, 12695, 11425, 10283, 9255
};
static const int16_t kPolyVecHi[6] = {
  26214, 20972, 16777, 13422, 10737, 8590
};

static __inline int32_t log2_Q8_LPC( uint32_t x ) {

  int32_t zeros;
  int16_t frac;

  zeros=WebRtcSpl_NormU32(x);
  frac = (int16_t)(((x << zeros) & 0x7FFFFFFF) >> 23);

  return ((31 - zeros) << 8) + frac;
}

static const int16_t kMulPitchGain = -25;
static const int16_t kChngFactor = 3523;
static const int16_t kExp2 = 11819;
const int kShiftLowerBand = 11;
const int kShiftHigherBand = 12;

void WebRtcIsacfix_GetVars(const int16_t *input, const int16_t *pitchGains_Q12,
                           uint32_t *oldEnergy, int16_t *varscale)
{
  int k;
  uint32_t nrgQ[4];
  int16_t nrgQlog[4];
  int16_t tmp16, chng1, chng2, chng3, chng4, tmp, chngQ, oldNrgQlog, pgQ, pg3;
  int32_t expPg32;
  int16_t expPg, divVal;
  int16_t tmp16_1, tmp16_2;

  nrgQ[0]=0;
  for (k = QLOOKAHEAD/2; k < (FRAMESAMPLES/4 + QLOOKAHEAD) / 2; k++) {
    nrgQ[0] += (uint32_t)(input[k] * input[k]);
  }
  nrgQ[1]=0;
  for ( ; k < (FRAMESAMPLES/2 + QLOOKAHEAD) / 2; k++) {
    nrgQ[1] += (uint32_t)(input[k] * input[k]);
  }
  nrgQ[2]=0;
  for ( ; k < (FRAMESAMPLES * 3 / 4 + QLOOKAHEAD) / 2; k++) {
    nrgQ[2] += (uint32_t)(input[k] * input[k]);
  }
  nrgQ[3]=0;
  for ( ; k < (FRAMESAMPLES + QLOOKAHEAD) / 2; k++) {
    nrgQ[3] += (uint32_t)(input[k] * input[k]);
  }

  for ( k=0; k<4; k++) {
    nrgQlog[k] = (int16_t)log2_Q8_LPC(nrgQ[k]);
  }
  oldNrgQlog = (int16_t)log2_Q8_LPC(*oldEnergy);

  chng1 = WEBRTC_SPL_ABS_W16(nrgQlog[3]-nrgQlog[2]);
  chng2 = WEBRTC_SPL_ABS_W16(nrgQlog[2]-nrgQlog[1]);
  chng3 = WEBRTC_SPL_ABS_W16(nrgQlog[1]-nrgQlog[0]);
  chng4 = WEBRTC_SPL_ABS_W16(nrgQlog[0]-oldNrgQlog);
  tmp = chng1+chng2+chng3+chng4;
  chngQ = (int16_t)(tmp * kChngFactor >> 10);
  chngQ += 2926;

  pgQ = 0;
  for (k=0; k<4; k++)
  {
    pgQ += pitchGains_Q12[k];
  }

  pg3 = (int16_t)(pgQ * pgQ >> 11);
  pg3 = (int16_t)(pgQ * pg3 >> 13);

  pg3 = (int16_t)(pg3 * kMulPitchGain >> 5);
  tmp16=(int16_t)WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(kExp2,pg3,13);
  if (tmp16<0) {
    tmp16_2 = (0x0400 | (tmp16 & 0x03FF));
    tmp16_1 = ((uint16_t)(tmp16 ^ 0xFFFF) >> 10) - 3;
    if (tmp16_1<0)
      expPg = -(tmp16_2 << -tmp16_1);
    else
      expPg = -(tmp16_2 >> tmp16_1);
  } else
    expPg = (int16_t) -16384;

  expPg32 = (int32_t)expPg << 8;
  divVal = WebRtcSpl_DivW32W16ResW16(expPg32, chngQ);

  tmp16=(int16_t)WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(kExp2,divVal,13);
  if (tmp16<0) {
    tmp16_2 = (0x0400 | (tmp16 & 0x03FF));
    tmp16_1 = ((uint16_t)(tmp16 ^ 0xFFFF) >> 10) - 3;
    if (tmp16_1<0)
      expPg = tmp16_2 << -tmp16_1;
    else
      expPg = tmp16_2 >> tmp16_1;
  } else
    expPg = (int16_t) 16384;

  *varscale = expPg-1;
  *oldEnergy = nrgQ[3];
}

static __inline int16_t  exp2_Q10_T(int16_t x) {

  int16_t tmp16_1, tmp16_2;

  tmp16_2=(int16_t)(0x0400|(x&0x03FF));
  tmp16_1 = -(x >> 10);
  if(tmp16_1>0)
    return tmp16_2 >> tmp16_1;
  else
    return tmp16_2 << -tmp16_1;

}

AutocorrFix WebRtcIsacfix_AutocorrFix;
CalculateResidualEnergy WebRtcIsacfix_CalculateResidualEnergy;

int32_t WebRtcIsacfix_CalculateResidualEnergyC(int lpc_order,
                                               int32_t q_val_corr,
                                               int q_val_polynomial,
                                               int16_t* a_polynomial,
                                               int32_t* corr_coeffs,
                                               int* q_val_residual_energy) {
  int i = 0, j = 0;
  int shift_internal = 0, shift_norm = 0;
  int32_t tmp32 = 0, word32_high = 0, word32_low = 0, residual_energy = 0;
  int64_t sum64 = 0, sum64_tmp = 0;

  for (i = 0; i <= lpc_order; i++) {
    for (j = i; j <= lpc_order; j++) {

      tmp32 = a_polynomial[j] * a_polynomial[j - i];

      if (i != 0) {
        tmp32 <<= 1;
      }
      sum64_tmp = (int64_t)tmp32 * (int64_t)corr_coeffs[i];
      sum64_tmp >>= shift_internal;

      if(((sum64_tmp > 0 && sum64 > 0) && (LLONG_MAX - sum64 < sum64_tmp)) ||
         ((sum64_tmp < 0 && sum64 < 0) && (LLONG_MIN - sum64 > sum64_tmp))) {

        shift_internal += 1;
        sum64 >>= 1;
        sum64 += sum64_tmp >> 1;
      } else {
        sum64 += sum64_tmp;
      }
    }
  }

  word32_high = (int32_t)(sum64 >> 32);
  word32_low = (int32_t)sum64;

  if(word32_high != 0) {
    shift_norm = 32 - WebRtcSpl_NormW32(word32_high);
    residual_energy = (int32_t)(sum64 >> shift_norm);
  } else {
    if((word32_low & 0x80000000) != 0) {
      shift_norm = 1;
      residual_energy = (uint32_t)word32_low >> 1;
    } else {
      shift_norm = WebRtcSpl_NormW32(word32_low);
      residual_energy = word32_low << shift_norm;
      shift_norm = -shift_norm;
    }
  }

  *q_val_residual_energy = q_val_corr - shift_internal - shift_norm
                           + q_val_polynomial * 2;

  return residual_energy;
}

void WebRtcIsacfix_GetLpcCoef(int16_t *inLoQ0,
                              int16_t *inHiQ0,
                              MaskFiltstr_enc *maskdata,
                              int16_t snrQ10,
                              const int16_t *pitchGains_Q12,
                              int32_t *gain_lo_hiQ17,
                              int16_t *lo_coeffQ15,
                              int16_t *hi_coeffQ15)
{
  int k, n, ii;
  int pos1, pos2;
  int sh_lo, sh_hi, sh, ssh, shMem;
  int16_t varscaleQ14;

  int16_t tmpQQlo, tmpQQhi;
  int32_t tmp32;
  int16_t tmp16,tmp16b;

  int16_t polyHI[ORDERHI+1];
  int16_t rcQ15_lo[ORDERLO], rcQ15_hi[ORDERHI];

  int16_t DataLoQ6[WINLEN], DataHiQ6[WINLEN];
  int32_t corrloQQ[ORDERLO+2];
  int32_t corrhiQQ[ORDERHI+1];
  int32_t corrlo2QQ[ORDERLO+1];
  int16_t scale;
  int16_t QdomLO, QdomHI, newQdomHI, newQdomLO;

  int32_t res_nrgQQ;
  int32_t sqrt_nrg;

  int16_t aaQ14;

  int16_t snrq;
  int shft;

  int16_t tmp16a;
  int32_t tmp32a, tmp32b, tmp32c;

  int16_t a_LOQ11[ORDERLO+1];
  int16_t k_vecloQ15[ORDERLO];
  int16_t a_HIQ12[ORDERHI+1];
  int16_t k_vechiQ15[ORDERHI];

  int16_t stab;

  snrq=snrQ10;

  tmp16 = (int16_t)(snrq * 172 >> 10);
  tmp16b = exp2_Q10_T(tmp16);
  snrq = (int16_t)(tmp16b * 285 >> 10);

  WebRtcIsacfix_GetVars(inLoQ0, pitchGains_Q12, &(maskdata->OldEnergy), &varscaleQ14);

  aaQ14 = (int16_t)((22938 * (8192 + (varscaleQ14 >> 1)) + 32768) >> 16);

  tmp16 = (int16_t)(aaQ14 * aaQ14 >> 15);
  tmpQQlo = 4096 + (tmp16 >> 1);

  tmp16 = 8192 + (aaQ14 >> 1);
  tmpQQhi = (int16_t)(tmp16 * tmp16 >> 14);

  for (pos1 = 0; pos1 < QLOOKAHEAD; pos1++) {
    maskdata->DataBufferLoQ0[pos1 + WINLEN - QLOOKAHEAD] = inLoQ0[pos1];
  }

  for (k = 0; k < SUBFRAMES; k++) {

    for (pos1 = 0; pos1 < WINLEN - UPDATE/2; pos1++) {
      maskdata->DataBufferLoQ0[pos1] = maskdata->DataBufferLoQ0[pos1 + UPDATE/2];
      maskdata->DataBufferHiQ0[pos1] = maskdata->DataBufferHiQ0[pos1 + UPDATE/2];
      DataLoQ6[pos1] = (int16_t)(maskdata->DataBufferLoQ0[pos1] *
          kWindowAutocorr[pos1] >> 15);
      DataHiQ6[pos1] = (int16_t)(maskdata->DataBufferHiQ0[pos1] *
          kWindowAutocorr[pos1] >> 15);
    }
    pos2 = (int16_t)(k * UPDATE / 2);
    for (n = 0; n < UPDATE/2; n++, pos1++) {
      maskdata->DataBufferLoQ0[pos1] = inLoQ0[QLOOKAHEAD + pos2];
      maskdata->DataBufferHiQ0[pos1] = inHiQ0[pos2++];
      DataLoQ6[pos1] = (int16_t)(maskdata->DataBufferLoQ0[pos1] *
          kWindowAutocorr[pos1] >> 15);
      DataHiQ6[pos1] = (int16_t)(maskdata->DataBufferHiQ0[pos1] *
          kWindowAutocorr[pos1] >> 15);
    }

    WebRtcIsacfix_AutocorrFix(corrloQQ,DataLoQ6,WINLEN, ORDERLO+1, &scale);
    QdomLO = 12-scale;
    sh_lo = WebRtcSpl_NormW32(corrloQQ[0]);
    QdomLO += sh_lo;
    for (ii=0; ii<ORDERLO+2; ii++) {
      corrloQQ[ii] <<= sh_lo;
    }

    WebRtcIsacfix_AutocorrFix(corrhiQQ,DataHiQ6,WINLEN, ORDERHI, &scale);

    QdomHI = 12-scale;
    sh_hi = WebRtcSpl_NormW32(corrhiQQ[0]);
    QdomHI += sh_hi;
    for (ii=0; ii<ORDERHI+1; ii++) {
      corrhiQQ[ii] <<= sh_hi;
    }

    corrlo2QQ[0] = (WEBRTC_SPL_MUL_16_32_RSFT16(tmpQQlo, corrloQQ[0]) >> 1) -
        (WEBRTC_SPL_MUL_16_32_RSFT16(aaQ14, corrloQQ[1]) >> 2);

    for (n = 1; n <= ORDERLO; n++) {

      tmp32 = (corrloQQ[n - 1] >> 1) + (corrloQQ[n + 1] >> 1);
      corrlo2QQ[n] = (WEBRTC_SPL_MUL_16_32_RSFT16(tmpQQlo, corrloQQ[n]) >> 1) -
          (WEBRTC_SPL_MUL_16_32_RSFT16(aaQ14, tmp32) >> 2);
    }
    QdomLO -= 5;

    for (n = 0; n <= ORDERHI; n++) {
      corrhiQQ[n] = WEBRTC_SPL_MUL_16_32_RSFT16(tmpQQhi, corrhiQQ[n]);
    }
    QdomHI -= 4;

    tmp32 = WEBRTC_SPL_SHIFT_W32((int32_t) 1, QdomLO-20);
    corrlo2QQ[0] += tmp32;
    tmp32 = WEBRTC_SPL_SHIFT_W32((int32_t) 1, QdomHI-20);
    corrhiQQ[0]  += tmp32;

    for (n = 0; n <= ORDERLO; n++) {
      corrlo2QQ[n] >>= 1;
    }
    QdomLO -= 1;

    for (n = 0; n <= ORDERHI; n++) {
      corrhiQQ[n] >>= 1;
    }
    QdomHI -= 1;

    newQdomLO = QdomLO;

    for (n = 0; n <= ORDERLO; n++) {
      int32_t tmp, tmpB, tmpCorr;
      int16_t alpha=328;
      int16_t beta=324;
      int16_t gamma=32440;

      if (maskdata->CorrBufLoQQ[n] != 0) {
        shMem=WebRtcSpl_NormW32(maskdata->CorrBufLoQQ[n]);
        sh = QdomLO - maskdata->CorrBufLoQdom[n];
        if (sh<=shMem) {
          tmp = WEBRTC_SPL_SHIFT_W32(maskdata->CorrBufLoQQ[n], sh);
          tmp = WEBRTC_SPL_MUL_16_32_RSFT15(alpha, tmp);
        } else if ((sh-shMem)<7){
          tmp = WEBRTC_SPL_SHIFT_W32(maskdata->CorrBufLoQQ[n], shMem);

          tmp = WEBRTC_SPL_MUL_16_32_RSFT15(alpha << (sh - shMem), tmp);
        } else {
          tmp = WEBRTC_SPL_SHIFT_W32(maskdata->CorrBufLoQQ[n], shMem);

          tmp = WEBRTC_SPL_MUL_16_32_RSFT15(alpha << 6, tmp);
          tmpCorr = corrloQQ[n] >> (sh - shMem - 6);
          tmp = tmp + tmpCorr;
          maskdata->CorrBufLoQQ[n] = tmp;
          newQdomLO = QdomLO-(sh-shMem-6);
          maskdata->CorrBufLoQdom[n] = newQdomLO;
        }
      } else
        tmp = 0;

      tmp = tmp + corrlo2QQ[n];

      maskdata->CorrBufLoQQ[n] = tmp;
      maskdata->CorrBufLoQdom[n] = QdomLO;

      tmp=WEBRTC_SPL_MUL_16_32_RSFT15(beta, tmp);
      tmpB=WEBRTC_SPL_MUL_16_32_RSFT15(gamma, corrlo2QQ[n]);
      corrlo2QQ[n] = tmp + tmpB;
    }
    if( newQdomLO!=QdomLO) {
      for (n = 0; n <= ORDERLO; n++) {
        if (maskdata->CorrBufLoQdom[n] != newQdomLO)
          corrloQQ[n] >>= maskdata->CorrBufLoQdom[n] - newQdomLO;
      }
      QdomLO = newQdomLO;
    }

    newQdomHI = QdomHI;

    for (n = 0; n <= ORDERHI; n++) {
      int32_t tmp, tmpB, tmpCorr;
      int16_t alpha=328;
      int16_t beta=324;
      int16_t gamma=32440;
      if (maskdata->CorrBufHiQQ[n] != 0) {
        shMem=WebRtcSpl_NormW32(maskdata->CorrBufHiQQ[n]);
        sh = QdomHI - maskdata->CorrBufHiQdom[n];
        if (sh<=shMem) {
          tmp = WEBRTC_SPL_SHIFT_W32(maskdata->CorrBufHiQQ[n], sh);
          tmp = WEBRTC_SPL_MUL_16_32_RSFT15(alpha, tmp);
          tmpCorr = corrhiQQ[n];
          tmp = tmp + tmpCorr;
          maskdata->CorrBufHiQQ[n] = tmp;
          maskdata->CorrBufHiQdom[n] = QdomHI;
        } else if ((sh-shMem)<7) {
          tmp = WEBRTC_SPL_SHIFT_W32(maskdata->CorrBufHiQQ[n], shMem);

          tmp = WEBRTC_SPL_MUL_16_32_RSFT15(alpha << (sh - shMem), tmp);
          tmpCorr = corrhiQQ[n];
          tmp = tmp + tmpCorr;
          maskdata->CorrBufHiQQ[n] = tmp;
          maskdata->CorrBufHiQdom[n] = QdomHI;
        } else {
          tmp = WEBRTC_SPL_SHIFT_W32(maskdata->CorrBufHiQQ[n], shMem);

          tmp = WEBRTC_SPL_MUL_16_32_RSFT15(alpha << 6, tmp);
          tmpCorr = corrhiQQ[n] >> (sh - shMem - 6);
          tmp = tmp + tmpCorr;
          maskdata->CorrBufHiQQ[n] = tmp;
          newQdomHI = QdomHI-(sh-shMem-6);
          maskdata->CorrBufHiQdom[n] = newQdomHI;
        }
      } else {
        tmp = corrhiQQ[n];
        tmpCorr = tmp;
        maskdata->CorrBufHiQQ[n] = tmp;
        maskdata->CorrBufHiQdom[n] = QdomHI;
      }

      tmp=WEBRTC_SPL_MUL_16_32_RSFT15(beta, tmp);
      tmpB=WEBRTC_SPL_MUL_16_32_RSFT15(gamma, tmpCorr);
      corrhiQQ[n] = tmp + tmpB;
    }

    if( newQdomHI!=QdomHI) {
      for (n = 0; n <= ORDERHI; n++) {
        if (maskdata->CorrBufHiQdom[n] != newQdomHI)
          corrhiQQ[n] >>= maskdata->CorrBufHiQdom[n] - newQdomHI;
      }
      QdomHI = newQdomHI;
    }

    stab=WebRtcSpl_LevinsonW32_JSK(corrlo2QQ, a_LOQ11, k_vecloQ15, ORDERLO);

    if (stab<0) {
      a_LOQ11[0]=2048;
      for (n = 1; n <= ORDERLO; n++) {
        a_LOQ11[n]=0;
      }

      stab=WebRtcSpl_LevinsonW32_JSK(corrlo2QQ, a_LOQ11, k_vecloQ15, 8);
    }

    WebRtcSpl_LevinsonDurbin(corrhiQQ,  a_HIQ12,  k_vechiQ15, ORDERHI);

    for (n = 1; n <= ORDERLO; n++) {
      a_LOQ11[n] = (int16_t)((kPolyVecLo[n - 1] * a_LOQ11[n] + (1 << 14)) >>
          15);
    }

    polyHI[0] = a_HIQ12[0];
    for (n = 1; n <= ORDERHI; n++) {
      a_HIQ12[n] = (int16_t)(((int32_t)(kPolyVecHi[n - 1] * a_HIQ12[n]) +
        (1 << 14)) >> 15);
      polyHI[n] = a_HIQ12[n];
    }

    sh = WebRtcSpl_NormW32(corrlo2QQ[0]);
    for (n = 0; n <= ORDERLO; n++) {
      corrlo2QQ[n] <<= sh;
    }
    QdomLO += sh;

    sh_lo = 31;
    res_nrgQQ = WebRtcIsacfix_CalculateResidualEnergy(ORDERLO, QdomLO,
        kShiftLowerBand, a_LOQ11, corrlo2QQ, &sh_lo);

    WebRtcSpl_AToK_JSK(a_LOQ11, ORDERLO, rcQ15_lo);

    if (sh_lo & 0x0001) {
      res_nrgQQ >>= 1;
      sh_lo-=1;
    }

    if( res_nrgQQ > 0 )
    {
      sqrt_nrg=WebRtcSpl_Sqrt(res_nrgQQ);

      tmp32a = varscaleQ14 >> 1;
      ssh = sh_lo >> 1;
      sh = ssh - 14;
      tmp32b = WEBRTC_SPL_SHIFT_W32(tmp32a, sh);
      tmp32c = sqrt_nrg + tmp32b;
      tmp32a = varscaleQ14 * snrq;

      sh = WebRtcSpl_NormW32(tmp32c);
      shft = 16 - sh;
      tmp16a = (int16_t) WEBRTC_SPL_SHIFT_W32(tmp32c, -shft);

      tmp32b = WebRtcSpl_DivW32W16(tmp32a, tmp16a);
      sh = ssh-shft-7;
      *gain_lo_hiQ17 = WEBRTC_SPL_SHIFT_W32(tmp32b, sh);
    }
    else
    {
      *gain_lo_hiQ17 = 100;
    }
    gain_lo_hiQ17++;

    for (n = 0; n < ORDERLO; n++) {
      *lo_coeffQ15 = (int16_t) (rcQ15_lo[n]);
      lo_coeffQ15++;
    }

    sh_hi = 31;
    res_nrgQQ = WebRtcIsacfix_CalculateResidualEnergy(ORDERHI, QdomHI,
        kShiftHigherBand, a_HIQ12, corrhiQQ, &sh_hi);

    WebRtcSpl_LpcToReflCoef(polyHI, ORDERHI, rcQ15_hi);

    if (sh_hi & 0x0001) {
      res_nrgQQ >>= 1;
      sh_hi-=1;
    }

    if( res_nrgQQ > 0 )
    {
      sqrt_nrg=WebRtcSpl_Sqrt(res_nrgQQ);

      tmp32a = varscaleQ14 >> 1;

      ssh = sh_hi >> 1;
      sh = ssh - 14;
      tmp32b = WEBRTC_SPL_SHIFT_W32(tmp32a, sh);
      tmp32c = sqrt_nrg + tmp32b;
      tmp32a = varscaleQ14 * snrq;

      sh = WebRtcSpl_NormW32(tmp32c);
      shft = 16 - sh;
      tmp16a = (int16_t) WEBRTC_SPL_SHIFT_W32(tmp32c, -shft);

      tmp32b = WebRtcSpl_DivW32W16(tmp32a, tmp16a);
      sh = ssh-shft-7;
      *gain_lo_hiQ17 = WEBRTC_SPL_SHIFT_W32(tmp32b, sh);
    }
    else
    {
      *gain_lo_hiQ17 = 100;
    }
    gain_lo_hiQ17++;

    for (n = 0; n < ORDERHI; n++) {
      *hi_coeffQ15 = rcQ15_hi[n];
      hi_coeffQ15++;
    }
  }
}
