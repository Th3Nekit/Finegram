/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aecm/echo_control_mobile.h"

#ifdef AEC_DEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "common_audio/ring_buffer.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "modules/audio_processing/aecm/aecm_defines.h"
}
#include "modules/audio_processing/aecm/aecm_core.h"

namespace webrtc {

namespace {

#define BUF_SIZE_FRAMES 50  // buffer size (frames)

#define MAX_RESAMP_LEN (5 * FRAME_LEN)

static const size_t kBufSizeSamp =
    BUF_SIZE_FRAMES * FRAME_LEN;
static const int kSampMsNb = 8;

static const int kInitCheck = 42;

typedef struct {
  int sampFreq;
  int scSampFreq;
  short bufSizeStart;
  int knownDelay;

  short farendOld[2][FRAME_LEN];
  short initFlag;

  short counter;
  short sum;
  short firstVal;
  short checkBufSizeCtr;

  short msInSndCardBuf;
  short filtDelay;
  int timeForDelayChange;
  int ECstartup;
  int checkBuffSize;
  int delayChange;
  short lastDelayDiff;

  int16_t echoMode;

#ifdef AEC_DEBUG
  FILE* bufFile;
  FILE* delayFile;
  FILE* preCompFile;
  FILE* postCompFile;
#endif

  RingBuffer* farendBuf;

  AecmCore* aecmCore;
} AecMobile;

}

static int WebRtcAecm_EstBufDelay(AecMobile* aecm, short msInSndCardBuf);

static int WebRtcAecm_DelayComp(AecMobile* aecm);

void* WebRtcAecm_Create() {

  AecMobile* aecm = static_cast<AecMobile*>(calloc(1, sizeof(AecMobile)));

  aecm->aecmCore = WebRtcAecm_CreateCore();
  if (!aecm->aecmCore) {
    WebRtcAecm_Free(aecm);
    return NULL;
  }

  aecm->farendBuf = WebRtc_CreateBuffer(kBufSizeSamp, sizeof(int16_t));
  if (!aecm->farendBuf) {
    WebRtcAecm_Free(aecm);
    return NULL;
  }

#ifdef AEC_DEBUG
  aecm->aecmCore->farFile = fopen("aecFar.pcm", "wb");
  aecm->aecmCore->nearFile = fopen("aecNear.pcm", "wb");
  aecm->aecmCore->outFile = fopen("aecOut.pcm", "wb");

  aecm->bufFile = fopen("aecBuf.dat", "wb");
  aecm->delayFile = fopen("aecDelay.dat", "wb");
  aecm->preCompFile = fopen("preComp.pcm", "wb");
  aecm->postCompFile = fopen("postComp.pcm", "wb");
#endif
  return aecm;
}

void WebRtcAecm_Free(void* aecmInst) {
  AecMobile* aecm = static_cast<AecMobile*>(aecmInst);

  if (aecm == NULL) {
    return;
  }

#ifdef AEC_DEBUG
  fclose(aecm->aecmCore->farFile);
  fclose(aecm->aecmCore->nearFile);
  fclose(aecm->aecmCore->outFile);

  fclose(aecm->bufFile);
  fclose(aecm->delayFile);
  fclose(aecm->preCompFile);
  fclose(aecm->postCompFile);
#endif
  WebRtcAecm_FreeCore(aecm->aecmCore);
  WebRtc_FreeBuffer(aecm->farendBuf);
  free(aecm);
}

int32_t WebRtcAecm_Init(void* aecmInst, int32_t sampFreq) {
  AecMobile* aecm = static_cast<AecMobile*>(aecmInst);
  AecmConfig aecConfig;

  if (aecm == NULL) {
    return -1;
  }

  if (sampFreq != 8000 && sampFreq != 16000) {
    return AECM_BAD_PARAMETER_ERROR;
  }
  aecm->sampFreq = sampFreq;

  if (WebRtcAecm_InitCore(aecm->aecmCore, aecm->sampFreq) == -1) {
    return AECM_UNSPECIFIED_ERROR;
  }

  WebRtc_InitBuffer(aecm->farendBuf);

  aecm->initFlag = kInitCheck;

  aecm->delayChange = 1;

  aecm->sum = 0;
  aecm->counter = 0;
  aecm->checkBuffSize = 1;
  aecm->firstVal = 0;

  aecm->ECstartup = 1;
  aecm->bufSizeStart = 0;
  aecm->checkBufSizeCtr = 0;
  aecm->filtDelay = 0;
  aecm->timeForDelayChange = 0;
  aecm->knownDelay = 0;
  aecm->lastDelayDiff = 0;

  memset(&aecm->farendOld, 0, sizeof(aecm->farendOld));

  aecConfig.cngMode = AecmTrue;
  aecConfig.echoMode = 3;

  if (WebRtcAecm_set_config(aecm, aecConfig) == -1) {
    return AECM_UNSPECIFIED_ERROR;
  }

  return 0;
}

int32_t WebRtcAecm_GetBufferFarendError(void* aecmInst,
                                        const int16_t* farend,
                                        size_t nrOfSamples) {
  AecMobile* aecm = static_cast<AecMobile*>(aecmInst);

  if (aecm == NULL)
    return -1;

  if (farend == NULL)
    return AECM_NULL_POINTER_ERROR;

  if (aecm->initFlag != kInitCheck)
    return AECM_UNINITIALIZED_ERROR;

  if (nrOfSamples != 80 && nrOfSamples != 160)
    return AECM_BAD_PARAMETER_ERROR;

  return 0;
}

int32_t WebRtcAecm_BufferFarend(void* aecmInst,
                                const int16_t* farend,
                                size_t nrOfSamples) {
  AecMobile* aecm = static_cast<AecMobile*>(aecmInst);

  const int32_t err =
      WebRtcAecm_GetBufferFarendError(aecmInst, farend, nrOfSamples);

  if (err != 0)
    return err;

  if (!aecm->ECstartup) {
    WebRtcAecm_DelayComp(aecm);
  }

  WebRtc_WriteBuffer(aecm->farendBuf, farend, nrOfSamples);

  return 0;
}

int32_t WebRtcAecm_Process(void* aecmInst,
                           const int16_t* nearendNoisy,
                           const int16_t* nearendClean,
                           int16_t* out,
                           size_t nrOfSamples,
                           int16_t msInSndCardBuf) {
  AecMobile* aecm = static_cast<AecMobile*>(aecmInst);
  int32_t retVal = 0;
  size_t i;
  short nmbrOfFilledBuffers;
  size_t nBlocks10ms;
  size_t nFrames;
#ifdef AEC_DEBUG
  short msInAECBuf;
#endif

  if (aecm == NULL) {
    return -1;
  }

  if (nearendNoisy == NULL) {
    return AECM_NULL_POINTER_ERROR;
  }

  if (out == NULL) {
    return AECM_NULL_POINTER_ERROR;
  }

  if (aecm->initFlag != kInitCheck) {
    return AECM_UNINITIALIZED_ERROR;
  }

  if (nrOfSamples != 80 && nrOfSamples != 160) {
    return AECM_BAD_PARAMETER_ERROR;
  }

  if (msInSndCardBuf < 0) {
    msInSndCardBuf = 0;
    retVal = AECM_BAD_PARAMETER_WARNING;
  } else if (msInSndCardBuf > 500) {
    msInSndCardBuf = 500;
    retVal = AECM_BAD_PARAMETER_WARNING;
  }
  msInSndCardBuf += 10;
  aecm->msInSndCardBuf = msInSndCardBuf;

  nFrames = nrOfSamples / FRAME_LEN;
  nBlocks10ms = nFrames / aecm->aecmCore->mult;

  if (aecm->ECstartup) {
    if (nearendClean == NULL) {
      if (out != nearendNoisy) {
        memcpy(out, nearendNoisy, sizeof(short) * nrOfSamples);
      }
    } else if (out != nearendClean) {
      memcpy(out, nearendClean, sizeof(short) * nrOfSamples);
    }

    nmbrOfFilledBuffers =
        (short)WebRtc_available_read(aecm->farendBuf) / FRAME_LEN;

    if (aecm->checkBuffSize) {
      aecm->checkBufSizeCtr++;

      if (aecm->counter == 0) {
        aecm->firstVal = aecm->msInSndCardBuf;
        aecm->sum = 0;
      }

      if (abs(aecm->firstVal - aecm->msInSndCardBuf) <
          WEBRTC_SPL_MAX(0.2 * aecm->msInSndCardBuf, kSampMsNb)) {
        aecm->sum += aecm->msInSndCardBuf;
        aecm->counter++;
      } else {
        aecm->counter = 0;
      }

      if (aecm->counter * nBlocks10ms >= 6) {

        aecm->bufSizeStart = WEBRTC_SPL_MIN(
            (3 * aecm->sum * aecm->aecmCore->mult) / (aecm->counter * 40),
            BUF_SIZE_FRAMES);

        aecm->checkBuffSize = 0;
      }

      if (aecm->checkBufSizeCtr * nBlocks10ms > 50) {

        aecm->bufSizeStart = WEBRTC_SPL_MIN(
            (3 * aecm->msInSndCardBuf * aecm->aecmCore->mult) / 40,
            BUF_SIZE_FRAMES);
        aecm->checkBuffSize = 0;
      }
    }

    if (!aecm->checkBuffSize) {

      if (nmbrOfFilledBuffers == aecm->bufSizeStart) {
        aecm->ECstartup = 0;
      } else if (nmbrOfFilledBuffers > aecm->bufSizeStart) {
        WebRtc_MoveReadPtr(aecm->farendBuf,
                           (int)WebRtc_available_read(aecm->farendBuf) -
                               (int)aecm->bufSizeStart * FRAME_LEN);
        aecm->ECstartup = 0;
      }
    }

  } else {

    for (i = 0; i < nFrames; i++) {
      int16_t farend[FRAME_LEN];
      const int16_t* farend_ptr = NULL;

      nmbrOfFilledBuffers =
          (short)WebRtc_available_read(aecm->farendBuf) / FRAME_LEN;

      if (nmbrOfFilledBuffers > 0) {

        WebRtc_ReadBuffer(aecm->farendBuf, (void**)&farend_ptr, farend,
                          FRAME_LEN);

        memcpy(&(aecm->farendOld[i][0]), farend_ptr, FRAME_LEN * sizeof(short));
      } else {

        memcpy(farend, &(aecm->farendOld[i][0]), FRAME_LEN * sizeof(short));
        farend_ptr = farend;
      }

      if ((i == 0 && aecm->sampFreq == 8000) ||
          (i == 1 && aecm->sampFreq == 16000)) {
        WebRtcAecm_EstBufDelay(aecm, aecm->msInSndCardBuf);
      }

      if (WebRtcAecm_ProcessFrame(
              aecm->aecmCore, farend_ptr, &nearendNoisy[FRAME_LEN * i],
              (nearendClean ? &nearendClean[FRAME_LEN * i] : NULL),
              &out[FRAME_LEN * i]) == -1)
        return -1;
    }
  }

#ifdef AEC_DEBUG
  msInAECBuf = (short)WebRtc_available_read(aecm->farendBuf) /
               (kSampMsNb * aecm->aecmCore->mult);
  fwrite(&msInAECBuf, 2, 1, aecm->bufFile);
  fwrite(&(aecm->knownDelay), sizeof(aecm->knownDelay), 1, aecm->delayFile);
#endif

  return retVal;
}

int32_t WebRtcAecm_set_config(void* aecmInst, AecmConfig config) {
  AecMobile* aecm = static_cast<AecMobile*>(aecmInst);

  if (aecm == NULL) {
    return -1;
  }

  if (aecm->initFlag != kInitCheck) {
    return AECM_UNINITIALIZED_ERROR;
  }

  if (config.cngMode != AecmFalse && config.cngMode != AecmTrue) {
    return AECM_BAD_PARAMETER_ERROR;
  }
  aecm->aecmCore->cngMode = config.cngMode;

  if (config.echoMode < 0 || config.echoMode > 4) {
    return AECM_BAD_PARAMETER_ERROR;
  }
  aecm->echoMode = config.echoMode;

  if (aecm->echoMode == 0) {
    aecm->aecmCore->supGain = SUPGAIN_DEFAULT >> 3;
    aecm->aecmCore->supGainOld = SUPGAIN_DEFAULT >> 3;
    aecm->aecmCore->supGainErrParamA = SUPGAIN_ERROR_PARAM_A >> 3;
    aecm->aecmCore->supGainErrParamD = SUPGAIN_ERROR_PARAM_D >> 3;
    aecm->aecmCore->supGainErrParamDiffAB =
        (SUPGAIN_ERROR_PARAM_A >> 3) - (SUPGAIN_ERROR_PARAM_B >> 3);
    aecm->aecmCore->supGainErrParamDiffBD =
        (SUPGAIN_ERROR_PARAM_B >> 3) - (SUPGAIN_ERROR_PARAM_D >> 3);
  } else if (aecm->echoMode == 1) {
    aecm->aecmCore->supGain = SUPGAIN_DEFAULT >> 2;
    aecm->aecmCore->supGainOld = SUPGAIN_DEFAULT >> 2;
    aecm->aecmCore->supGainErrParamA = SUPGAIN_ERROR_PARAM_A >> 2;
    aecm->aecmCore->supGainErrParamD = SUPGAIN_ERROR_PARAM_D >> 2;
    aecm->aecmCore->supGainErrParamDiffAB =
        (SUPGAIN_ERROR_PARAM_A >> 2) - (SUPGAIN_ERROR_PARAM_B >> 2);
    aecm->aecmCore->supGainErrParamDiffBD =
        (SUPGAIN_ERROR_PARAM_B >> 2) - (SUPGAIN_ERROR_PARAM_D >> 2);
  } else if (aecm->echoMode == 2) {
    aecm->aecmCore->supGain = SUPGAIN_DEFAULT >> 1;
    aecm->aecmCore->supGainOld = SUPGAIN_DEFAULT >> 1;
    aecm->aecmCore->supGainErrParamA = SUPGAIN_ERROR_PARAM_A >> 1;
    aecm->aecmCore->supGainErrParamD = SUPGAIN_ERROR_PARAM_D >> 1;
    aecm->aecmCore->supGainErrParamDiffAB =
        (SUPGAIN_ERROR_PARAM_A >> 1) - (SUPGAIN_ERROR_PARAM_B >> 1);
    aecm->aecmCore->supGainErrParamDiffBD =
        (SUPGAIN_ERROR_PARAM_B >> 1) - (SUPGAIN_ERROR_PARAM_D >> 1);
  } else if (aecm->echoMode == 3) {
    aecm->aecmCore->supGain = SUPGAIN_DEFAULT;
    aecm->aecmCore->supGainOld = SUPGAIN_DEFAULT;
    aecm->aecmCore->supGainErrParamA = SUPGAIN_ERROR_PARAM_A;
    aecm->aecmCore->supGainErrParamD = SUPGAIN_ERROR_PARAM_D;
    aecm->aecmCore->supGainErrParamDiffAB =
        SUPGAIN_ERROR_PARAM_A - SUPGAIN_ERROR_PARAM_B;
    aecm->aecmCore->supGainErrParamDiffBD =
        SUPGAIN_ERROR_PARAM_B - SUPGAIN_ERROR_PARAM_D;
  } else if (aecm->echoMode == 4) {
    aecm->aecmCore->supGain = SUPGAIN_DEFAULT << 1;
    aecm->aecmCore->supGainOld = SUPGAIN_DEFAULT << 1;
    aecm->aecmCore->supGainErrParamA = SUPGAIN_ERROR_PARAM_A << 1;
    aecm->aecmCore->supGainErrParamD = SUPGAIN_ERROR_PARAM_D << 1;
    aecm->aecmCore->supGainErrParamDiffAB =
        (SUPGAIN_ERROR_PARAM_A << 1) - (SUPGAIN_ERROR_PARAM_B << 1);
    aecm->aecmCore->supGainErrParamDiffBD =
        (SUPGAIN_ERROR_PARAM_B << 1) - (SUPGAIN_ERROR_PARAM_D << 1);
  }

  return 0;
}

int32_t WebRtcAecm_InitEchoPath(void* aecmInst,
                                const void* echo_path,
                                size_t size_bytes) {
  AecMobile* aecm = static_cast<AecMobile*>(aecmInst);
  const int16_t* echo_path_ptr = static_cast<const int16_t*>(echo_path);

  if (aecmInst == NULL) {
    return -1;
  }
  if (echo_path == NULL) {
    return AECM_NULL_POINTER_ERROR;
  }
  if (size_bytes != WebRtcAecm_echo_path_size_bytes()) {

    return AECM_BAD_PARAMETER_ERROR;
  }
  if (aecm->initFlag != kInitCheck) {
    return AECM_UNINITIALIZED_ERROR;
  }

  WebRtcAecm_InitEchoPathCore(aecm->aecmCore, echo_path_ptr);

  return 0;
}

int32_t WebRtcAecm_GetEchoPath(void* aecmInst,
                               void* echo_path,
                               size_t size_bytes) {
  AecMobile* aecm = static_cast<AecMobile*>(aecmInst);
  int16_t* echo_path_ptr = static_cast<int16_t*>(echo_path);

  if (aecmInst == NULL) {
    return -1;
  }
  if (echo_path == NULL) {
    return AECM_NULL_POINTER_ERROR;
  }
  if (size_bytes != WebRtcAecm_echo_path_size_bytes()) {

    return AECM_BAD_PARAMETER_ERROR;
  }
  if (aecm->initFlag != kInitCheck) {
    return AECM_UNINITIALIZED_ERROR;
  }

  memcpy(echo_path_ptr, aecm->aecmCore->channelStored, size_bytes);
  return 0;
}

size_t WebRtcAecm_echo_path_size_bytes() {
  return (PART_LEN1 * sizeof(int16_t));
}

static int WebRtcAecm_EstBufDelay(AecMobile* aecm, short msInSndCardBuf) {
  short delayNew, nSampSndCard;
  short nSampFar = (short)WebRtc_available_read(aecm->farendBuf);
  short diff;

  nSampSndCard = msInSndCardBuf * kSampMsNb * aecm->aecmCore->mult;

  delayNew = nSampSndCard - nSampFar;

  if (delayNew < FRAME_LEN) {
    WebRtc_MoveReadPtr(aecm->farendBuf, FRAME_LEN);
    delayNew += FRAME_LEN;
  }

  aecm->filtDelay =
      WEBRTC_SPL_MAX(0, (8 * aecm->filtDelay + 2 * delayNew) / 10);

  diff = aecm->filtDelay - aecm->knownDelay;
  if (diff > 224) {
    if (aecm->lastDelayDiff < 96) {
      aecm->timeForDelayChange = 0;
    } else {
      aecm->timeForDelayChange++;
    }
  } else if (diff < 96 && aecm->knownDelay > 0) {
    if (aecm->lastDelayDiff > 224) {
      aecm->timeForDelayChange = 0;
    } else {
      aecm->timeForDelayChange++;
    }
  } else {
    aecm->timeForDelayChange = 0;
  }
  aecm->lastDelayDiff = diff;

  if (aecm->timeForDelayChange > 25) {
    aecm->knownDelay = WEBRTC_SPL_MAX((int)aecm->filtDelay - 160, 0);
  }
  return 0;
}

static int WebRtcAecm_DelayComp(AecMobile* aecm) {
  int nSampFar = (int)WebRtc_available_read(aecm->farendBuf);
  int nSampSndCard, delayNew, nSampAdd;
  const int maxStuffSamp = 10 * FRAME_LEN;

  nSampSndCard = aecm->msInSndCardBuf * kSampMsNb * aecm->aecmCore->mult;
  delayNew = nSampSndCard - nSampFar;

  if (delayNew > FAR_BUF_LEN - FRAME_LEN * aecm->aecmCore->mult) {

    nSampAdd =
        (int)(WEBRTC_SPL_MAX(((nSampSndCard >> 1) - nSampFar), FRAME_LEN));
    nSampAdd = WEBRTC_SPL_MIN(nSampAdd, maxStuffSamp);

    WebRtc_MoveReadPtr(aecm->farendBuf, -nSampAdd);
    aecm->delayChange = 1;
  }

  return 0;
}

}
