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
 * WebRtcIsacfix_kTransform.c
 *
 * Transform functions
 *
 */

#include "modules/audio_coding/codecs/isac/fix/source/codec.h"
#include "modules/audio_coding/codecs/isac/fix/source/fft.h"
#include "modules/audio_coding/codecs/isac/fix/source/settings.h"
#include "modules/third_party/fft/fft.h"

extern const int16_t WebRtcIsacfix_kCosTab1[FRAMESAMPLES/2];

extern const int16_t WebRtcIsacfix_kSinTab1[FRAMESAMPLES/2];

extern const int16_t WebRtcIsacfix_kSinTab2[FRAMESAMPLES/4];

void WebRtcIsacfix_Time2SpecC(int16_t *inre1Q9,
                              int16_t *inre2Q9,
                              int16_t *outreQ7,
                              int16_t *outimQ7)
{

  int k;
  int32_t tmpreQ16[FRAMESAMPLES/2], tmpimQ16[FRAMESAMPLES/2];
  int16_t tmp1rQ14, tmp1iQ14;
  int32_t xrQ16, xiQ16, yrQ16, yiQ16;
  int32_t v1Q16, v2Q16;
  int16_t factQ19, sh;

  factQ19 = 16921;
  for (k = 0; k < FRAMESAMPLES/2; k++) {
    tmp1rQ14 = WebRtcIsacfix_kCosTab1[k];
    tmp1iQ14 = WebRtcIsacfix_kSinTab1[k];
    xrQ16 = (tmp1rQ14 * inre1Q9[k] + tmp1iQ14 * inre2Q9[k]) >> 7;
    xiQ16 = (tmp1rQ14 * inre2Q9[k] - tmp1iQ14 * inre1Q9[k]) >> 7;

    tmpreQ16[k] = (WEBRTC_SPL_MUL_16_32_RSFT16(factQ19, xrQ16) + 4) >> 3;
    tmpimQ16[k] = (WEBRTC_SPL_MUL_16_32_RSFT16(factQ19, xiQ16) + 4) >> 3;
  }

  xrQ16  = WebRtcSpl_MaxAbsValueW32(tmpreQ16, FRAMESAMPLES/2);
  yrQ16 = WebRtcSpl_MaxAbsValueW32(tmpimQ16, FRAMESAMPLES/2);
  if (yrQ16>xrQ16) {
    xrQ16 = yrQ16;
  }

  sh = WebRtcSpl_NormW32(xrQ16);
  sh = sh-24;

  if (sh>=0) {
    for (k=0; k<FRAMESAMPLES/2; k++) {
      inre1Q9[k] = (int16_t)(tmpreQ16[k] << sh);
      inre2Q9[k] = (int16_t)(tmpimQ16[k] << sh);
    }
  } else {
    int32_t round = 1 << (-sh - 1);
    for (k=0; k<FRAMESAMPLES/2; k++) {
      inre1Q9[k] = (int16_t)((tmpreQ16[k] + round) >> -sh);
      inre2Q9[k] = (int16_t)((tmpimQ16[k] + round) >> -sh);
    }
  }

  WebRtcIsacfix_FftRadix16Fastest(inre1Q9, inre2Q9, -1);

  if (sh>=0) {
    for (k=0; k<FRAMESAMPLES/2; k++) {
      tmpreQ16[k] = inre1Q9[k] >> sh;
      tmpimQ16[k] = inre2Q9[k] >> sh;
    }
  } else {
    for (k=0; k<FRAMESAMPLES/2; k++) {
      tmpreQ16[k] = inre1Q9[k] << -sh;
      tmpimQ16[k] = inre2Q9[k] << -sh;
    }
  }

  for (k = 0; k < FRAMESAMPLES/4; k++) {
    xrQ16 = tmpreQ16[k] + tmpreQ16[FRAMESAMPLES/2 - 1 - k];
    yiQ16 = -tmpreQ16[k] + tmpreQ16[FRAMESAMPLES/2 - 1 - k];
    xiQ16 = tmpimQ16[k] - tmpimQ16[FRAMESAMPLES/2 - 1 - k];
    yrQ16 = tmpimQ16[k] + tmpimQ16[FRAMESAMPLES/2 - 1 - k];
    tmp1rQ14 = -WebRtcIsacfix_kSinTab2[FRAMESAMPLES/4 - 1 - k];
    tmp1iQ14 = WebRtcIsacfix_kSinTab2[k];
    v1Q16 = WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, xrQ16) - WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, xiQ16);
    v2Q16 = WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, xrQ16) + WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, xiQ16);
    outreQ7[k] = (int16_t)(v1Q16 >> 9);
    outimQ7[k] = (int16_t)(v2Q16 >> 9);
    v1Q16 = -WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, yrQ16) - WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, yiQ16);
    v2Q16 = -WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, yrQ16) + WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, yiQ16);

    outreQ7[FRAMESAMPLES / 2 - 1 - k] = (int16_t)(v1Q16 >> 9);

    outimQ7[FRAMESAMPLES / 2 - 1 - k] = (int16_t)(v2Q16 >> 9);

  }
}

void WebRtcIsacfix_Spec2TimeC(int16_t *inreQ7, int16_t *inimQ7, int32_t *outre1Q16, int32_t *outre2Q16)
{

  int k;
  int16_t tmp1rQ14, tmp1iQ14;
  int32_t xrQ16, xiQ16, yrQ16, yiQ16;
  int32_t tmpInRe, tmpInIm, tmpInRe2, tmpInIm2;
  int16_t factQ11;
  int16_t sh;

  for (k = 0; k < FRAMESAMPLES/4; k++) {

    tmp1rQ14 = -WebRtcIsacfix_kSinTab2[FRAMESAMPLES/4 - 1 - k];
    tmp1iQ14 = WebRtcIsacfix_kSinTab2[k];

    tmpInRe = inreQ7[k] * (1 << 9);
    tmpInIm = inimQ7[k] * (1 << 9);
    tmpInRe2 = inreQ7[FRAMESAMPLES / 2 - 1 - k] * (1 << 9);
    tmpInIm2 = inimQ7[FRAMESAMPLES / 2 - 1 - k] * (1 << 9);

    xrQ16 = WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, tmpInRe) + WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, tmpInIm);
    xiQ16 = WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, tmpInIm) - WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, tmpInRe);
    yrQ16 = -WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, tmpInIm2) - WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, tmpInRe2);
    yiQ16 = -WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, tmpInRe2) + WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, tmpInIm2);

    outre1Q16[k] = xrQ16 - yiQ16;
    outre1Q16[FRAMESAMPLES/2 - 1 - k] = xrQ16 + yiQ16;
    outre2Q16[k] = xiQ16 + yrQ16;
    outre2Q16[FRAMESAMPLES/2 - 1 - k] = -xiQ16 + yrQ16;
  }

  tmpInRe  = WebRtcSpl_MaxAbsValueW32(outre1Q16, 240);
  tmpInIm = WebRtcSpl_MaxAbsValueW32(outre2Q16, 240);
  if (tmpInIm>tmpInRe) {
    tmpInRe = tmpInIm;
  }

  sh = WebRtcSpl_NormW32(tmpInRe);
  sh = sh-24;

  if (sh>=0) {
    for (k=0; k<240; k++) {
      inreQ7[k] = (int16_t)(outre1Q16[k] << sh);
      inimQ7[k] = (int16_t)(outre2Q16[k] << sh);
    }
  } else {
    int32_t round = 1 << (-sh - 1);
    for (k=0; k<240; k++) {
      inreQ7[k] = (int16_t)((outre1Q16[k] + round) >> -sh);
      inimQ7[k] = (int16_t)((outre2Q16[k] + round) >> -sh);
    }
  }

  WebRtcIsacfix_FftRadix16Fastest(inreQ7, inimQ7, 1);

  if (sh>=0) {
    for (k=0; k<240; k++) {
      outre1Q16[k] = inreQ7[k] >> sh;
      outre2Q16[k] = inimQ7[k] >> sh;
    }
  } else {
    for (k=0; k<240; k++) {
      outre1Q16[k] = inreQ7[k] * (1 << -sh);
      outre2Q16[k] = inimQ7[k] * (1 << -sh);
    }
  }

  for (k=0; k<240; k++) {
    outre1Q16[k] = WEBRTC_SPL_MUL_16_32_RSFT16(273, outre1Q16[k]);
    outre2Q16[k] = WEBRTC_SPL_MUL_16_32_RSFT16(273, outre2Q16[k]);
  }

  factQ11 = 31727;
  for (k = 0; k < FRAMESAMPLES/2; k++) {
    tmp1rQ14 = WebRtcIsacfix_kCosTab1[k];
    tmp1iQ14 = WebRtcIsacfix_kSinTab1[k];
    xrQ16 = WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, outre1Q16[k]) - WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, outre2Q16[k]);
    xiQ16 = WEBRTC_SPL_MUL_16_32_RSFT14(tmp1rQ14, outre2Q16[k]) + WEBRTC_SPL_MUL_16_32_RSFT14(tmp1iQ14, outre1Q16[k]);
    xrQ16 = WEBRTC_SPL_MUL_16_32_RSFT11(factQ11, xrQ16);
    xiQ16 = WEBRTC_SPL_MUL_16_32_RSFT11(factQ11, xiQ16);
    outre2Q16[k] = xiQ16;
    outre1Q16[k] = xrQ16;
  }
}
