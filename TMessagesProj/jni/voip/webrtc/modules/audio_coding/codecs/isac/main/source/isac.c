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
 * isac.c
 *
 * This C file contains the functions for the ISAC API
 *
 */

#include "modules/audio_coding/codecs/isac/main/include/isac.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rtc_base/checks.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "modules/audio_coding/codecs/isac/main/source/bandwidth_estimator.h"
#include "modules/audio_coding/codecs/isac/main/source/codec.h"
#include "modules/audio_coding/codecs/isac/main/source/crc.h"
#include "modules/audio_coding/codecs/isac/main/source/entropy_coding.h"
#include "modules/audio_coding/codecs/isac/main/source/lpc_shape_swb16_tables.h"
#include "modules/audio_coding/codecs/isac/main/source/os_specific_inline.h"
#include "modules/audio_coding/codecs/isac/main/source/structs.h"
#include "modules/audio_coding/codecs/isac/main/source/isac_vad.h"
#include "rtc_base/system/arch.h"

#define BIT_MASK_DEC_INIT 0x0001
#define BIT_MASK_ENC_INIT 0x0002

#define LEN_CHECK_SUM_WORD8     4
#define MAX_NUM_LAYERS         10

static void UpdatePayloadSizeLimit(ISACMainStruct* instISAC) {
  int16_t lim30MsPayloadBytes = WEBRTC_SPL_MIN(
                          (instISAC->maxPayloadSizeBytes),
                          (instISAC->maxRateBytesPer30Ms));
  int16_t lim60MsPayloadBytes = WEBRTC_SPL_MIN(
                          (instISAC->maxPayloadSizeBytes),
                          (instISAC->maxRateBytesPer30Ms << 1));

  if (instISAC->bandwidthKHz == isac8kHz) {

    instISAC->instLB.ISACencLB_obj.payloadLimitBytes60 =
      lim60MsPayloadBytes;
    instISAC->instLB.ISACencLB_obj.payloadLimitBytes30 =
      lim30MsPayloadBytes;
  } else {

    if (lim30MsPayloadBytes > 250) {

      instISAC->instLB.ISACencLB_obj.payloadLimitBytes30 =
        (lim30MsPayloadBytes << 2) / 5;
    } else if (lim30MsPayloadBytes > 200) {

      instISAC->instLB.ISACencLB_obj.payloadLimitBytes30 =
        (lim30MsPayloadBytes << 1) / 5 + 100;
    } else {

      instISAC->instLB.ISACencLB_obj.payloadLimitBytes30 =
        lim30MsPayloadBytes - 20;
    }
    instISAC->instUB.ISACencUB_obj.maxPayloadSizeBytes =
      lim30MsPayloadBytes;
  }
}

static void UpdateBottleneck(ISACMainStruct* instISAC) {

  if ((instISAC->codingMode == 0) &&
      (instISAC->instLB.ISACencLB_obj.buffer_index == 0) &&
      (instISAC->instLB.ISACencLB_obj.frame_nb == 0)) {
    int32_t bottleneck =
        WebRtcIsac_GetUplinkBandwidth(&instISAC->bwestimator_obj);

    if ((instISAC->bandwidthKHz == isac8kHz)
        && (bottleneck > 37000)
        && (bottleneck < 41000)) {
      bottleneck = 37000;
    }

    if ((instISAC->bandwidthKHz != isac16kHz) &&
        (bottleneck > 46000)) {
      bottleneck = 46000;
    }

    if (instISAC->encoderSamplingRateKHz == kIsacWideband) {

      instISAC->instLB.ISACencLB_obj.bottleneck =
        (bottleneck > 32000) ? 32000 : bottleneck;
      instISAC->bandwidthKHz = isac8kHz;
    } else {

      enum ISACBandwidth bandwidth;
      WebRtcIsac_RateAllocation(bottleneck,
                                &(instISAC->instLB.ISACencLB_obj.bottleneck),
                                &(instISAC->instUB.ISACencUB_obj.bottleneck),
                                &bandwidth);
      if (bandwidth != isac8kHz) {
        instISAC->instLB.ISACencLB_obj.new_framelength = 480;
      }
      if (bandwidth != instISAC->bandwidthKHz) {

        instISAC->bandwidthKHz = bandwidth;
        UpdatePayloadSizeLimit(instISAC);
        if (bandwidth == isac12kHz) {
          instISAC->instLB.ISACencLB_obj.buffer_index = 0;
        }

      }
    }
  }
}

static void GetSendBandwidthInfo(ISACMainStruct* instISAC,
                                 int16_t* bandwidthIndex,
                                 int16_t* jitterInfo) {
  if ((instISAC->instLB.ISACencLB_obj.buffer_index ==
      (FRAMESAMPLES_10ms << 1)) &&
      (instISAC->instLB.ISACencLB_obj.frame_nb == 0)) {

    WebRtcIsac_GetDownlinkBwJitIndexImpl(&(instISAC->bwestimator_obj),
                                         bandwidthIndex, jitterInfo,
                                         instISAC->decoderSamplingRateKHz);
  }
}

int16_t WebRtcIsac_Create(ISACStruct** ISAC_main_inst) {
  ISACMainStruct* instISAC;

  if (ISAC_main_inst != NULL) {
    instISAC = (ISACMainStruct*)malloc(sizeof(ISACMainStruct));
    *ISAC_main_inst = (ISACStruct*)instISAC;
    if (*ISAC_main_inst != NULL) {
      instISAC->errorCode = 0;
      instISAC->initFlag = 0;

      instISAC->bandwidthKHz = isac8kHz;
      instISAC->encoderSamplingRateKHz = kIsacWideband;
      instISAC->decoderSamplingRateKHz = kIsacWideband;
      instISAC->in_sample_rate_hz = 16000;

      WebRtcIsac_InitTransform(&instISAC->transform_tables);
      return 0;
    } else {
      return -1;
    }
  } else {
    return -1;
  }
}

int16_t WebRtcIsac_Free(ISACStruct* ISAC_main_inst) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;
  free(instISAC);
  return 0;
}

static int16_t EncoderInitLb(ISACLBStruct* instLB,
                             int16_t codingMode,
                             enum IsacSamplingRate sampRate) {
  int16_t statusInit = 0;
  int k;

  for (k = 0; k < STREAM_SIZE_MAX_60; k++) {
    instLB->ISACencLB_obj.bitstr_obj.stream[k] = 0;
  }

  if ((codingMode == 1) || (sampRate == kIsacSuperWideband)) {

    instLB->ISACencLB_obj.new_framelength = 480;
  } else {
    instLB->ISACencLB_obj.new_framelength = INITIAL_FRAMESAMPLES;
  }

  WebRtcIsac_InitMasking(&instLB->ISACencLB_obj.maskfiltstr_obj);
  WebRtcIsac_InitPreFilterbank(&instLB->ISACencLB_obj.prefiltbankstr_obj);
  WebRtcIsac_InitPitchFilter(&instLB->ISACencLB_obj.pitchfiltstr_obj);
  WebRtcIsac_InitPitchAnalysis(
    &instLB->ISACencLB_obj.pitchanalysisstr_obj);

  instLB->ISACencLB_obj.buffer_index = 0;
  instLB->ISACencLB_obj.frame_nb = 0;

  instLB->ISACencLB_obj.bottleneck = 32000;
  instLB->ISACencLB_obj.current_framesamples = 0;
  instLB->ISACencLB_obj.s2nr = 0;
  instLB->ISACencLB_obj.payloadLimitBytes30 = STREAM_SIZE_MAX_30;
  instLB->ISACencLB_obj.payloadLimitBytes60 = STREAM_SIZE_MAX_60;
  instLB->ISACencLB_obj.maxPayloadBytes = STREAM_SIZE_MAX_60;
  instLB->ISACencLB_obj.maxRateInBytes = STREAM_SIZE_MAX_30;
  instLB->ISACencLB_obj.enforceFrameSize = 0;

  instLB->ISACencLB_obj.lastBWIdx            = -1;
  return statusInit;
}

static int16_t EncoderInitUb(ISACUBStruct* instUB,
                             int16_t bandwidth) {
  int16_t statusInit = 0;
  int k;

  for (k = 0; k < STREAM_SIZE_MAX_60; k++) {
    instUB->ISACencUB_obj.bitstr_obj.stream[k] = 0;
  }

  WebRtcIsac_InitMasking(&instUB->ISACencUB_obj.maskfiltstr_obj);
  WebRtcIsac_InitPreFilterbank(&instUB->ISACencUB_obj.prefiltbankstr_obj);

  if (bandwidth == isac16kHz) {
    instUB->ISACencUB_obj.buffer_index = LB_TOTAL_DELAY_SAMPLES;
  } else {
    instUB->ISACencUB_obj.buffer_index = 0;
  }

  instUB->ISACencUB_obj.bottleneck = 32000;

  instUB->ISACencUB_obj.maxPayloadSizeBytes = STREAM_SIZE_MAX_30 << 1;

  instUB->ISACencUB_obj.numBytesUsed = 0;
  memset(instUB->ISACencUB_obj.data_buffer_float, 0,
         (MAX_FRAMESAMPLES + LB_TOTAL_DELAY_SAMPLES) * sizeof(float));

  memcpy(&(instUB->ISACencUB_obj.lastLPCVec),
         WebRtcIsac_kMeanLarUb16, sizeof(double) * UB_LPC_ORDER);

  return statusInit;
}

int16_t WebRtcIsac_EncoderInit(ISACStruct* ISAC_main_inst,
                               int16_t codingMode) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;
  int16_t status;

  if ((codingMode != 0) && (codingMode != 1)) {
    instISAC->errorCode = ISAC_DISALLOWED_CODING_MODE;
    return -1;
  }

  instISAC->bottleneck = MAX_ISAC_BW;

  if (instISAC->encoderSamplingRateKHz == kIsacWideband) {
    instISAC->bandwidthKHz = isac8kHz;
    instISAC->maxPayloadSizeBytes = STREAM_SIZE_MAX_60;
    instISAC->maxRateBytesPer30Ms = STREAM_SIZE_MAX_30;
  } else {
    instISAC->bandwidthKHz = isac16kHz;
    instISAC->maxPayloadSizeBytes = STREAM_SIZE_MAX;
    instISAC->maxRateBytesPer30Ms = STREAM_SIZE_MAX;
  }

  instISAC->codingMode = codingMode;

  WebRtcIsac_InitBandwidthEstimator(&instISAC->bwestimator_obj,
                                    instISAC->encoderSamplingRateKHz,
                                    instISAC->decoderSamplingRateKHz);

  WebRtcIsac_InitRateModel(&instISAC->rate_data_obj);

  instISAC->MaxDelay = 10.0;

  status = EncoderInitLb(&instISAC->instLB, codingMode,
                         instISAC->encoderSamplingRateKHz);
  if (status < 0) {
    instISAC->errorCode = -status;
    return -1;
  }

  if (instISAC->encoderSamplingRateKHz == kIsacSuperWideband) {

    memset(instISAC->analysisFBState1, 0,
           FB_STATE_SIZE_WORD32 * sizeof(int32_t));
    memset(instISAC->analysisFBState2, 0,
           FB_STATE_SIZE_WORD32 * sizeof(int32_t));

    status = EncoderInitUb(&(instISAC->instUB),
                           instISAC->bandwidthKHz);
    if (status < 0) {
      instISAC->errorCode = -status;
      return -1;
    }
  }

  instISAC->initFlag |= BIT_MASK_ENC_INIT;
  return 0;
}

int WebRtcIsac_Encode(ISACStruct* ISAC_main_inst,
                      const int16_t* speechIn,
                      uint8_t* encoded) {
  float inFrame[FRAMESAMPLES_10ms];
  int16_t speechInLB[FRAMESAMPLES_10ms];
  int16_t speechInUB[FRAMESAMPLES_10ms];
  int streamLenLB = 0;
  int streamLenUB = 0;
  int streamLen = 0;
  size_t k = 0;
  uint8_t garbageLen = 0;
  int32_t bottleneck = 0;
  int16_t bottleneckIdx = 0;
  int16_t jitterInfo = 0;

  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;
  ISACLBStruct* instLB = &(instISAC->instLB);
  ISACUBStruct* instUB = &(instISAC->instUB);

  if ((instISAC->initFlag & BIT_MASK_ENC_INIT) !=
      BIT_MASK_ENC_INIT) {
    instISAC->errorCode = ISAC_ENCODER_NOT_INITIATED;
    return -1;
  }

  if (instISAC->encoderSamplingRateKHz == kIsacSuperWideband) {
    WebRtcSpl_AnalysisQMF(speechIn, SWBFRAMESAMPLES_10ms, speechInLB,
                          speechInUB, instISAC->analysisFBState1,
                          instISAC->analysisFBState2);

    for (k = 0; k < FRAMESAMPLES_10ms; k++) {
      inFrame[k] = (float)speechInLB[k];
    }
  } else {
    for (k = 0; k < FRAMESAMPLES_10ms; k++) {
      inFrame[k] = (float) speechIn[k];
    }
  }

  inFrame[0] += (float)1.23455334e-3;
  inFrame[1] -= (float)2.04324239e-3;
  inFrame[2] += (float)1.90854954e-3;
  inFrame[9] += (float)1.84854878e-3;

  UpdateBottleneck(instISAC);

  GetSendBandwidthInfo(instISAC, &bottleneckIdx, &jitterInfo);

  streamLenLB = WebRtcIsac_EncodeLb(&instISAC->transform_tables,
                                    inFrame, &instLB->ISACencLB_obj,
                                    instISAC->codingMode, bottleneckIdx);
  if (streamLenLB < 0) {
    return -1;
  }

  if (instISAC->encoderSamplingRateKHz == kIsacSuperWideband) {
    instUB = &(instISAC->instUB);

    for (k = 0; k < FRAMESAMPLES_10ms; k++) {
      inFrame[k] = (float) speechInUB[k];
    }

    inFrame[0] += (float)1.23455334e-3;
    inFrame[1] -= (float)2.04324239e-3;
    inFrame[2] += (float)1.90854954e-3;
    inFrame[9] += (float)1.84854878e-3;

    instUB->ISACencUB_obj.numBytesUsed =
        (int16_t)(streamLenLB + 1 + LEN_CHECK_SUM_WORD8);

    switch (instISAC->bandwidthKHz) {
      case isac12kHz: {
        streamLenUB = WebRtcIsac_EncodeUb12(&instISAC->transform_tables,
                                            inFrame, &instUB->ISACencUB_obj,
                                            jitterInfo);
        break;
      }
      case isac16kHz: {
        streamLenUB = WebRtcIsac_EncodeUb16(&instISAC->transform_tables,
                                            inFrame, &instUB->ISACencUB_obj,
                                            jitterInfo);
        break;
      }
      case isac8kHz: {
        streamLenUB = 0;
        break;
      }
    }

    if ((streamLenUB < 0) && (streamLenUB != -ISAC_PAYLOAD_LARGER_THAN_LIMIT)) {

      return -1;
    }

    if (streamLenLB == 0) {
      return 0;
    }

    if ((streamLenUB > (255 - (LEN_CHECK_SUM_WORD8 + 1))) ||
        (streamLenUB == -ISAC_PAYLOAD_LARGER_THAN_LIMIT)) {

      streamLenUB = 0;
    }

    memcpy(encoded, instLB->ISACencLB_obj.bitstr_obj.stream, streamLenLB);
    streamLen = streamLenLB;
    if (streamLenUB > 0) {
      encoded[streamLenLB] = (uint8_t)(streamLenUB + 1 + LEN_CHECK_SUM_WORD8);
      memcpy(&encoded[streamLenLB + 1],
             instUB->ISACencUB_obj.bitstr_obj.stream,
             streamLenUB);
      streamLen += encoded[streamLenLB];
    } else {
      encoded[streamLenLB] = 0;
    }
  } else {
    if (streamLenLB == 0) {
      return 0;
    }
    memcpy(encoded, instLB->ISACencLB_obj.bitstr_obj.stream, streamLenLB);
    streamLenUB = 0;
    streamLen = streamLenLB;
  }

  bottleneck = WebRtcIsac_GetUplinkBandwidth(&instISAC->bwestimator_obj);
  if (instISAC->codingMode == 0) {
    int minBytes;
    int limit;
    uint8_t* ptrGarbage;

    instISAC->MaxDelay = (double)WebRtcIsac_GetUplinkMaxDelay(
                           &instISAC->bwestimator_obj);

    minBytes = WebRtcIsac_GetMinBytes(
        &(instISAC->rate_data_obj), streamLen,
        instISAC->instLB.ISACencLB_obj.current_framesamples, bottleneck,
        instISAC->MaxDelay, instISAC->bandwidthKHz);

    if (instISAC->bandwidthKHz == isac8kHz) {
      if (instLB->ISACencLB_obj.current_framesamples == FRAMESAMPLES) {
        limit = instLB->ISACencLB_obj.payloadLimitBytes30;
      } else {
        limit = instLB->ISACencLB_obj.payloadLimitBytes60;
      }
    } else {
      limit = instUB->ISACencUB_obj.maxPayloadSizeBytes;
    }
    minBytes = (minBytes > limit) ? limit : minBytes;

    if ((instISAC->bandwidthKHz == isac8kHz) ||
        (streamLenUB == 0)) {
      ptrGarbage = &encoded[streamLenLB];
      limit = streamLen + 255;
    } else {
      ptrGarbage = &encoded[streamLenLB + 1 + streamLenUB];
      limit = streamLen + (255 - encoded[streamLenLB]);
    }
    minBytes = (minBytes > limit) ? limit : minBytes;

    garbageLen = (minBytes > streamLen) ? (uint8_t)(minBytes - streamLen) : 0;

    if (garbageLen > 0) {

      memset(ptrGarbage, 0, garbageLen);

      if ((instISAC->bandwidthKHz == isac8kHz) ||
          (streamLenUB == 0)) {
        encoded[streamLenLB] = garbageLen;
      } else {
        encoded[streamLenLB] += garbageLen;

        encoded[streamLenLB + 1 + streamLenUB] = garbageLen;

      }
      streamLen += garbageLen;
    }
  } else {

    WebRtcIsac_UpdateRateModel(
        &instISAC->rate_data_obj, streamLen,
        instISAC->instLB.ISACencLB_obj.current_framesamples, bottleneck);
    garbageLen = 0;
  }

  if ((instISAC->bandwidthKHz != isac8kHz) && (streamLenUB > 0)) {
    uint32_t crc;

    WebRtcIsac_GetCrc((int16_t*)(&(encoded[streamLenLB + 1])),
                      streamLenUB + garbageLen, &crc);
#ifndef WEBRTC_ARCH_BIG_ENDIAN
    for (k = 0; k < LEN_CHECK_SUM_WORD8; k++) {
      encoded[streamLen - LEN_CHECK_SUM_WORD8 + k] =
          (uint8_t)(crc >> (24 - k * 8));
    }
#else
    memcpy(&encoded[streamLenLB + streamLenUB + 1], &crc, LEN_CHECK_SUM_WORD8);
#endif
  }
  return streamLen;
}

int16_t WebRtcIsac_GetNewBitStream(ISACStruct*  ISAC_main_inst,
                                   int16_t  bweIndex,
                                   int16_t  jitterInfo,
                                   int32_t  rate,
                                   uint8_t* encoded,
                                   int16_t  isRCU) {
  Bitstr iSACBitStreamInst;
  int16_t streamLenLB;
  int16_t streamLenUB;
  int16_t totalStreamLen;
  double gain2;
  double gain1;
  float scale;
  enum ISACBandwidth bandwidthKHz;
  double rateLB;
  double rateUB;
  int32_t currentBN;
  uint32_t crc;
#ifndef WEBRTC_ARCH_BIG_ENDIAN
  int16_t  k;
#endif
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;

  if ((instISAC->initFlag & BIT_MASK_ENC_INIT) !=
      BIT_MASK_ENC_INIT) {
    return -1;
  }

  WebRtcIsac_GetUplinkBw(ISAC_main_inst, &currentBN);
  if (rate > currentBN) {
    rate = currentBN;
  }

  if (WebRtcIsac_RateAllocation(rate, &rateLB, &rateUB, &bandwidthKHz) < 0) {
    return -1;
  }

  if ((bandwidthKHz == isac12kHz) &&
      (instISAC->bandwidthKHz == isac16kHz)) {
    return -1;
  }

  gain1 = WebRtcIsac_GetSnr(
      rateLB, instISAC->instLB.ISACencLB_obj.current_framesamples);

  gain2 = WebRtcIsac_GetSnr(
      instISAC->instLB.ISACencLB_obj.bottleneck,
      instISAC->instLB.ISACencLB_obj.current_framesamples);

  scale = (float)pow(10, (gain1 - gain2) / 20.0);

  scale = (isRCU) ? (scale * RCU_TRANSCODING_SCALE) : scale;

  streamLenLB = WebRtcIsac_EncodeStoredDataLb(
                  &instISAC->instLB.ISACencLB_obj.SaveEnc_obj,
                  &iSACBitStreamInst, bweIndex, scale);

  if (streamLenLB < 0) {
    return -1;
  }

  memcpy(encoded, iSACBitStreamInst.stream, streamLenLB);

  if (bandwidthKHz == isac8kHz) {
    return streamLenLB;
  }

  totalStreamLen = streamLenLB;

  gain1 = WebRtcIsac_GetSnr(rateUB, FRAMESAMPLES);

  gain2 = WebRtcIsac_GetSnr(instISAC->instUB.ISACencUB_obj.bottleneck,
                            FRAMESAMPLES);

  scale = (float)pow(10, (gain1 - gain2) / 20.0);

  scale = (isRCU)? (scale * RCU_TRANSCODING_SCALE_UB) : scale;

  streamLenUB = WebRtcIsac_EncodeStoredDataUb(
                  &(instISAC->instUB.ISACencUB_obj.SaveEnc_obj),
                  &iSACBitStreamInst, jitterInfo, scale,
                  instISAC->bandwidthKHz);

  if (streamLenUB < 0) {
    return -1;
  }

  if (streamLenUB + 1 + LEN_CHECK_SUM_WORD8 > 255) {
    return streamLenLB;
  }

  totalStreamLen = streamLenLB + streamLenUB + 1 + LEN_CHECK_SUM_WORD8;
  encoded[streamLenLB] = streamLenUB + 1 + LEN_CHECK_SUM_WORD8;

  memcpy(&encoded[streamLenLB + 1], iSACBitStreamInst.stream,
         streamLenUB);

  WebRtcIsac_GetCrc((int16_t*)(&(encoded[streamLenLB + 1])),
                    streamLenUB, &crc);
#ifndef WEBRTC_ARCH_BIG_ENDIAN
  for (k = 0; k < LEN_CHECK_SUM_WORD8; k++) {
    encoded[totalStreamLen - LEN_CHECK_SUM_WORD8 + k] =
      (uint8_t)((crc >> (24 - k * 8)) & 0xFF);
  }
#else
  memcpy(&encoded[streamLenLB + streamLenUB + 1], &crc,
         LEN_CHECK_SUM_WORD8);
#endif
  return totalStreamLen;
}

static void DecoderInitLb(ISACLBStruct* instISAC) {
  int i;

  for (i = 0; i < STREAM_SIZE_MAX_60; i++) {
    instISAC->ISACdecLB_obj.bitstr_obj.stream[i] = 0;
  }

  WebRtcIsac_InitMasking(&instISAC->ISACdecLB_obj.maskfiltstr_obj);
  WebRtcIsac_InitPostFilterbank(
    &instISAC->ISACdecLB_obj.postfiltbankstr_obj);
  WebRtcIsac_InitPitchFilter(&instISAC->ISACdecLB_obj.pitchfiltstr_obj);
}

static void DecoderInitUb(ISACUBStruct* instISAC) {
  int i;

  for (i = 0; i < STREAM_SIZE_MAX_60; i++) {
    instISAC->ISACdecUB_obj.bitstr_obj.stream[i] = 0;
  }

  WebRtcIsac_InitMasking(&instISAC->ISACdecUB_obj.maskfiltstr_obj);
  WebRtcIsac_InitPostFilterbank(
    &instISAC->ISACdecUB_obj.postfiltbankstr_obj);
}

void WebRtcIsac_DecoderInit(ISACStruct* ISAC_main_inst) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;

  DecoderInitLb(&instISAC->instLB);
  if (instISAC->decoderSamplingRateKHz == kIsacSuperWideband) {
    memset(instISAC->synthesisFBState1, 0,
           FB_STATE_SIZE_WORD32 * sizeof(int32_t));
    memset(instISAC->synthesisFBState2, 0,
           FB_STATE_SIZE_WORD32 * sizeof(int32_t));
    DecoderInitUb(&(instISAC->instUB));
  }
  if ((instISAC->initFlag & BIT_MASK_ENC_INIT) != BIT_MASK_ENC_INIT) {
    WebRtcIsac_InitBandwidthEstimator(&instISAC->bwestimator_obj,
                                      instISAC->encoderSamplingRateKHz,
                                      instISAC->decoderSamplingRateKHz);
  }
  instISAC->initFlag |= BIT_MASK_DEC_INIT;
  instISAC->resetFlag_8kHz = 0;
}

int16_t WebRtcIsac_UpdateBwEstimate(ISACStruct* ISAC_main_inst,
                                    const uint8_t* encoded,
                                    size_t packet_size,
                                    uint16_t rtp_seq_number,
                                    uint32_t send_ts,
                                    uint32_t arr_ts) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;
  Bitstr streamdata;
#ifndef WEBRTC_ARCH_BIG_ENDIAN
  int k;
#endif
  int16_t err;

  if ((instISAC->initFlag & BIT_MASK_DEC_INIT) != BIT_MASK_DEC_INIT) {
    instISAC->errorCode = ISAC_DECODER_NOT_INITIATED;
    return -1;
  }

  if (packet_size < 10) {

    instISAC->errorCode = ISAC_EMPTY_PACKET;
    return -1;
  }

  WebRtcIsac_ResetBitstream(&(streamdata));

#ifndef WEBRTC_ARCH_BIG_ENDIAN
  for (k = 0; k < 10; k++) {
    uint16_t ek = ((const uint16_t*)encoded)[k >> 1];
    streamdata.stream[k] = (uint8_t)((ek >> ((k & 1) << 3)) & 0xff);
  }
#else
  memcpy(streamdata.stream, encoded, 10);
#endif

  err = WebRtcIsac_EstimateBandwidth(&instISAC->bwestimator_obj, &streamdata,
                                     packet_size, rtp_seq_number, send_ts,
                                     arr_ts, instISAC->encoderSamplingRateKHz,
                                     instISAC->decoderSamplingRateKHz);
  if (err < 0) {

    instISAC->errorCode = -err;
    return -1;
  }
  return 0;
}

static int Decode(ISACStruct* ISAC_main_inst,
                  const uint8_t* encoded,
                  size_t lenEncodedBytes,
                  int16_t* decoded,
                  int16_t* speechType,
                  int16_t isRCUPayload) {

  int16_t numSamplesLB;
  int16_t numSamplesUB;
  int16_t speechIdx;
  float outFrame[MAX_FRAMESAMPLES];
  int16_t outFrameLB[MAX_FRAMESAMPLES];
  int16_t outFrameUB[MAX_FRAMESAMPLES];
  int numDecodedBytesLBint;
  size_t numDecodedBytesLB;
  int numDecodedBytesUB;
  size_t lenEncodedLBBytes;
  int16_t validChecksum = 1;
  int16_t k;
  uint16_t numLayer;
  size_t totSizeBytes;
  int16_t err;

  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;
  ISACUBDecStruct* decInstUB = &(instISAC->instUB.ISACdecUB_obj);
  ISACLBDecStruct* decInstLB = &(instISAC->instLB.ISACdecLB_obj);

  if ((instISAC->initFlag & BIT_MASK_DEC_INIT) !=
      BIT_MASK_DEC_INIT) {
    instISAC->errorCode = ISAC_DECODER_NOT_INITIATED;
    return -1;
  }

  if (lenEncodedBytes == 0) {

    instISAC->errorCode = ISAC_EMPTY_PACKET;
    return -1;
  }

  lenEncodedLBBytes = (lenEncodedBytes > STREAM_SIZE_MAX) ?
      STREAM_SIZE_MAX : lenEncodedBytes;

  memcpy(instISAC->instLB.ISACdecLB_obj.bitstr_obj.stream, encoded,
         lenEncodedLBBytes);

  numSamplesLB = 0;

  numDecodedBytesLBint = WebRtcIsac_DecodeLb(&instISAC->transform_tables,
                                             outFrame, decInstLB,
                                             &numSamplesLB, isRCUPayload);
  numDecodedBytesLB = (size_t)numDecodedBytesLBint;
  if ((numDecodedBytesLBint < 0) ||
      (numDecodedBytesLB > lenEncodedLBBytes) ||
      (numSamplesLB > MAX_FRAMESAMPLES)) {
    instISAC->errorCode = ISAC_LENGTH_MISMATCH;
    return -1;
  }

  numLayer = 1;
  totSizeBytes = numDecodedBytesLB;
  while (totSizeBytes != lenEncodedBytes) {
    if ((totSizeBytes > lenEncodedBytes) ||
        (encoded[totSizeBytes] == 0) ||
        (numLayer > MAX_NUM_LAYERS)) {
      instISAC->errorCode = ISAC_LENGTH_MISMATCH;
      return -1;
    }
    totSizeBytes += encoded[totSizeBytes];
    numLayer++;
  }

  if (instISAC->decoderSamplingRateKHz == kIsacWideband) {
    for (k = 0; k < numSamplesLB; k++) {
      if (outFrame[k] > 32767) {
        decoded[k] = 32767;
      } else if (outFrame[k] < -32768) {
        decoded[k] = -32768;
      } else {
        decoded[k] = (int16_t)WebRtcIsac_lrint(outFrame[k]);
      }
    }
    numSamplesUB = 0;
  } else {
    uint32_t crc;

    for (k = 0; k < numSamplesLB; k++) {
      if (outFrame[k] > 32767) {
        outFrameLB[k] = 32767;
      } else if (outFrame[k] < -32768) {
        outFrameLB[k] = -32768;
      } else {
        outFrameLB[k] = (int16_t)WebRtcIsac_lrint(outFrame[k]);
      }
    }

    if (numDecodedBytesLB == lenEncodedBytes) {

      numSamplesUB = numSamplesLB;
      memset(outFrameUB, 0, sizeof(int16_t) *  numSamplesUB);

      instISAC->resetFlag_8kHz = 2;
    } else {

      int16_t lenNextStream = encoded[numDecodedBytesLB];

      if (lenNextStream <= (LEN_CHECK_SUM_WORD8 + 1)) {

        validChecksum = 0;
      } else {

        WebRtcIsac_GetCrc((int16_t*)(&encoded[numDecodedBytesLB + 1]),
                          lenNextStream - LEN_CHECK_SUM_WORD8 - 1, &crc);

        validChecksum = 1;
        for (k = 0; k < LEN_CHECK_SUM_WORD8; k++) {
          validChecksum &= (((crc >> (24 - k * 8)) & 0xFF) ==
                            encoded[numDecodedBytesLB + lenNextStream -
                                          LEN_CHECK_SUM_WORD8 + k]);
        }
      }

      if (!validChecksum) {

        numSamplesUB = numSamplesLB;
        memset(outFrameUB, 0, sizeof(int16_t) * numSamplesUB);
      } else {

        enum ISACBandwidth bandwidthKHz;
        int32_t maxDelayBit;

        if (numSamplesLB > FRAMESAMPLES) {
          instISAC->errorCode = ISAC_LENGTH_MISMATCH;
          return -1;
        }

        lenNextStream -= (LEN_CHECK_SUM_WORD8 + 1);

        memcpy(decInstUB->bitstr_obj.stream,
               &encoded[numDecodedBytesLB + 1], lenNextStream);

        WebRtcIsac_ResetBitstream(&(decInstUB->bitstr_obj));

        err = WebRtcIsac_DecodeJitterInfo(&decInstUB->bitstr_obj, &maxDelayBit);
        if (err < 0) {
          instISAC->errorCode = -err;
          return -1;
        }

        if (instISAC->encoderSamplingRateKHz == kIsacSuperWideband) {
          err = WebRtcIsac_UpdateUplinkJitter(
                  &(instISAC->bwestimator_obj), maxDelayBit);
          if (err < 0) {
            instISAC->errorCode = -err;
            return -1;
          }
        }

        err = WebRtcIsac_DecodeBandwidth(&decInstUB->bitstr_obj,
                                         &bandwidthKHz);
        if (err < 0) {
          instISAC->errorCode = -err;
          return -1;
        }

        switch (bandwidthKHz) {
          case isac12kHz: {
            numDecodedBytesUB = WebRtcIsac_DecodeUb12(
                &instISAC->transform_tables, outFrame, decInstUB, isRCUPayload);

            if (instISAC->resetFlag_8kHz > 0) {
              if (instISAC->resetFlag_8kHz == 2) {

                memset(outFrame, 0, MAX_FRAMESAMPLES *
                       sizeof(float));
              } else {
                const float rampStep = 2.0f / MAX_FRAMESAMPLES;
                float rampVal = 0;
                memset(outFrame, 0, (MAX_FRAMESAMPLES >> 1) *
                       sizeof(float));

                for (k = MAX_FRAMESAMPLES / 2; k < MAX_FRAMESAMPLES; k++) {
                  outFrame[k] *= rampVal;
                  rampVal += rampStep;
                }
              }
              instISAC->resetFlag_8kHz -= 1;
            }

            break;
          }
          case isac16kHz: {
            numDecodedBytesUB = WebRtcIsac_DecodeUb16(
                &instISAC->transform_tables, outFrame, decInstUB, isRCUPayload);
            break;
          }
          default:
            return -1;
        }

        if (numDecodedBytesUB < 0) {
          instISAC->errorCode = numDecodedBytesUB;
          return -1;
        }
        if (numDecodedBytesLB + numDecodedBytesUB > lenEncodedBytes) {

          instISAC->errorCode = ISAC_LENGTH_MISMATCH;
          return -1;
        }

        if ((numDecodedBytesUB != lenNextStream) &&
            (numDecodedBytesLB + 1 + numDecodedBytesUB >= lenEncodedBytes ||
             numDecodedBytesUB !=
                 (lenNextStream -
                  encoded[numDecodedBytesLB + 1 + numDecodedBytesUB]))) {
          instISAC->errorCode = ISAC_LENGTH_MISMATCH;
          return -1;
        }

        numSamplesUB = FRAMESAMPLES;

        for (k = 0; k < numSamplesUB; k++) {
          if (outFrame[k] > 32767) {
            outFrameUB[k] = 32767;
          } else if (outFrame[k] < -32768) {
            outFrameUB[k] = -32768;
          } else {
            outFrameUB[k] = (int16_t)WebRtcIsac_lrint(
                              outFrame[k]);
          }
        }
      }
    }

    speechIdx = 0;
    while (speechIdx < numSamplesLB) {
      WebRtcSpl_SynthesisQMF(&outFrameLB[speechIdx], &outFrameUB[speechIdx],
                             FRAMESAMPLES_10ms, &decoded[(speechIdx << 1)],
                             instISAC->synthesisFBState1,
                             instISAC->synthesisFBState2);

      speechIdx += FRAMESAMPLES_10ms;
    }
  }
  *speechType = 0;
  return (numSamplesLB + numSamplesUB);
}

int WebRtcIsac_Decode(ISACStruct* ISAC_main_inst,
                      const uint8_t* encoded,
                      size_t lenEncodedBytes,
                      int16_t* decoded,
                      int16_t* speechType) {
  int16_t isRCUPayload = 0;
  return Decode(ISAC_main_inst, encoded, lenEncodedBytes, decoded,
                speechType, isRCUPayload);
}

int WebRtcIsac_DecodeRcu(ISACStruct* ISAC_main_inst,
                         const uint8_t* encoded,
                         size_t lenEncodedBytes,
                         int16_t* decoded,
                         int16_t* speechType) {
  int16_t isRCUPayload = 1;
  return Decode(ISAC_main_inst, encoded, lenEncodedBytes, decoded,
                speechType, isRCUPayload);
}

size_t WebRtcIsac_DecodePlc(ISACStruct* ISAC_main_inst,
                            int16_t* decoded,
                            size_t noOfLostFrames) {
  size_t numSamples = 0;
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;

  if (noOfLostFrames > 2) {
    noOfLostFrames = 2;
  }

  switch (instISAC->decoderSamplingRateKHz) {
    case kIsacWideband: {
      numSamples = 480 * noOfLostFrames;
      break;
    }
    case kIsacSuperWideband: {
      numSamples = 960 * noOfLostFrames;
      break;
    }
  }

  memset(decoded, 0, numSamples * sizeof(int16_t));
  return numSamples;
}

static int16_t ControlLb(ISACLBStruct* instISAC, double rate,
                         int16_t frameSize) {
  if ((rate >= 10000) && (rate <= 32000)) {
    instISAC->ISACencLB_obj.bottleneck = rate;
  } else {
    return -ISAC_DISALLOWED_BOTTLENECK;
  }

  if ((frameSize == 30) || (frameSize == 60)) {
    instISAC->ISACencLB_obj.new_framelength = (FS / 1000) *  frameSize;
  } else {
    return -ISAC_DISALLOWED_FRAME_LENGTH;
  }

  return 0;
}

static int16_t ControlUb(ISACUBStruct* instISAC, double rate) {
  if ((rate >= 10000) && (rate <= 32000)) {
    instISAC->ISACencUB_obj.bottleneck = rate;
  } else {
    return -ISAC_DISALLOWED_BOTTLENECK;
  }
  return 0;
}

int16_t WebRtcIsac_Control(ISACStruct* ISAC_main_inst,
                           int32_t bottleneckBPS,
                           int frameSize) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;
  int16_t status;
  double rateLB;
  double rateUB;
  enum ISACBandwidth bandwidthKHz;

  if (instISAC->codingMode == 0) {

    instISAC->errorCode = ISAC_MODE_MISMATCH;
    return -1;
  }

  if ((instISAC->initFlag & BIT_MASK_ENC_INIT) !=
      BIT_MASK_ENC_INIT) {
    instISAC->errorCode = ISAC_ENCODER_NOT_INITIATED;
    return -1;
  }

  if (instISAC->encoderSamplingRateKHz == kIsacWideband) {

    bandwidthKHz = isac8kHz;
    rateLB = (bottleneckBPS > 32000) ? 32000 : bottleneckBPS;
    rateUB = 0;
  } else {
    if (WebRtcIsac_RateAllocation(bottleneckBPS, &rateLB, &rateUB,
                                  &bandwidthKHz) < 0) {
      return -1;
    }
  }

  if ((instISAC->encoderSamplingRateKHz == kIsacSuperWideband) &&
      (frameSize != 30) &&
      (bandwidthKHz != isac8kHz)) {

    instISAC->errorCode = ISAC_DISALLOWED_FRAME_LENGTH;
    return -1;
  }

  status = ControlLb(&instISAC->instLB, rateLB, (int16_t)frameSize);
  if (status < 0) {
    instISAC->errorCode = -status;
    return -1;
  }
  if (bandwidthKHz != isac8kHz) {
    status = ControlUb(&(instISAC->instUB), rateUB);
    if (status < 0) {
      instISAC->errorCode = -status;
      return -1;
    }
  }

  if ((instISAC->bandwidthKHz == isac8kHz) && (bandwidthKHz != isac8kHz)) {
    memset(instISAC->instUB.ISACencUB_obj.data_buffer_float, 0,
           sizeof(float) * (MAX_FRAMESAMPLES + LB_TOTAL_DELAY_SAMPLES));

    if (bandwidthKHz == isac12kHz) {
      instISAC->instUB.ISACencUB_obj.buffer_index =
        instISAC->instLB.ISACencLB_obj.buffer_index;
    } else {
      instISAC->instUB.ISACencUB_obj.buffer_index =
          LB_TOTAL_DELAY_SAMPLES + instISAC->instLB.ISACencLB_obj.buffer_index;

      memcpy(&(instISAC->instUB.ISACencUB_obj.lastLPCVec),
             WebRtcIsac_kMeanLarUb16, sizeof(double) * UB_LPC_ORDER);
    }
  }

  if (instISAC->bandwidthKHz != bandwidthKHz) {
    instISAC->bandwidthKHz = bandwidthKHz;
    UpdatePayloadSizeLimit(instISAC);
  }
  instISAC->bottleneck = bottleneckBPS;
  return 0;
}

void WebRtcIsac_SetInitialBweBottleneck(ISACStruct* ISAC_main_inst,
                                        int bottleneck_bits_per_second) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;
  RTC_DCHECK_GE(bottleneck_bits_per_second, 10000);
  RTC_DCHECK_LE(bottleneck_bits_per_second, 32000);
  instISAC->bwestimator_obj.send_bw_avg = (float)bottleneck_bits_per_second;
}

int16_t WebRtcIsac_ControlBwe(ISACStruct* ISAC_main_inst,
                              int32_t bottleneckBPS,
                              int frameSizeMs,
                              int16_t enforceFrameSize) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;
  enum ISACBandwidth bandwidth;

  if ((instISAC->initFlag & BIT_MASK_ENC_INIT) !=
      BIT_MASK_ENC_INIT) {
    instISAC->errorCode = ISAC_ENCODER_NOT_INITIATED;
    return -1;
  }

  if (instISAC->codingMode != 0) {
    instISAC->errorCode = ISAC_MODE_MISMATCH;
    return -1;
  }
  if ((frameSizeMs != 30) &&
      (instISAC->encoderSamplingRateKHz == kIsacSuperWideband)) {
    return -1;
  }

  if (enforceFrameSize != 0) {
    instISAC->instLB.ISACencLB_obj.enforceFrameSize = 1;
  } else {
    instISAC->instLB.ISACencLB_obj.enforceFrameSize = 0;
  }

  if (bottleneckBPS != 0) {
    double rateLB;
    double rateUB;
    if (WebRtcIsac_RateAllocation(bottleneckBPS, &rateLB, &rateUB,
                                  &bandwidth) < 0) {
      return -1;
    }
    instISAC->bwestimator_obj.send_bw_avg = (float)bottleneckBPS;
    instISAC->bandwidthKHz = bandwidth;
  }

  if (frameSizeMs != 0) {
    if ((frameSizeMs  == 30) || (frameSizeMs == 60)) {
      instISAC->instLB.ISACencLB_obj.new_framelength =
          (int16_t)((FS / 1000) * frameSizeMs);
    } else {
      instISAC->errorCode = ISAC_DISALLOWED_FRAME_LENGTH;
      return -1;
    }
  }
  return 0;
}

int16_t WebRtcIsac_GetDownLinkBwIndex(ISACStruct* ISAC_main_inst,
                                      int16_t* bweIndex,
                                      int16_t* jitterInfo) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;

  if ((instISAC->initFlag & BIT_MASK_DEC_INIT) !=
      BIT_MASK_DEC_INIT) {
    instISAC->errorCode = ISAC_ENCODER_NOT_INITIATED;
    return -1;
  }

  WebRtcIsac_GetDownlinkBwJitIndexImpl(&(instISAC->bwestimator_obj), bweIndex,
                                       jitterInfo,
                                       instISAC->decoderSamplingRateKHz);
  return 0;
}

int16_t WebRtcIsac_UpdateUplinkBw(ISACStruct* ISAC_main_inst,
                                  int16_t bweIndex) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;
  int16_t returnVal;

  if ((instISAC->initFlag & BIT_MASK_ENC_INIT) !=
      BIT_MASK_ENC_INIT) {
    instISAC->errorCode = ISAC_ENCODER_NOT_INITIATED;
    return -1;
  }

  returnVal = WebRtcIsac_UpdateUplinkBwImpl(
                &(instISAC->bwestimator_obj), bweIndex,
                instISAC->encoderSamplingRateKHz);

  if (returnVal < 0) {
    instISAC->errorCode = -returnVal;
    return -1;
  } else {
    return 0;
  }
}

int16_t WebRtcIsac_ReadBwIndex(const uint8_t* encoded,
                               int16_t* bweIndex) {
  Bitstr streamdata;
#ifndef WEBRTC_ARCH_BIG_ENDIAN
  int k;
#endif
  int16_t err;

  WebRtcIsac_ResetBitstream(&(streamdata));

#ifndef WEBRTC_ARCH_BIG_ENDIAN
  for (k = 0; k < 10; k++) {
    int16_t ek2 = ((const int16_t*)encoded)[k >> 1];
    streamdata.stream[k] = (uint8_t)((ek2 >> ((k & 1) << 3)) & 0xff);
  }
#else
  memcpy(streamdata.stream, encoded, 10);
#endif

  err = WebRtcIsac_DecodeFrameLen(&streamdata, bweIndex);
  if (err < 0) {
    return err;
  }

  err = WebRtcIsac_DecodeSendBW(&streamdata, bweIndex);
  if (err < 0) {
    return err;
  }

  return 0;
}

int16_t WebRtcIsac_ReadFrameLen(const ISACStruct* ISAC_main_inst,
                                const uint8_t* encoded,
                                int16_t* frameLength) {
  Bitstr streamdata;
#ifndef WEBRTC_ARCH_BIG_ENDIAN
  int k;
#endif
  int16_t err;
  ISACMainStruct* instISAC;

  WebRtcIsac_ResetBitstream(&(streamdata));

#ifndef WEBRTC_ARCH_BIG_ENDIAN
  for (k = 0; k < 10; k++) {
    int16_t ek2 = ((const int16_t*)encoded)[k >> 1];
    streamdata.stream[k] = (uint8_t)((ek2 >> ((k & 1) << 3)) & 0xff);
  }
#else
  memcpy(streamdata.stream, encoded, 10);
#endif

  err = WebRtcIsac_DecodeFrameLen(&streamdata, frameLength);
  if (err < 0) {
    return -1;
  }
  instISAC = (ISACMainStruct*)ISAC_main_inst;

  if (instISAC->decoderSamplingRateKHz == kIsacSuperWideband) {

    *frameLength <<= 1;
  }
  return 0;
}

int16_t WebRtcIsac_GetNewFrameLen(ISACStruct* ISAC_main_inst) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;

  if (instISAC->in_sample_rate_hz == 16000)
    return (instISAC->instLB.ISACencLB_obj.new_framelength);
  else
    return ((instISAC->instLB.ISACencLB_obj.new_framelength) * 2);
}

int16_t WebRtcIsac_GetErrorCode(ISACStruct* ISAC_main_inst) {
 return ((ISACMainStruct*)ISAC_main_inst)->errorCode;
}

int16_t WebRtcIsac_GetUplinkBw(ISACStruct*  ISAC_main_inst,
                               int32_t* bottleneck) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;

  if (instISAC->codingMode == 0) {

    *bottleneck = (int32_t)instISAC->bwestimator_obj.send_bw_avg;
  } else {
    *bottleneck = instISAC->bottleneck;
  }

  if ((*bottleneck > 32000) && (*bottleneck < 38000)) {
    *bottleneck = 32000;
  } else if ((*bottleneck > 45000) && (*bottleneck < 50000)) {
    *bottleneck = 45000;
  } else if (*bottleneck > 56000) {
    *bottleneck = 56000;
  }
  return 0;
}

int16_t WebRtcIsac_SetMaxPayloadSize(ISACStruct* ISAC_main_inst,
                                     int16_t maxPayloadBytes) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;
  int16_t status = 0;

  if ((instISAC->initFlag & BIT_MASK_ENC_INIT) !=
      BIT_MASK_ENC_INIT) {
    instISAC->errorCode = ISAC_ENCODER_NOT_INITIATED;
    return -1;
  }

  if (instISAC->encoderSamplingRateKHz == kIsacSuperWideband) {

    if (maxPayloadBytes < 120) {

      maxPayloadBytes = 120;
      status = -1;
    }

    if (maxPayloadBytes > STREAM_SIZE_MAX) {

      maxPayloadBytes = STREAM_SIZE_MAX;
      status = -1;
    }
  } else {
    if (maxPayloadBytes < 120) {

      maxPayloadBytes = 120;
      status = -1;
    }
    if (maxPayloadBytes > STREAM_SIZE_MAX_60) {

      maxPayloadBytes = STREAM_SIZE_MAX_60;
      status = -1;
    }
  }
  instISAC->maxPayloadSizeBytes = maxPayloadBytes;
  UpdatePayloadSizeLimit(instISAC);
  return status;
}

int16_t WebRtcIsac_SetMaxRate(ISACStruct* ISAC_main_inst,
                              int32_t maxRate) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;
  int16_t maxRateInBytesPer30Ms;
  int16_t status = 0;

  if ((instISAC->initFlag & BIT_MASK_ENC_INIT) != BIT_MASK_ENC_INIT) {
    instISAC->errorCode = ISAC_ENCODER_NOT_INITIATED;
    return -1;
  }

  maxRateInBytesPer30Ms = (int16_t)(maxRate * 3 / 800);

  if (instISAC->encoderSamplingRateKHz == kIsacWideband) {
    if (maxRate < 32000) {

      maxRateInBytesPer30Ms = 120;
      status = -1;
    }

    if (maxRate > 53400) {

      maxRateInBytesPer30Ms = 200;
      status = -1;
    }
  } else {
    if (maxRateInBytesPer30Ms < 120) {

      maxRateInBytesPer30Ms = 120;
      status = -1;
    }

    if (maxRateInBytesPer30Ms > STREAM_SIZE_MAX) {

      maxRateInBytesPer30Ms = STREAM_SIZE_MAX;
      status = -1;
    }
  }
  instISAC->maxRateBytesPer30Ms = maxRateInBytesPer30Ms;
  UpdatePayloadSizeLimit(instISAC);
  return status;
}

int16_t WebRtcIsac_GetRedPayload(ISACStruct* ISAC_main_inst,
                                 uint8_t* encoded) {
  Bitstr iSACBitStreamInst;
  int16_t streamLenLB;
  int16_t streamLenUB;
  int16_t streamLen;
  int16_t totalLenUB;
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;
#ifndef WEBRTC_ARCH_BIG_ENDIAN
  int k;
#endif

  if ((instISAC->initFlag & BIT_MASK_ENC_INIT) !=
      BIT_MASK_ENC_INIT) {
    instISAC->errorCode = ISAC_ENCODER_NOT_INITIATED;
  }

  WebRtcIsac_ResetBitstream(&(iSACBitStreamInst));

  streamLenLB = WebRtcIsac_EncodeStoredDataLb(
                  &instISAC->instLB.ISACencLB_obj.SaveEnc_obj,
                  &iSACBitStreamInst,
                  instISAC->instLB.ISACencLB_obj.lastBWIdx,
                  RCU_TRANSCODING_SCALE);
  if (streamLenLB < 0) {
    return -1;
  }

  memcpy(encoded, iSACBitStreamInst.stream, streamLenLB);
  streamLen = streamLenLB;
  if (instISAC->bandwidthKHz == isac8kHz) {
    return streamLenLB;
  }

  streamLenUB = WebRtcIsac_GetRedPayloadUb(
                  &instISAC->instUB.ISACencUB_obj.SaveEnc_obj,
                  &iSACBitStreamInst, instISAC->bandwidthKHz);
  if (streamLenUB < 0) {

    return -1;
  }

  totalLenUB = streamLenUB + 1 + LEN_CHECK_SUM_WORD8;
  if (totalLenUB > 255) {
    streamLenUB = 0;
  }

  if ((instISAC->bandwidthKHz != isac8kHz) &&
      (streamLenUB > 0)) {
    uint32_t crc;
    streamLen += totalLenUB;
    encoded[streamLenLB] = (uint8_t)totalLenUB;
    memcpy(&encoded[streamLenLB + 1], iSACBitStreamInst.stream,
           streamLenUB);

    WebRtcIsac_GetCrc((int16_t*)(&(encoded[streamLenLB + 1])),
                      streamLenUB, &crc);
#ifndef WEBRTC_ARCH_BIG_ENDIAN
    for (k = 0; k < LEN_CHECK_SUM_WORD8; k++) {
      encoded[streamLen - LEN_CHECK_SUM_WORD8 + k] =
        (uint8_t)((crc >> (24 - k * 8)) & 0xFF);
    }
#else
    memcpy(&encoded[streamLenLB + streamLenUB + 1], &crc,
           LEN_CHECK_SUM_WORD8);
#endif
  }
  return streamLen;
}

void WebRtcIsac_version(char* version) {
  strcpy(version, "4.3.0");
}

int16_t WebRtcIsac_SetEncSampRate(ISACStruct* ISAC_main_inst,
                                  uint16_t sample_rate_hz) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;
  enum IsacSamplingRate encoder_operational_rate;

  if ((sample_rate_hz != 16000) && (sample_rate_hz != 32000)) {

    instISAC->errorCode = ISAC_UNSUPPORTED_SAMPLING_FREQUENCY;
    return -1;
  }
  if (sample_rate_hz == 16000) {
    encoder_operational_rate = kIsacWideband;
  } else {
    encoder_operational_rate = kIsacSuperWideband;
  }

  if ((instISAC->initFlag & BIT_MASK_ENC_INIT) !=
      BIT_MASK_ENC_INIT) {
    if (encoder_operational_rate == kIsacWideband) {
      instISAC->bandwidthKHz = isac8kHz;
    } else {
      instISAC->bandwidthKHz = isac16kHz;
    }
  } else {
    ISACUBStruct* instUB = &(instISAC->instUB);
    ISACLBStruct* instLB = &(instISAC->instLB);
    int32_t bottleneck = instISAC->bottleneck;
    int16_t codingMode = instISAC->codingMode;
    int16_t frameSizeMs = instLB->ISACencLB_obj.new_framelength /
        (FS / 1000);

    if ((encoder_operational_rate == kIsacWideband) &&
        (instISAC->encoderSamplingRateKHz == kIsacSuperWideband)) {

      instISAC->bandwidthKHz = isac8kHz;
      if (codingMode == 1) {
        ControlLb(instLB,
                  (bottleneck > 32000) ? 32000 : bottleneck, FRAMESIZE);
      }
      instISAC->maxPayloadSizeBytes = STREAM_SIZE_MAX_60;
      instISAC->maxRateBytesPer30Ms = STREAM_SIZE_MAX_30;
    } else if ((encoder_operational_rate == kIsacSuperWideband) &&
               (instISAC->encoderSamplingRateKHz == kIsacWideband)) {
      double bottleneckLB = 0;
      double bottleneckUB = 0;
      if (codingMode == 1) {
        WebRtcIsac_RateAllocation(bottleneck, &bottleneckLB, &bottleneckUB,
                                  &(instISAC->bandwidthKHz));
      }

      instISAC->bandwidthKHz = isac16kHz;
      instISAC->maxPayloadSizeBytes = STREAM_SIZE_MAX;
      instISAC->maxRateBytesPer30Ms = STREAM_SIZE_MAX;

      EncoderInitLb(instLB, codingMode, encoder_operational_rate);
      EncoderInitUb(instUB, instISAC->bandwidthKHz);

      memset(instISAC->analysisFBState1, 0,
             FB_STATE_SIZE_WORD32 * sizeof(int32_t));
      memset(instISAC->analysisFBState2, 0,
             FB_STATE_SIZE_WORD32 * sizeof(int32_t));

      if (codingMode == 1) {
        instISAC->bottleneck = bottleneck;
        ControlLb(instLB, bottleneckLB,
                  (instISAC->bandwidthKHz == isac8kHz) ? frameSizeMs:FRAMESIZE);
        if (instISAC->bandwidthKHz > isac8kHz) {
          ControlUb(instUB, bottleneckUB);
        }
      } else {
        instLB->ISACencLB_obj.enforceFrameSize = 0;
        instLB->ISACencLB_obj.new_framelength = FRAMESAMPLES;
      }
    }
  }
  instISAC->encoderSamplingRateKHz = encoder_operational_rate;
  instISAC->in_sample_rate_hz = sample_rate_hz;
  return 0;
}

int16_t WebRtcIsac_SetDecSampRate(ISACStruct* ISAC_main_inst,
                                  uint16_t sample_rate_hz) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;
  enum IsacSamplingRate decoder_operational_rate;

  if (sample_rate_hz == 16000) {
    decoder_operational_rate = kIsacWideband;
  } else if (sample_rate_hz == 32000) {
    decoder_operational_rate = kIsacSuperWideband;
  } else {

    instISAC->errorCode = ISAC_UNSUPPORTED_SAMPLING_FREQUENCY;
    return -1;
  }

  if ((instISAC->decoderSamplingRateKHz == kIsacWideband) &&
        (decoder_operational_rate == kIsacSuperWideband)) {

      memset(instISAC->synthesisFBState1, 0,
             FB_STATE_SIZE_WORD32 * sizeof(int32_t));
      memset(instISAC->synthesisFBState2, 0,
             FB_STATE_SIZE_WORD32 * sizeof(int32_t));

      DecoderInitUb(&instISAC->instUB);
  }
  instISAC->decoderSamplingRateKHz = decoder_operational_rate;
  return 0;
}

uint16_t WebRtcIsac_EncSampRate(ISACStruct* ISAC_main_inst) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;
  return instISAC->in_sample_rate_hz;
}

uint16_t WebRtcIsac_DecSampRate(ISACStruct* ISAC_main_inst) {
  ISACMainStruct* instISAC = (ISACMainStruct*)ISAC_main_inst;
  return instISAC->decoderSamplingRateKHz == kIsacWideband ? 16000 : 32000;
}

void WebRtcIsac_SetEncSampRateInDecoder(ISACStruct* inst,
                                        int sample_rate_hz) {
  ISACMainStruct* instISAC = (ISACMainStruct*)inst;
  RTC_DCHECK_NE(0, instISAC->initFlag & BIT_MASK_DEC_INIT);
  RTC_DCHECK(!(instISAC->initFlag & BIT_MASK_ENC_INIT));
  RTC_DCHECK(sample_rate_hz == 16000 || sample_rate_hz == 32000);
  instISAC->encoderSamplingRateKHz = sample_rate_hz / 1000;
}
