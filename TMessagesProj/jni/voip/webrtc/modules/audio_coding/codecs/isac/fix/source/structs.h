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
 * structs.h
 *
 * This header file contains all the structs used in the ISAC codec
 *
 */

#ifndef MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_STRUCTS_H_
#define MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_STRUCTS_H_

#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "modules/audio_coding/codecs/isac/bandwidth_info.h"
#include "modules/audio_coding/codecs/isac/fix/source/settings.h"

typedef struct Bitstreamstruct_dec {
  uint16_t stream[INTERNAL_STREAM_SIZE_W16];
  uint32_t W_upper;
  uint32_t streamval;
  uint16_t stream_index;
  int16_t full;

  size_t stream_size;
} Bitstr_dec;

typedef struct Bitstreamstruct_enc {
  uint16_t
      stream[STREAM_MAXW16_60MS];
  uint32_t W_upper;
  uint32_t streamval;
  uint16_t stream_index;
  int16_t full;

} Bitstr_enc;

typedef struct {
  int16_t DataBufferLoQ0[WINLEN];
  int16_t DataBufferHiQ0[WINLEN];

  int32_t CorrBufLoQQ[ORDERLO + 1];
  int32_t CorrBufHiQQ[ORDERHI + 1];

  int16_t CorrBufLoQdom[ORDERLO + 1];
  int16_t CorrBufHiQdom[ORDERHI + 1];

  int32_t PreStateLoGQ15[ORDERLO + 1];
  int32_t PreStateHiGQ15[ORDERHI + 1];

  uint32_t OldEnergy;

} MaskFiltstr_enc;

typedef struct {
  int16_t PostStateLoGQ0[ORDERLO + 1];
  int16_t PostStateHiGQ0[ORDERHI + 1];

  uint32_t OldEnergy;

} MaskFiltstr_dec;

typedef struct {

  int32_t INSTAT1_fix[2 * (QORDER - 1)];
  int32_t INSTAT2_fix[2 * (QORDER - 1)];
  int16_t INLABUF1_fix[QLOOKAHEAD];
  int16_t INLABUF2_fix[QLOOKAHEAD];

  int32_t HPstates_fix[HPORDER];

} PreFiltBankstr;

typedef struct {

  int32_t STATE_0_LOWER_fix[2 * POSTQORDER];
  int32_t STATE_0_UPPER_fix[2 * POSTQORDER];

  int32_t HPstates1_fix[HPORDER];
  int32_t HPstates2_fix[HPORDER];

} PostFiltBankstr;

typedef struct {

  int16_t ubufQQ[PITCH_BUFFSIZE];

  int16_t ystateQQ[PITCH_DAMPORDER];

  int16_t oldlagQ7;
  int16_t oldgainQ12;

} PitchFiltstr;

typedef struct {

  int16_t dec_buffer16[PITCH_CORR_LEN2 + PITCH_CORR_STEP2 + PITCH_MAX_LAG / 2 -
                       PITCH_FRAME_LEN / 2 + 2];
  int32_t decimator_state32[2 * ALLPASSSECTIONS + 1];
  int16_t inbuf[QLOOKAHEAD];

  PitchFiltstr PFstr_wght;
  PitchFiltstr PFstr;

} PitchAnalysisStruct;

typedef struct {

  int16_t prevPitchInvIn[FRAMESAMPLES / 2];
  int16_t prevPitchInvOut[PITCH_MAX_LAG + 10];
  int32_t prevHP[PITCH_MAX_LAG + 10];

  int16_t decayCoeffPriodic;
  int16_t decayCoeffNoise;
  int16_t used;

  int16_t* lastPitchLP;

  int16_t lofilt_coefQ15[ORDERLO];
  int16_t hifilt_coefQ15[ORDERHI];
  int32_t gain_lo_hiQ17[2];

  int16_t AvgPitchGain_Q12;
  int16_t lastPitchGain_Q12;
  int16_t lastPitchLag_Q7;

  int16_t overlapLP[RECOVERY_OVERLAP];

  int16_t pitchCycles;
  int16_t A;
  int16_t B;
  size_t pitchIndex;
  size_t stretchLag;
  int16_t* prevPitchLP;
  int16_t seed;

  int16_t std;
} PLCstr;

typedef struct {
  int16_t prevFrameSizeMs;
  uint16_t prevRtpNumber;

  uint32_t prevSendTime;
  uint32_t prevArrivalTime;

  uint16_t prevRtpRate;

  uint32_t lastUpdate;

  uint32_t lastReduction;
  int32_t countUpdates;

  uint32_t recBw;
  uint32_t recBwInv;
  uint32_t recBwAvg;
  uint32_t recBwAvgQ;

  uint32_t minBwInv;
  uint32_t maxBwInv;

  int32_t recJitter;
  int32_t recJitterShortTerm;
  int32_t recJitterShortTermAbs;
  int32_t recMaxDelay;
  int32_t recMaxDelayAvgQ;

  int16_t recHeaderRate;

  uint32_t sendBwAvg;

  int32_t sendMaxDelayAvg;

  int16_t countRecPkts;
  int16_t highSpeedRec;

  int16_t countHighSpeedRec;

  int16_t inWaitPeriod;

  uint32_t startWaitPeriod;

  int16_t countHighSpeedSent;

  int16_t highSpeedSend;

  IsacBandwidthInfo external_bw_info;
} BwEstimatorstr;

typedef struct {

  int16_t PrevExceed;

  int16_t ExceedAgo;

  int16_t BurstCounter;

  int16_t InitCounter;

  int16_t StillBuffered;

} RateModel;

typedef struct {

  int startIdx;

  int16_t framelength;

  int16_t pitchGain_index[2];

  int32_t meanGain[2];
  int16_t pitchIndex[PITCH_SUBFRAMES * 2];

  int32_t LPCcoeffs_g[12 * 2];
  int16_t LPCindex_s[108 * 2];
  int16_t LPCindex_g[12 * 2];

  int16_t fre[FRAMESAMPLES];
  int16_t fim[FRAMESAMPLES];
  int16_t AvgPitchGain[2];

  int minBytes;

} IsacSaveEncoderData;

typedef struct {
  Bitstr_enc bitstr_obj;
  MaskFiltstr_enc maskfiltstr_obj;
  PreFiltBankstr prefiltbankstr_obj;
  PitchFiltstr pitchfiltstr_obj;
  PitchAnalysisStruct pitchanalysisstr_obj;
  RateModel rate_data_obj;

  int16_t buffer_index;
  int16_t current_framesamples;

  int16_t data_buffer_fix[FRAMESAMPLES];

  int16_t frame_nb;
  int16_t BottleNeck;
  int16_t MaxDelay;
  int16_t new_framelength;
  int16_t s2nr;
  uint16_t MaxBits;

  int16_t bitstr_seed;

  IsacSaveEncoderData* SaveEnc_ptr;
  int16_t payloadLimitBytes30;

  int16_t payloadLimitBytes60;

  int16_t maxPayloadBytes;

  int16_t maxRateInBytes;
  int16_t enforceFrameSize;

} IsacFixEncoderInstance;

typedef struct {
  Bitstr_dec bitstr_obj;
  MaskFiltstr_dec maskfiltstr_obj;
  PostFiltBankstr postfiltbankstr_obj;
  PitchFiltstr pitchfiltstr_obj;
  PLCstr plcstr_obj;
} IsacFixDecoderInstance;

typedef struct {
  IsacFixEncoderInstance ISACenc_obj;
  IsacFixDecoderInstance ISACdec_obj;
  BwEstimatorstr bwestimator_obj;
  int16_t CodingMode;
  int16_t errorcode;
  int16_t initflag;

} ISACFIX_SubStruct;

typedef struct {
  int32_t lpcGains[12];

  uint32_t W_upper;
  uint32_t streamval;
  uint16_t stream_index;
  int16_t full;

  uint16_t beforeLastWord;
  uint16_t lastWord;
} transcode_obj;

#endif
