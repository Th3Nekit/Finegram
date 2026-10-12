/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/isac/fix/source/pitch_estimator.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "modules/audio_coding/codecs/isac/fix/source/settings.h"
#include "modules/audio_coding/codecs/isac/fix/source/structs.h"
#include "rtc_base/compile_assert_c.h"

static const int kSegments = 5;

static const int16_t kDivFactor = 6553;

static const int16_t kIntrpCoef[PITCH_FRACS][PITCH_FRACORDER] = {
  {-367, 1090, -2706,  9945, 10596, -3318,  1626, -781,  287},
  {-325,  953, -2292,  7301, 12963, -3320,  1570, -743,  271},
  {-240,  693, -1622,  4634, 14809, -2782,  1262, -587,  212},
  {-125,  358,  -817,  2144, 15982, -1668,   721, -329,  118},
  {   0,    0,    -1,     1, 16380,     1,    -1,    0,    0},
  { 118, -329,   721, -1668, 15982,  2144,  -817,  358, -125},
  { 212, -587,  1262, -2782, 14809,  4634, -1622,  693, -240},
  { 271, -743,  1570, -3320, 12963,  7301, -2292,  953, -325}
};

static __inline size_t CalcLrIntQ(int16_t fixVal,
                                  int16_t qDomain) {
  int32_t roundVal = 1 << (qDomain - 1);

  return (fixVal + roundVal) >> qDomain;
}

void WebRtcIsacfix_PitchFilter(int16_t* indatQQ,

                               int16_t* outdatQQ,
                               PitchFiltstr* pfp,
                               int16_t* lagsQ7,
                               int16_t* gainsQ12,
                               int16_t type) {
  int    k, ind, cnt;
  int16_t sign = 1;
  int16_t inystateQQ[PITCH_DAMPORDER];
  int16_t ubufQQ[PITCH_INTBUFFSIZE + QLOOKAHEAD];
  const int16_t Gain = 21299;
  int16_t oldLagQ7;
  int16_t oldGainQ12, lagdeltaQ7, curLagQ7, gaindeltaQ12, curGainQ12;
  size_t frcQQ = 0;
  int32_t indW32 = 0;
  const int16_t* fracoeffQQ = NULL;

  RTC_COMPILE_ASSERT(PITCH_FRACORDER == 9);
  RTC_COMPILE_ASSERT(PITCH_DAMPORDER == 5);

  memcpy(ubufQQ, pfp->ubufQQ, sizeof(pfp->ubufQQ));
  memcpy(inystateQQ, pfp->ystateQQ, sizeof(inystateQQ));

  oldLagQ7 = pfp->oldlagQ7;
  oldGainQ12 = pfp->oldgainQ12;

  if (type == 4) {
    sign = -1;

    for (k = 0; k < PITCH_SUBFRAMES; k++) {
      gainsQ12[k] = (int16_t)(gainsQ12[k] * Gain >> 14);
    }
  }

  if (((lagsQ7[0] * 3 >> 1) < oldLagQ7) || (lagsQ7[0] > (oldLagQ7 * 3 >> 1))) {
    oldLagQ7 = lagsQ7[0];
    oldGainQ12 = gainsQ12[0];
  }

  ind = 0;

  for (k = 0; k < PITCH_SUBFRAMES; k++) {

    lagdeltaQ7 = lagsQ7[k] - oldLagQ7;
    lagdeltaQ7 = (int16_t)WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(
                  lagdeltaQ7, kDivFactor, 15);
    curLagQ7 = oldLagQ7;
    gaindeltaQ12 = gainsQ12[k] - oldGainQ12;
    gaindeltaQ12 = (int16_t)(gaindeltaQ12 * kDivFactor >> 15);

    curGainQ12 = oldGainQ12;
    oldLagQ7 = lagsQ7[k];
    oldGainQ12 = gainsQ12[k];

    for (cnt = 0; cnt < kSegments; cnt++) {

      curGainQ12 += gaindeltaQ12;
      curLagQ7 += lagdeltaQ7;
      indW32 = CalcLrIntQ(curLagQ7, 7);
      if (indW32 < PITCH_FRACORDER - 2) {

        indW32 = PITCH_FRACORDER - 2;
      }
      frcQQ = ((indW32 << 7) + 64 - curLagQ7) >> 4;

      if (frcQQ >= PITCH_FRACS) {
        frcQQ = 0;
      }
      fracoeffQQ = kIntrpCoef[frcQQ];

      WebRtcIsacfix_PitchFilterCore(PITCH_SUBFRAME_LEN / kSegments, curGainQ12,
        indW32, sign, inystateQQ, ubufQQ, fracoeffQQ, indatQQ, outdatQQ, &ind);
    }
  }

  memcpy(pfp->ubufQQ, ubufQQ + PITCH_FRAME_LEN, sizeof(pfp->ubufQQ));
  memcpy(pfp->ystateQQ, inystateQQ, sizeof(pfp->ystateQQ));

  pfp->oldlagQ7 = oldLagQ7;
  pfp->oldgainQ12 = oldGainQ12;

  if (type == 2) {

    WebRtcIsacfix_PitchFilterCore(QLOOKAHEAD, curGainQ12, indW32, 1, inystateQQ,
                ubufQQ, fracoeffQQ, indatQQ, outdatQQ, &ind);
  }
}

void WebRtcIsacfix_PitchFilterGains(const int16_t* indatQ0,
                                    PitchFiltstr* pfp,
                                    int16_t* lagsQ7,
                                    int16_t* gainsQ12) {
  int  k, n, m;
  size_t ind, pos, pos3QQ;

  int16_t ubufQQ[PITCH_INTBUFFSIZE];
  int16_t oldLagQ7, lagdeltaQ7, curLagQ7;
  const int16_t* fracoeffQQ = NULL;
  int16_t scale;
  int16_t cnt = 0, tmpW16;
  size_t frcQQ, indW16 = 0;
  int32_t tmpW32, tmp2W32, csum1QQ, esumxQQ;

  memcpy(ubufQQ, pfp->ubufQQ, sizeof(pfp->ubufQQ));
  oldLagQ7 = pfp->oldlagQ7;

  if (((lagsQ7[0] * 3 >> 1) < oldLagQ7) || (lagsQ7[0] > (oldLagQ7 * 3 >> 1))) {
    oldLagQ7 = lagsQ7[0];
  }

  ind = 0;
  pos = ind + PITCH_BUFFSIZE;
  scale = 0;
  for (k = 0; k < PITCH_SUBFRAMES; k++) {

    lagdeltaQ7 = lagsQ7[k] - oldLagQ7;
    lagdeltaQ7 = (int16_t)WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(
                   lagdeltaQ7, kDivFactor, 15);
    curLagQ7 = oldLagQ7;
    oldLagQ7 = lagsQ7[k];

    csum1QQ = 1;
    esumxQQ = 1;

    for (cnt = 0; cnt < kSegments; cnt++) {

      curLagQ7 += lagdeltaQ7;
      indW16 = CalcLrIntQ(curLagQ7, 7);
      frcQQ = ((indW16 << 7) + 64 - curLagQ7) >> 4;

      if (frcQQ >= PITCH_FRACS) {
        frcQQ = 0;
      }
      fracoeffQQ = kIntrpCoef[frcQQ];

      pos3QQ = pos - (indW16 + 4);

      for (n = 0; n < PITCH_SUBFRAME_LEN / kSegments; n++) {

        tmpW32 = 0;
        for (m = 0; m < PITCH_FRACORDER; m++) {
          tmpW32 += ubufQQ[pos3QQ + m] * fracoeffQQ[m];
        }

        ubufQQ[pos] = indatQ0[ind];

        tmp2W32 = WEBRTC_SPL_MUL_16_32_RSFT14(indatQ0[ind], tmpW32);
        tmpW32 += 8192;
        tmpW16 = tmpW32 >> 14;
        tmpW32 = tmpW16 * tmpW16;

        if ((tmp2W32 > 1073700000) || (csum1QQ > 1073700000) ||
            (tmpW32 > 1073700000) || (esumxQQ > 1073700000)) {
          scale++;
          csum1QQ >>= 1;
          esumxQQ >>= 1;
        }
        csum1QQ += tmp2W32 >> scale;
        esumxQQ += tmpW32 >> scale;

        ind++;
        pos++;
        pos3QQ++;
      }
    }

    if (csum1QQ < esumxQQ) {
      tmp2W32 = WebRtcSpl_DivResultInQ31(csum1QQ, esumxQQ);

      tmpW32 = tmp2W32 >> 20;
    } else {
      tmpW32 = 4096;
    }
    gainsQ12[k] = (int16_t)WEBRTC_SPL_SAT(PITCH_MAX_GAIN_Q12, tmpW32, 0);
  }

  memcpy(pfp->ubufQQ, ubufQQ + PITCH_FRAME_LEN, sizeof(pfp->ubufQQ));
  pfp->oldlagQ7 = lagsQ7[PITCH_SUBFRAMES - 1];
  pfp->oldgainQ12 = gainsQ12[PITCH_SUBFRAMES - 1];

}
