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
 * structs.h
 *
 * This header file contains all the structs used in the ISAC codec
 *
 */

#ifndef MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_STRUCTS_H_
#define MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_STRUCTS_H_

#include "modules/audio_coding/codecs/isac/bandwidth_info.h"
#include "modules/audio_coding/codecs/isac/main/source/settings.h"
#include "modules/third_party/fft/fft.h"

typedef struct Bitstreamstruct {
  uint8_t stream[STREAM_SIZE_MAX];
  uint32_t W_upper;
  uint32_t streamval;
  uint32_t stream_index;

} Bitstr;

typedef struct {
  double DataBufferLo[WINLEN];
  double DataBufferHi[WINLEN];

  double CorrBufLo[ORDERLO + 1];
  double CorrBufHi[ORDERHI + 1];

  float PreStateLoF[ORDERLO + 1];
  float PreStateLoG[ORDERLO + 1];
  float PreStateHiF[ORDERHI + 1];
  float PreStateHiG[ORDERHI + 1];
  float PostStateLoF[ORDERLO + 1];
  float PostStateLoG[ORDERLO + 1];
  float PostStateHiF[ORDERHI + 1];
  float PostStateHiG[ORDERHI + 1];

  double OldEnergy;

} MaskFiltstr;

typedef struct {

  double INSTAT1[2 * (QORDER - 1)];
  double INSTAT2[2 * (QORDER - 1)];
  double INSTATLA1[2 * (QORDER - 1)];
  double INSTATLA2[2 * (QORDER - 1)];
  double INLABUF1[QLOOKAHEAD];
  double INLABUF2[QLOOKAHEAD];

  float INSTAT1_float[2 * (QORDER - 1)];
  float INSTAT2_float[2 * (QORDER - 1)];
  float INSTATLA1_float[2 * (QORDER - 1)];
  float INSTATLA2_float[2 * (QORDER - 1)];
  float INLABUF1_float[QLOOKAHEAD];
  float INLABUF2_float[QLOOKAHEAD];

  double HPstates[HPORDER];
  float HPstates_float[HPORDER];

} PreFiltBankstr;

typedef struct {

  double STATE_0_LOWER[2 * POSTQORDER];
  double STATE_0_UPPER[2 * POSTQORDER];

  double HPstates1[HPORDER];
  double HPstates2[HPORDER];

  float STATE_0_LOWER_float[2 * POSTQORDER];
  float STATE_0_UPPER_float[2 * POSTQORDER];

  float HPstates1_float[HPORDER];
  float HPstates2_float[HPORDER];

} PostFiltBankstr;

typedef struct {

  double ubuf[PITCH_BUFFSIZE];

  double ystate[PITCH_DAMPORDER];

  double oldlagp[1];
  double oldgainp[1];

} PitchFiltstr;

typedef struct {

  double buffer[PITCH_WLPCBUFLEN];

  double istate[PITCH_WLPCORDER];
  double weostate[PITCH_WLPCORDER];
  double whostate[PITCH_WLPCORDER];

  double window[PITCH_WLPCWINLEN];

} WeightFiltstr;

typedef struct {

  double dec_buffer[PITCH_CORR_LEN2 + PITCH_CORR_STEP2 + PITCH_MAX_LAG / 2 -
                    PITCH_FRAME_LEN / 2 + 2];
  double decimator_state[2 * ALLPASSSECTIONS + 1];
  double hp_state[2];

  double whitened_buf[QLOOKAHEAD];

  double inbuf[QLOOKAHEAD];

  PitchFiltstr PFstr_wght;
  PitchFiltstr PFstr;
  WeightFiltstr Wghtstr;

} PitchAnalysisStruct;

typedef struct {

  int32_t prev_frame_length;

  int32_t prev_rec_rtp_number;

  uint32_t prev_rec_send_ts;

  uint32_t prev_rec_arr_ts;

  float prev_rec_rtp_rate;

  uint32_t last_update_ts;

  uint32_t last_reduction_ts;

  int32_t count_tot_updates_rec;

  int32_t rec_bw;
  float rec_bw_inv;
  float rec_bw_avg;
  float rec_bw_avg_Q;

  float rec_jitter;
  float rec_jitter_short_term;
  float rec_jitter_short_term_abs;
  float rec_max_delay;
  float rec_max_delay_avg_Q;

  float rec_header_rate;

  float send_bw_avg;

  float send_max_delay_avg;

  int num_pkts_rec;

  int num_consec_rec_pkts_over_30k;

  int hsn_detect_rec;

  int num_consec_snt_pkts_over_30k;

  int hsn_detect_snd;

  uint32_t start_wait_period;

  int in_wait_period;

  int change_to_WB;

  uint32_t senderTimestamp;
  uint32_t receiverTimestamp;

  uint16_t numConsecLatePkts;
  float consecLatency;
  int16_t inWaitLatePkts;

  IsacBandwidthInfo external_bw_info;
} BwEstimatorstr;

typedef struct {

  int PrevExceed;

  int ExceedAgo;

  int BurstCounter;

  int InitCounter;

  double StillBuffered;

} RateModel;

typedef struct {

  int startIdx;

  int16_t framelength;

  int pitchGain_index[2];

  double meanGain[2];
  int pitchIndex[PITCH_SUBFRAMES * 2];

  int LPCindex_s[108 * 2];
  int LPCindex_g[12 * 2];
  double LPCcoeffs_lo[(ORDERLO + 1) * SUBFRAMES * 2];
  double LPCcoeffs_hi[(ORDERHI + 1) * SUBFRAMES * 2];

  int16_t fre[FRAMESAMPLES];
  int16_t fim[FRAMESAMPLES];
  int16_t AvgPitchGain[2];

  int minBytes;

} IsacSaveEncoderData;

typedef struct {
  int indexLPCShape[UB_LPC_ORDER * UB16_LPC_VEC_PER_FRAME];
  double lpcGain[SUBFRAMES << 1];
  int lpcGainIndex[SUBFRAMES << 1];

  Bitstr bitStreamObj;

  int16_t realFFT[FRAMESAMPLES_HALF];
  int16_t imagFFT[FRAMESAMPLES_HALF];
} ISACUBSaveEncDataStruct;

typedef struct {
  Bitstr bitstr_obj;
  MaskFiltstr maskfiltstr_obj;
  PreFiltBankstr prefiltbankstr_obj;
  PitchFiltstr pitchfiltstr_obj;
  PitchAnalysisStruct pitchanalysisstr_obj;
  FFTstr fftstr_obj;
  IsacSaveEncoderData SaveEnc_obj;

  int buffer_index;
  int16_t current_framesamples;

  float data_buffer_float[FRAMESAMPLES_30ms];

  int frame_nb;
  double bottleneck;
  int16_t new_framelength;
  double s2nr;

  int16_t payloadLimitBytes30;

  int16_t payloadLimitBytes60;

  int16_t maxPayloadBytes;

  int16_t maxRateInBytes;

  int16_t enforceFrameSize;

  int16_t lastBWIdx;
} ISACLBEncStruct;

typedef struct {
  Bitstr bitstr_obj;
  MaskFiltstr maskfiltstr_obj;
  PreFiltBankstr prefiltbankstr_obj;
  FFTstr fftstr_obj;
  ISACUBSaveEncDataStruct SaveEnc_obj;

  int buffer_index;
  float data_buffer_float[MAX_FRAMESAMPLES + LB_TOTAL_DELAY_SAMPLES];
  double bottleneck;

  int16_t maxPayloadSizeBytes;

  double lastLPCVec[UB_LPC_ORDER];
  int16_t numBytesUsed;
  int16_t lastJitterInfo;
} ISACUBEncStruct;

typedef struct {
  Bitstr bitstr_obj;
  MaskFiltstr maskfiltstr_obj;
  PostFiltBankstr postfiltbankstr_obj;
  PitchFiltstr pitchfiltstr_obj;
  FFTstr fftstr_obj;

} ISACLBDecStruct;

typedef struct {
  Bitstr bitstr_obj;
  MaskFiltstr maskfiltstr_obj;
  PostFiltBankstr postfiltbankstr_obj;
  FFTstr fftstr_obj;

} ISACUBDecStruct;

typedef struct {
  ISACLBEncStruct ISACencLB_obj;
  ISACLBDecStruct ISACdecLB_obj;
} ISACLBStruct;

typedef struct {
  ISACUBEncStruct ISACencUB_obj;
  ISACUBDecStruct ISACdecUB_obj;
} ISACUBStruct;

typedef struct {

  double loFiltGain[SUBFRAMES];
  double hiFiltGain[SUBFRAMES];

  uint32_t W_upper;
  uint32_t streamval;

  uint32_t stream_index;
  uint8_t stream[3];
} transcode_obj;

typedef struct {

  double costab1[FRAMESAMPLES_HALF];
  double sintab1[FRAMESAMPLES_HALF];
  double costab2[FRAMESAMPLES_QUARTER];
  double sintab2[FRAMESAMPLES_QUARTER];
} TransformTables;

typedef struct {

  ISACLBStruct instLB;

  ISACUBStruct instUB;

  BwEstimatorstr bwestimator_obj;
  RateModel rate_data_obj;
  double MaxDelay;

  int16_t codingMode;

  int32_t bottleneck;

  int32_t analysisFBState1[FB_STATE_SIZE_WORD32];
  int32_t analysisFBState2[FB_STATE_SIZE_WORD32];
  int32_t synthesisFBState1[FB_STATE_SIZE_WORD32];
  int32_t synthesisFBState2[FB_STATE_SIZE_WORD32];

  int16_t errorCode;

  enum ISACBandwidth bandwidthKHz;

  enum IsacSamplingRate encoderSamplingRateKHz;
  enum IsacSamplingRate decoderSamplingRateKHz;

  int16_t initFlag;

  int16_t resetFlag_8kHz;

  int16_t maxRateBytesPer30Ms;

  int16_t maxPayloadSizeBytes;

  uint16_t in_sample_rate_hz;

  TransformTables transform_tables;
} ISACMainStruct;

#endif
