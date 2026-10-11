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
 * bandwidth_estimator.c
 *
 * This file contains the code for the Bandwidth Estimator designed
 * for iSAC.
 *
 * NOTE! Castings needed for C55, do not remove!
 *
 */

#include "modules/audio_coding/codecs/isac/fix/source/bandwidth_estimator.h"

#include "modules/audio_coding/codecs/isac/fix/source/settings.h"
#include "rtc_base/checks.h"

static const int16_t kQRateTable[12] = {
  10000, 11115, 12355, 13733, 15265, 16967,
  18860, 20963, 23301, 25900, 28789, 32000
};

static const int32_t KQRate01[12] = {
  65536000,  72843264,  80969728,  90000589,  100040704, 111194931,
  123600896, 137383117, 152705434, 169738240, 188671590, 209715200
};

static const int32_t kBitsByteSec = 4369000;

static const int16_t kRecHeaderRate[2] = {
  9333, 4666
};

static const int32_t kInvBandwidth[4] = {
  55539, 25978,
  73213, 29284
};

static const int32_t kSamplesIn25msec = 400;

int32_t WebRtcIsacfix_InitBandwidthEstimator(BwEstimatorstr *bweStr)
{
  bweStr->prevFrameSizeMs       = INIT_FRAME_LEN;
  bweStr->prevRtpNumber         = 0;
  bweStr->prevSendTime          = 0;
  bweStr->prevArrivalTime       = 0;
  bweStr->prevRtpRate           = 1;
  bweStr->lastUpdate            = 0;
  bweStr->lastReduction         = 0;
  bweStr->countUpdates          = -9;

  bweStr->recBwInv              = 43531;
  bweStr->recBw                 = INIT_BN_EST;
  bweStr->recBwAvgQ             = INIT_BN_EST_Q7;
  bweStr->recBwAvg              = INIT_REC_BN_EST_Q5;
  bweStr->recJitter             = (int32_t) 327680;
  bweStr->recJitterShortTerm    = 0;
  bweStr->recJitterShortTermAbs = (int32_t) 40960;
  bweStr->recMaxDelay           = (int32_t) 10;
  bweStr->recMaxDelayAvgQ       = (int32_t) 5120;
  bweStr->recHeaderRate         = INIT_HDR_RATE;
  bweStr->countRecPkts          = 0;
  bweStr->sendBwAvg             = INIT_BN_EST_Q7;
  bweStr->sendMaxDelayAvg       = (int32_t) 5120;

  bweStr->countHighSpeedRec     = 0;
  bweStr->highSpeedRec          = 0;
  bweStr->countHighSpeedSent    = 0;
  bweStr->highSpeedSend         = 0;
  bweStr->inWaitPeriod          = 0;

  bweStr->maxBwInv              = kInvBandwidth[3];
  bweStr->minBwInv              = kInvBandwidth[2];

  bweStr->external_bw_info.in_use = 0;

  return 0;
}

int32_t WebRtcIsacfix_UpdateUplinkBwImpl(BwEstimatorstr *bweStr,
                                         const uint16_t rtpNumber,
                                         const int16_t  frameSize,
                                         const uint32_t sendTime,
                                         const uint32_t arrivalTime,
                                         const size_t   pksize,
                                         const uint16_t Index)
{
  uint16_t  weight = 0;
  uint32_t  currBwInv = 0;
  uint16_t  recRtpRate;
  uint32_t  arrTimeProj;
  int32_t   arrTimeDiff;
  int32_t   arrTimeNoise;
  int32_t   arrTimeNoiseAbs;
  int32_t   sendTimeDiff;

  int32_t delayCorrFactor = DELAY_CORRECTION_MED;
  int32_t lateDiff = 0;
  int16_t immediateSet = 0;
  int32_t frameSizeSampl;

  int32_t  temp;
  int32_t  msec;
  uint32_t exponent;
  uint32_t reductionFactor;
  uint32_t numBytesInv;
  int32_t  sign;

  uint32_t byteSecondsPerBit;
  uint32_t tempLower;
  uint32_t tempUpper;
  int32_t recBwAvgInv;
  int32_t numPktsExpected;

  int16_t errCode;

  RTC_DCHECK(!bweStr->external_bw_info.in_use);

  errCode = WebRtcIsacfix_UpdateUplinkBwRec(bweStr, Index);
  if (errCode <0) {
    return(errCode);
  }

  if (frameSize == 60) {

    if ( (frameSize != bweStr->prevFrameSizeMs) && (bweStr->countUpdates > 0)) {
      bweStr->countUpdates = 10;
      bweStr->recHeaderRate = kRecHeaderRate[1];

      bweStr->maxBwInv = kInvBandwidth[3];
      bweStr->minBwInv = kInvBandwidth[2];
      bweStr->recBwInv = 1073741824 / (bweStr->recBw + bweStr->recHeaderRate);
    }

    recRtpRate = (int16_t)((kBitsByteSec * pksize) >> 15) +
        bweStr->recHeaderRate;

  } else {

    if ( (frameSize != bweStr->prevFrameSizeMs) && (bweStr->countUpdates > 0)) {
      bweStr->countUpdates = 10;
      bweStr->recHeaderRate = kRecHeaderRate[0];

      bweStr->maxBwInv = kInvBandwidth[1];
      bweStr->minBwInv = kInvBandwidth[0];
      bweStr->recBwInv = 1073741824 / (bweStr->recBw + bweStr->recHeaderRate);
    }

    recRtpRate = (uint16_t)((kBitsByteSec * pksize) >> 14) +
        bweStr->recHeaderRate;
  }

  if (arrivalTime < bweStr->prevArrivalTime) {
    bweStr->prevArrivalTime = arrivalTime;
    bweStr->lastUpdate      = arrivalTime;
    bweStr->lastReduction   = arrivalTime + FS3;

    bweStr->countRecPkts      = 0;

    bweStr->prevFrameSizeMs = frameSize;

    bweStr->prevRtpRate = recRtpRate;

    bweStr->prevRtpNumber = rtpNumber;

    return 0;
  }

  bweStr->countRecPkts++;

  frameSizeSampl = SAMPLES_PER_MSEC * frameSize;

  if ( bweStr->countUpdates > 0 ) {

    if(bweStr->inWaitPeriod) {
      if ((arrivalTime - bweStr->startWaitPeriod)> FS_1_HALF) {
        bweStr->inWaitPeriod = 0;
      }
    }

    sendTimeDiff = sendTime - bweStr->prevSendTime;
    if (sendTimeDiff <= frameSizeSampl * 2) {

      if ((arrivalTime - bweStr->lastUpdate) > FS3) {

        numPktsExpected = (arrivalTime - bweStr->lastUpdate) / frameSizeSampl;

        if ((int32_t)bweStr->countRecPkts << 10 > 922 * numPktsExpected) {

          msec = (arrivalTime - bweStr->lastReduction);

          if (msec > 208000) {
            msec = 208000;
          }

          exponent = WEBRTC_SPL_UMUL(0x0000004C, msec);

          reductionFactor = WEBRTC_SPL_RSHIFT_U32(0x01000000 | (exponent & 0x00FFFFFF),
                                                  WEBRTC_SPL_RSHIFT_U32(exponent, 24));

          reductionFactor = WEBRTC_SPL_RSHIFT_U32(reductionFactor, 11);

          if ( reductionFactor != 0 ) {
            bweStr->recBwInv = WEBRTC_SPL_MUL((int32_t)bweStr->recBwInv, (int32_t)reductionFactor);
            bweStr->recBwInv = (int32_t)bweStr->recBwInv >> 13;

          } else {
            static const uint32_t kInitRate = INIT_BN_EST + INIT_HDR_RATE;

            bweStr->recBwInv = (1073741824 + kInitRate / 2) / kInitRate;
          }

          bweStr->lastReduction = arrivalTime;
        } else {

          bweStr->lastReduction = arrivalTime + FS3;
          bweStr->lastUpdate    = arrivalTime;
          bweStr->countRecPkts  = 0;
        }
      }
    } else {
      bweStr->lastReduction = arrivalTime + FS3;
      bweStr->lastUpdate    = arrivalTime;
      bweStr->countRecPkts  = 0;
    }

    if ( rtpNumber == bweStr->prevRtpNumber + 1 ) {
      arrTimeDiff = arrivalTime - bweStr->prevArrivalTime;

      if (!(bweStr->highSpeedSend && bweStr->highSpeedRec)) {
        if (arrTimeDiff > frameSizeSampl) {
          if (sendTimeDiff > 0) {
            lateDiff = arrTimeDiff - sendTimeDiff - frameSizeSampl * 2;
          } else {
            lateDiff = arrTimeDiff - frameSizeSampl;
          }

          if (lateDiff > 8000) {
            delayCorrFactor = (int32_t) DELAY_CORRECTION_MAX;
            bweStr->inWaitPeriod = 1;
            bweStr->startWaitPeriod = arrivalTime;
            immediateSet = 1;
          } else if (lateDiff > 5120) {
            delayCorrFactor = (int32_t) DELAY_CORRECTION_MED;
            immediateSet = 1;
            bweStr->inWaitPeriod = 1;
            bweStr->startWaitPeriod = arrivalTime;
          }
        }
      }

      if ((bweStr->prevRtpRate > (int32_t)bweStr->recBwAvg >> 5) &&
          (recRtpRate > (int32_t)bweStr->recBwAvg >> 5) &&
          !bweStr->inWaitPeriod) {

        if (bweStr->countUpdates++ > 99) {

          weight = (uint16_t) 82;
        } else {

          weight = (uint16_t) WebRtcSpl_DivW32W16(
              8192 + (bweStr->countUpdates >> 1),
              (int16_t)bweStr->countUpdates);
        }

        if (arrTimeDiff > frameSizeSampl + kSamplesIn25msec) {
          arrTimeDiff = frameSizeSampl + kSamplesIn25msec;
        }

        if (arrTimeDiff < frameSizeSampl - FRAMESAMPLES_10ms) {
          arrTimeDiff = frameSizeSampl - FRAMESAMPLES_10ms;
        }

        numBytesInv = (uint16_t) WebRtcSpl_DivW32W16(
            (int32_t)(524288 + ((pksize + HEADER_SIZE) >> 1)),
            (int16_t)(pksize + HEADER_SIZE));

        byteSecondsPerBit = (uint32_t)(arrTimeDiff * 8389);

        tempUpper = WEBRTC_SPL_RSHIFT_U32(byteSecondsPerBit, 15);

        tempLower = byteSecondsPerBit & 0x00007FFF;

        tempUpper = WEBRTC_SPL_MUL(tempUpper, numBytesInv);
        tempLower = WEBRTC_SPL_MUL(tempLower, numBytesInv);
        tempLower = WEBRTC_SPL_RSHIFT_U32(tempLower, 15);

        currBwInv = tempUpper + tempLower;
        currBwInv = WEBRTC_SPL_RSHIFT_U32(currBwInv, 4);

        if(currBwInv < bweStr->maxBwInv) {
          currBwInv = bweStr->maxBwInv;
        } else if(currBwInv > bweStr->minBwInv) {
          currBwInv = bweStr->minBwInv;
        }

        bweStr->recBwInv = WEBRTC_SPL_UMUL(weight, currBwInv) +
            WEBRTC_SPL_UMUL((uint32_t) 8192 - weight, bweStr->recBwInv);

        bweStr->recBwInv = WEBRTC_SPL_RSHIFT_U32(bweStr->recBwInv, 13);

        bweStr->lastUpdate    = arrivalTime;
        bweStr->lastReduction = arrivalTime + FS3;
        bweStr->countRecPkts  = 0;

        recBwAvgInv = (0x80000000 + bweStr->recBwAvg / 2) / bweStr->recBwAvg;

        arrTimeProj = WEBRTC_SPL_MUL((int32_t)8000, recBwAvgInv);

        arrTimeProj = WEBRTC_SPL_RSHIFT_U32(arrTimeProj, 4);

        arrTimeProj = WEBRTC_SPL_MUL(((int32_t)pksize + HEADER_SIZE), arrTimeProj);

        arrTimeProj = WEBRTC_SPL_RSHIFT_U32(arrTimeProj, 12);

        if ((arrTimeDiff << 6) > (int32_t)arrTimeProj) {
          arrTimeNoise = (arrTimeDiff << 6) - arrTimeProj;
          sign = 1;
        } else {
          arrTimeNoise = arrTimeProj - (arrTimeDiff << 6);
          sign = -1;
        }

        arrTimeNoiseAbs = arrTimeNoise;

        weight >>= 3;
        bweStr->recJitter = weight * (arrTimeNoiseAbs << 5) +
            (1024 - weight) * bweStr->recJitter;

        bweStr->recJitter >>= 10;

        if (bweStr->recJitter > (int32_t)327680) {
          bweStr->recJitter = (int32_t)327680;
        }

        bweStr->recJitterShortTermAbs = 51 * (arrTimeNoiseAbs << 3) +
            WEBRTC_SPL_MUL(973, bweStr->recJitterShortTermAbs);
        bweStr->recJitterShortTermAbs >>= 10;

        bweStr->recJitterShortTerm = 205 * (arrTimeNoise << 3) * sign +
            WEBRTC_SPL_MUL(3891, bweStr->recJitterShortTerm);

        if (bweStr->recJitterShortTerm < 0) {
          temp = -bweStr->recJitterShortTerm;
          temp >>= 12;
          bweStr->recJitterShortTerm = -temp;
        } else {
          bweStr->recJitterShortTerm >>= 12;
        }
      }
    }
  } else {

    bweStr->lastUpdate    = arrivalTime;
    bweStr->lastReduction = arrivalTime + FS3;
    bweStr->countRecPkts  = 0;
    bweStr->countUpdates++;
  }

  if (bweStr->recBwInv > bweStr->minBwInv) {
    bweStr->recBwInv = bweStr->minBwInv;
  } else if (bweStr->recBwInv < bweStr->maxBwInv) {
    bweStr->recBwInv = bweStr->maxBwInv;
  }

  bweStr->prevFrameSizeMs = frameSize;

  bweStr->prevRtpRate = recRtpRate;

  bweStr->prevRtpNumber = rtpNumber;

  if (bweStr->prevArrivalTime != 0xffffffff) {
    bweStr->recMaxDelay = WEBRTC_SPL_MUL(3, bweStr->recJitter);
  }

  bweStr->prevArrivalTime = arrivalTime;
  bweStr->prevSendTime = sendTime;

  bweStr->recBw = 1073741824 / bweStr->recBwInv - bweStr->recHeaderRate;

  if (immediateSet) {

    bweStr->recBw = WEBRTC_SPL_UMUL(delayCorrFactor, bweStr->recBw);
    bweStr->recBw = WEBRTC_SPL_RSHIFT_U32(bweStr->recBw, 10);

    if (bweStr->recBw < (int32_t) MIN_ISAC_BW) {
      bweStr->recBw = (int32_t) MIN_ISAC_BW;
    }

    bweStr->recBwAvg = (bweStr->recBw + bweStr->recHeaderRate) << 5;

    bweStr->recBwAvgQ = bweStr->recBw << 7;

    bweStr->recJitterShortTerm = 0;

    bweStr->recBwInv = 1073741824 / (bweStr->recBw + bweStr->recHeaderRate);

    immediateSet = 0;
  }

  return 0;
}

int16_t WebRtcIsacfix_UpdateUplinkBwRec(BwEstimatorstr *bweStr,
                                        const int16_t Index)
{
  uint16_t RateInd;

  RTC_DCHECK(!bweStr->external_bw_info.in_use);

  if ( (Index < 0) || (Index > 23) ) {
    return -ISAC_RANGE_ERROR_BW_ESTIMATOR;
  }

  if ( Index > 11 ) {
    RateInd = Index - 12;

    bweStr->sendMaxDelayAvg = WEBRTC_SPL_MUL(461, bweStr->sendMaxDelayAvg) +
        51 * (MAX_ISAC_MD << 9);
    bweStr->sendMaxDelayAvg >>= 9;

  } else {
    RateInd = Index;

    bweStr->sendMaxDelayAvg = WEBRTC_SPL_MUL(461, bweStr->sendMaxDelayAvg) +
        51 * (MIN_ISAC_MD << 9);
    bweStr->sendMaxDelayAvg >>= 9;

  }

  bweStr->sendBwAvg = 461 * bweStr->sendBwAvg +
      51 * ((uint32_t)kQRateTable[RateInd] << 7);
  bweStr->sendBwAvg = WEBRTC_SPL_RSHIFT_U32(bweStr->sendBwAvg, 9);

  if (WEBRTC_SPL_RSHIFT_U32(bweStr->sendBwAvg, 7) > 28000 && !bweStr->highSpeedSend) {
    bweStr->countHighSpeedSent++;

    if (bweStr->countHighSpeedSent >= 66) {
      bweStr->highSpeedSend = 1;
    }
  } else if (!bweStr->highSpeedSend) {
    bweStr->countHighSpeedSent = 0;
  }

  return 0;
}

uint16_t WebRtcIsacfix_GetDownlinkBwIndexImpl(BwEstimatorstr *bweStr)
{
  int32_t  rate;
  int32_t  maxDelay;
  uint16_t rateInd;
  uint16_t maxDelayBit;
  int32_t  tempTerm1;
  int32_t  tempTerm2;
  int32_t  tempTermX;
  int32_t  tempTermY;
  int32_t  tempMin;
  int32_t  tempMax;

  if (bweStr->external_bw_info.in_use)
    return bweStr->external_bw_info.bottleneck_idx;

  rate = WebRtcIsacfix_GetDownlinkBandwidth(bweStr);

  bweStr->recBwAvg = 922 * bweStr->recBwAvg +
      102 * (((uint32_t)rate + bweStr->recHeaderRate) << 5);
  bweStr->recBwAvg = WEBRTC_SPL_RSHIFT_U32(bweStr->recBwAvg, 10);

  for (rateInd = 1; rateInd < 11; rateInd++) {
    if (rate <= kQRateTable[rateInd]){
      break;
    }
  }

  tempTerm1 = WEBRTC_SPL_MUL(bweStr->recBwAvgQ, 25);
  tempTerm1 >>= 7;
  tempTermX = WEBRTC_SPL_UMUL(461, bweStr->recBwAvgQ) - tempTerm1;

  tempTermY = rate << 16;

  tempTerm1 = tempTermX + KQRate01[rateInd] - tempTermY;
  tempTerm2 = tempTermY - tempTermX - KQRate01[rateInd-1];

  if (tempTerm1  > tempTerm2) {
    rateInd--;
  }

  tempTermX += KQRate01[rateInd];

  bweStr->recBwAvgQ = tempTermX >> 9;

  if ((bweStr->recBwAvgQ > 3584000) && !bweStr->highSpeedRec) {
    bweStr->countHighSpeedRec++;
    if (bweStr->countHighSpeedRec >= 66) {
      bweStr->highSpeedRec = 1;
    }
  } else if (!bweStr->highSpeedRec)    {
    bweStr->countHighSpeedRec = 0;
  }

  maxDelay = WebRtcIsacfix_GetDownlinkMaxDelay(bweStr);

  tempMax = 652800;
  tempMin = 130560;
  tempTermX = WEBRTC_SPL_MUL((int32_t)bweStr->recMaxDelayAvgQ, (int32_t)461);
  tempTermY = maxDelay << 18;

  tempTerm1 = tempTermX + tempMax - tempTermY;
  tempTerm2 = tempTermY - tempTermX - tempMin;

  if ( tempTerm1 > tempTerm2) {
    maxDelayBit = 0;
    tempTerm1 = tempTermX + tempMin;

    bweStr->recMaxDelayAvgQ = tempTerm1 >> 9;
  } else {
    maxDelayBit = 12;
    tempTerm1 =  tempTermX + tempMax;

    bweStr->recMaxDelayAvgQ = tempTerm1 >> 9;
  }

  return (uint16_t)(rateInd + maxDelayBit);
}

uint16_t WebRtcIsacfix_GetDownlinkBandwidth(const BwEstimatorstr *bweStr)
{
  uint32_t  recBw;
  int32_t   jitter_sign;
  int32_t   bw_adjust;
  int32_t   rec_jitter_short_term_abs_inv;
  int32_t   temp;

  RTC_DCHECK(!bweStr->external_bw_info.in_use);

  rec_jitter_short_term_abs_inv = 0x80000000u / bweStr->recJitterShortTermAbs;

  jitter_sign = (bweStr->recJitterShortTerm >> 4) *
      rec_jitter_short_term_abs_inv;

  if (jitter_sign < 0) {
    temp = -jitter_sign;
    temp >>= 19;
    jitter_sign = -temp;
  } else {
    jitter_sign >>= 19;
  }

  temp = 9830 + ((38 * jitter_sign * jitter_sign) >> 8);

  if (jitter_sign < 0) {
    temp = WEBRTC_SPL_MUL(jitter_sign, temp);
    temp = -temp;
    temp >>= 8;
    bw_adjust = (uint32_t)65536 + temp;
  } else {

    bw_adjust = 65536 - ((jitter_sign * temp) >> 8);
  }

  bw_adjust >>= 2;

  recBw = WEBRTC_SPL_UMUL(bweStr->recBw, bw_adjust);

  recBw >>= 14;

  if (recBw < MIN_ISAC_BW) {
    recBw = MIN_ISAC_BW;
  } else if (recBw > MAX_ISAC_BW) {
    recBw = MAX_ISAC_BW;
  }

  return  (uint16_t) recBw;
}

int16_t WebRtcIsacfix_GetDownlinkMaxDelay(const BwEstimatorstr *bweStr)
{
  int16_t recMaxDelay = (int16_t)(bweStr->recMaxDelay >> 15);

  RTC_DCHECK(!bweStr->external_bw_info.in_use);

  if (recMaxDelay < MIN_ISAC_MD) {
    recMaxDelay = MIN_ISAC_MD;
  } else if (recMaxDelay > MAX_ISAC_MD) {
    recMaxDelay = MAX_ISAC_MD;
  }

  return recMaxDelay;
}

static int16_t clamp(int16_t val, int16_t min, int16_t max) {
  RTC_DCHECK_LE(min, max);
  return val < min ? min : (val > max ? max : val);
}

int16_t WebRtcIsacfix_GetUplinkBandwidth(const BwEstimatorstr* bweStr) {
  return bweStr->external_bw_info.in_use
             ? bweStr->external_bw_info.send_bw_avg
             : clamp(bweStr->sendBwAvg >> 7, MIN_ISAC_BW, MAX_ISAC_BW);
}

int16_t WebRtcIsacfix_GetUplinkMaxDelay(const BwEstimatorstr* bweStr) {
  return bweStr->external_bw_info.in_use
             ? bweStr->external_bw_info.send_max_delay_avg
             : clamp(bweStr->sendMaxDelayAvg >> 9, MIN_ISAC_MD, MAX_ISAC_MD);
}

uint16_t WebRtcIsacfix_GetMinBytes(RateModel *State,
                                   int16_t StreamSize,
                                   const int16_t FrameSamples,
                                   const int16_t BottleNeck,
                                   const int16_t DelayBuildUp)
{
  int32_t MinRate = 0;
  uint16_t    MinBytes;
  int16_t TransmissionTime;
  int32_t inv_Q12;
  int32_t den;

  if (State->InitCounter > 0) {
    if (State->InitCounter-- <= INIT_BURST_LEN) {
      MinRate = INIT_RATE;
    } else {
      MinRate = 0;
    }
  } else {

    if (State->BurstCounter) {
      if (State->StillBuffered <
          (((512 - 512 / BURST_LEN) * DelayBuildUp) >> 9)) {

        inv_Q12 = 4096 / (BURST_LEN * FrameSamples);
        MinRate = (512 + SAMPLES_PER_MSEC * ((DelayBuildUp * inv_Q12) >> 3)) *
            BottleNeck;
      } else {

        inv_Q12 = 4096 / FrameSamples;
        if (DelayBuildUp > State->StillBuffered) {
          MinRate = (512 + SAMPLES_PER_MSEC * (((DelayBuildUp -
              State->StillBuffered) * inv_Q12) >> 3)) * BottleNeck;
        } else if ((den = WEBRTC_SPL_MUL(SAMPLES_PER_MSEC, (State->StillBuffered - DelayBuildUp))) >= FrameSamples) {

          MinRate = 0;
        } else {
          MinRate = (512 - ((den * inv_Q12) >> 3)) * BottleNeck;
        }

        if (MinRate < WEBRTC_SPL_MUL(532, BottleNeck)) {
          MinRate += WEBRTC_SPL_MUL(22, BottleNeck);
        }
      }

      State->BurstCounter--;
    }
  }

  MinRate += 256;
  MinRate >>= 9;
  MinBytes = MinRate * FrameSamples / FS8;

  if (StreamSize < MinBytes) {
    StreamSize = MinBytes;
  }

  if ((StreamSize * (int32_t)FS8) / FrameSamples > (517 * BottleNeck) >> 9) {
    if (State->PrevExceed) {

      State->ExceedAgo -= BURST_INTERVAL / (BURST_LEN - 1);
      if (State->ExceedAgo < 0) {
        State->ExceedAgo = 0;
      }
    } else {
      State->ExceedAgo += FrameSamples / SAMPLES_PER_MSEC;
      State->PrevExceed = 1;
    }
  } else {
    State->PrevExceed = 0;
    State->ExceedAgo += FrameSamples / SAMPLES_PER_MSEC;
  }

  if ((State->ExceedAgo > BURST_INTERVAL) && (State->BurstCounter == 0)) {
    if (State->PrevExceed) {
      State->BurstCounter = BURST_LEN - 1;
    } else {
      State->BurstCounter = BURST_LEN;
    }
  }

  TransmissionTime = (StreamSize * 8000) / BottleNeck;
  State->StillBuffered += TransmissionTime;
  State->StillBuffered -= FrameSamples / SAMPLES_PER_MSEC;
  if (State->StillBuffered < 0) {
    State->StillBuffered = 0;
  }

  if (State->StillBuffered > 2000) {
    State->StillBuffered = 2000;
  }

  return MinBytes;
}

void WebRtcIsacfix_UpdateRateModel(RateModel *State,
                                   int16_t StreamSize,
                                   const int16_t FrameSamples,
                                   const int16_t BottleNeck)
{
  const int16_t TransmissionTime = (StreamSize * 8000) / BottleNeck;

  State->InitCounter = 0;

  State->StillBuffered += TransmissionTime;
  State->StillBuffered -= FrameSamples >> 4;
  if (State->StillBuffered < 0) {
    State->StillBuffered = 0;
  }

}

void WebRtcIsacfix_InitRateModel(RateModel *State)
{
  State->PrevExceed      = 0;
  State->ExceedAgo       = 0;
  State->BurstCounter    = 0;
  State->InitCounter     = INIT_BURST_LEN + 10;
  State->StillBuffered   = 1;
}

int16_t WebRtcIsacfix_GetNewFrameLength(int16_t bottle_neck, int16_t current_framesamples)
{
  int16_t new_framesamples;

  new_framesamples = current_framesamples;

  switch(current_framesamples) {
    case 480:
      if (bottle_neck < Thld_30_60) {
        new_framesamples = 960;
      }
      break;
    case 960:
      if (bottle_neck >= Thld_60_30) {
        new_framesamples = 480;
      }
      break;
    default:
      new_framesamples = -1;
  }

  return new_framesamples;
}

int16_t WebRtcIsacfix_GetSnr(int16_t bottle_neck, int16_t framesamples)
{
  int16_t s2nr = 0;

  switch(framesamples) {

    case 480:

      s2nr = -22500 + (int16_t)(500 * bottle_neck >> 10);
      break;
    case 960:

      s2nr = -22500 + (int16_t)(500 * bottle_neck >> 10);
      break;
    default:
      s2nr = -1;
  }

  return s2nr;

}
