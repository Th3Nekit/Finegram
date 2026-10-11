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
 * encode.c
 *
 * Encoding function for the iSAC coder.
 *
 */

#include "rtc_base/checks.h"
#include "modules/audio_coding/codecs/isac/fix/source/codec.h"

#include <stdio.h>

#include "modules/audio_coding/codecs/isac/fix/source/arith_routins.h"
#include "modules/audio_coding/codecs/isac/fix/source/bandwidth_estimator.h"
#include "modules/audio_coding/codecs/isac/fix/source/entropy_coding.h"
#include "modules/audio_coding/codecs/isac/fix/source/lpc_masking_model.h"
#include "modules/audio_coding/codecs/isac/fix/source/lpc_tables.h"
#include "modules/audio_coding/codecs/isac/fix/source/pitch_estimator.h"
#include "modules/audio_coding/codecs/isac/fix/source/pitch_gain_tables.h"
#include "modules/audio_coding/codecs/isac/fix/source/pitch_lag_tables.h"
#include "modules/audio_coding/codecs/isac/fix/source/structs.h"

int WebRtcIsacfix_EncodeImpl(int16_t      *in,
                             IsacFixEncoderInstance  *ISACenc_obj,
                             BwEstimatorstr      *bw_estimatordata,
                             int16_t         CodingMode)
{
  int16_t stream_length = 0;
  int16_t usefulstr_len = 0;
  int k;
  int16_t BWno;

  int16_t lofilt_coefQ15[(ORDERLO)*SUBFRAMES];
  int16_t hifilt_coefQ15[(ORDERHI)*SUBFRAMES];
  int32_t gain_lo_hiQ17[2*SUBFRAMES];

  int16_t LPandHP[FRAMESAMPLES/2 + QLOOKAHEAD];
  int16_t LP16a[FRAMESAMPLES/2 + QLOOKAHEAD];
  int16_t HP16a[FRAMESAMPLES/2 + QLOOKAHEAD];

  int16_t PitchLags_Q7[PITCH_SUBFRAMES];
  int16_t PitchGains_Q12[PITCH_SUBFRAMES];
  int16_t AvgPitchGain_Q12;

  int16_t frame_mode;
  int16_t processed_samples;
  int status;

  int32_t bits_gainsQ11;
  int16_t MinBytes;
  int16_t bmodel;

  transcode_obj transcodingParam;
  int16_t payloadLimitBytes;
  int16_t arithLenBeforeEncodingDFT;
  int16_t iterCntr;

  if (ISACenc_obj->buffer_index == 0) {

    ISACenc_obj->current_framesamples = ISACenc_obj->new_framelength;
  }

  frame_mode = ISACenc_obj->current_framesamples/MAX_FRAMESAMPLES;
  processed_samples = ISACenc_obj->current_framesamples/(frame_mode+1);

  for(k=0; k<FRAMESAMPLES_10ms; k++) {
    ISACenc_obj->data_buffer_fix[k + ISACenc_obj->buffer_index] = in[k];
  }

  if (ISACenc_obj->buffer_index + FRAMESAMPLES_10ms != processed_samples) {
    ISACenc_obj->buffer_index = ISACenc_obj->buffer_index + FRAMESAMPLES_10ms;
    return 0;
  }

  ISACenc_obj->buffer_index = 0;

  if (frame_mode == 0 || ISACenc_obj->frame_nb == 0 )
  {

    ISACenc_obj->bitstr_obj.W_upper = 0xFFFFFFFF;
    ISACenc_obj->bitstr_obj.streamval = 0;
    ISACenc_obj->bitstr_obj.stream_index = 0;
    ISACenc_obj->bitstr_obj.full = 1;

    if (CodingMode == 0) {
      ISACenc_obj->BottleNeck =  WebRtcIsacfix_GetUplinkBandwidth(bw_estimatordata);
      ISACenc_obj->MaxDelay =  WebRtcIsacfix_GetUplinkMaxDelay(bw_estimatordata);
    }
    if (CodingMode == 0 && frame_mode == 0 && (ISACenc_obj->enforceFrameSize == 0)) {
      ISACenc_obj->new_framelength = WebRtcIsacfix_GetNewFrameLength(ISACenc_obj->BottleNeck,
                                                                     ISACenc_obj->current_framesamples);
    }

    ISACenc_obj->s2nr = WebRtcIsacfix_GetSnr(
        (int16_t)(ISACenc_obj->BottleNeck * 901 >> 10),
        ISACenc_obj->current_framesamples);

    status = WebRtcIsacfix_EncodeFrameLen(ISACenc_obj->current_framesamples, &ISACenc_obj->bitstr_obj);
    if (status < 0)
    {

      if (frame_mode == 1 && ISACenc_obj->frame_nb == 1)
      {

        ISACenc_obj->frame_nb = 0;
      }
      return status;
    }

    if (ISACenc_obj->SaveEnc_ptr != NULL) {
      (ISACenc_obj->SaveEnc_ptr)->framelength=ISACenc_obj->current_framesamples;
    }

    BWno = WebRtcIsacfix_GetDownlinkBwIndexImpl(bw_estimatordata);
    status = WebRtcIsacfix_EncodeReceiveBandwidth(&BWno, &ISACenc_obj->bitstr_obj);
    if (status < 0)
    {
      if (frame_mode == 1 && ISACenc_obj->frame_nb == 1)
      {

        ISACenc_obj->frame_nb = 0;
      }
      return status;
    }
  }

  WebRtcIsacfix_SplitAndFilter1(ISACenc_obj->data_buffer_fix, LP16a, HP16a, &ISACenc_obj->prefiltbankstr_obj );

  WebRtcIsacfix_PitchAnalysis(LP16a+QLOOKAHEAD, LPandHP,
                              &ISACenc_obj->pitchanalysisstr_obj,  PitchLags_Q7, PitchGains_Q12);

  if (ISACenc_obj->SaveEnc_ptr != NULL) {
    if (frame_mode == 0 || ISACenc_obj->frame_nb == 0)
    {
      (ISACenc_obj->SaveEnc_ptr)->startIdx = 0;
    }
    else
    {
      (ISACenc_obj->SaveEnc_ptr)->startIdx = 1;
    }
  }

  status = WebRtcIsacfix_EncodePitchGain(PitchGains_Q12, &ISACenc_obj->bitstr_obj,  ISACenc_obj->SaveEnc_ptr);
  if (status < 0)
  {
    if (frame_mode == 1 && ISACenc_obj->frame_nb == 1)
    {

      ISACenc_obj->frame_nb = 0;
    }
    return status;
  }
  status = WebRtcIsacfix_EncodePitchLag(PitchLags_Q7 , PitchGains_Q12, &ISACenc_obj->bitstr_obj,  ISACenc_obj->SaveEnc_ptr);
  if (status < 0)
  {
    if (frame_mode == 1 && ISACenc_obj->frame_nb == 1)
    {

      ISACenc_obj->frame_nb = 0;
    }
    return status;
  }
  AvgPitchGain_Q12 = (PitchGains_Q12[0] + PitchGains_Q12[1] +
      PitchGains_Q12[2] + PitchGains_Q12[3]) >> 2;

  WebRtcIsacfix_GetLpcCoef(LPandHP, HP16a+QLOOKAHEAD, &ISACenc_obj->maskfiltstr_obj,
                           ISACenc_obj->s2nr, PitchGains_Q12,
                           gain_lo_hiQ17, lofilt_coefQ15, hifilt_coefQ15);

  for(k = 0; k < KLT_ORDER_GAIN; k++)
  {
    transcodingParam.lpcGains[k] = gain_lo_hiQ17[k];
  }

  status = WebRtcIsacfix_EncodeLpc(gain_lo_hiQ17, lofilt_coefQ15, hifilt_coefQ15,
                                   &bmodel, &bits_gainsQ11, &ISACenc_obj->bitstr_obj, ISACenc_obj->SaveEnc_ptr, &transcodingParam);
  if (status < 0)
  {
    if (frame_mode == 1 && ISACenc_obj->frame_nb == 1)
    {

      ISACenc_obj->frame_nb = 0;
    }
    return status;
  }
  arithLenBeforeEncodingDFT = (ISACenc_obj->bitstr_obj.stream_index << 1) + (1-ISACenc_obj->bitstr_obj.full);

  WebRtcIsacfix_NormLatticeFilterMa(ORDERLO, ISACenc_obj->maskfiltstr_obj.PreStateLoGQ15,
                                    LP16a, lofilt_coefQ15, gain_lo_hiQ17, 0, LPandHP);

  WebRtcIsacfix_PitchFilter(LPandHP, LP16a, &ISACenc_obj->pitchfiltstr_obj, PitchLags_Q7, PitchGains_Q12, 1);

  WebRtcIsacfix_NormLatticeFilterMa(ORDERHI, ISACenc_obj->maskfiltstr_obj.PreStateHiGQ15,
                                    HP16a, hifilt_coefQ15, gain_lo_hiQ17, 1, LPandHP);

  WebRtcIsacfix_Time2Spec(LP16a, LPandHP, LP16a, LPandHP);

  if (ISACenc_obj->SaveEnc_ptr != NULL) {
    for (k = 0; k < FRAMESAMPLES_HALF; k++) {
      (ISACenc_obj->SaveEnc_ptr)->fre[k + (ISACenc_obj->SaveEnc_ptr)->startIdx*FRAMESAMPLES_HALF] = LP16a[k];
      (ISACenc_obj->SaveEnc_ptr)->fim[k + (ISACenc_obj->SaveEnc_ptr)->startIdx*FRAMESAMPLES_HALF] = LPandHP[k];
    }
    (ISACenc_obj->SaveEnc_ptr)->AvgPitchGain[(ISACenc_obj->SaveEnc_ptr)->startIdx] = AvgPitchGain_Q12;
  }

  status = WebRtcIsacfix_EncodeSpec(LP16a, LPandHP, &ISACenc_obj->bitstr_obj, AvgPitchGain_Q12);
  if((status <= -1) && (status != -ISAC_DISALLOWED_BITSTREAM_LENGTH))
  {
    if (frame_mode == 1 && ISACenc_obj->frame_nb == 1)
    {

      ISACenc_obj->frame_nb = 0;
    }
    return status;
  }

  if((frame_mode == 1) && (ISACenc_obj->frame_nb == 0))
  {

    payloadLimitBytes = ISACenc_obj->payloadLimitBytes60 >> 1;
  }
  else if (frame_mode == 0)
  {

    payloadLimitBytes = (ISACenc_obj->payloadLimitBytes30) - 3;
  }
  else
  {

    payloadLimitBytes = ISACenc_obj->payloadLimitBytes60 - 3;
  }

  iterCntr = 0;
  while((((ISACenc_obj->bitstr_obj.stream_index) << 1) > payloadLimitBytes) ||
        (status == -ISAC_DISALLOWED_BITSTREAM_LENGTH))
  {
    int16_t arithLenDFTByte;
    int16_t bytesLeftQ5;
    int16_t ratioQ5[8] = {0, 6, 9, 12, 16, 19, 22, 25};

    int16_t scaleQ14[8] = {0, 348, 828, 1408, 2015, 3195, 3500, 3500};
    int16_t idx;

    if(iterCntr >= MAX_PAYLOAD_LIMIT_ITERATION)
    {

      if((frame_mode == 1) && (ISACenc_obj->frame_nb == 0))
      {

        ISACenc_obj->frame_nb = 1;
        return 0;
      }
      else if((frame_mode == 1) && (ISACenc_obj->frame_nb == 1))
      {
        ISACenc_obj->frame_nb = 0;
      }

      if(status != -ISAC_DISALLOWED_BITSTREAM_LENGTH)
      {
        return -ISAC_PAYLOAD_LARGER_THAN_LIMIT;
      }
      else
      {
        return status;
      }
    }
    if(status != -ISAC_DISALLOWED_BITSTREAM_LENGTH)
    {
      arithLenDFTByte = (ISACenc_obj->bitstr_obj.stream_index << 1) + (1-ISACenc_obj->bitstr_obj.full) - arithLenBeforeEncodingDFT;
      bytesLeftQ5 = (payloadLimitBytes - arithLenBeforeEncodingDFT) << 5;

      idx = 4;
      idx += (bytesLeftQ5 >= ratioQ5[idx] * arithLenDFTByte) ? 2 : -2;
      idx += (bytesLeftQ5 >= ratioQ5[idx] * arithLenDFTByte) ? 1 : -1;
      idx += (bytesLeftQ5 >= ratioQ5[idx] * arithLenDFTByte) ? 0 : -1;
    }
    else
    {

      idx = 0;
    }

    for(k = 0; k < FRAMESAMPLES_HALF; k++)
    {
      LP16a[k] = (int16_t)(LP16a[k] * scaleQ14[idx] >> 14);
      LPandHP[k] = (int16_t)(LPandHP[k] * scaleQ14[idx] >> 14);
    }

    if (ISACenc_obj->SaveEnc_ptr != NULL)
    {
      for(k = 0; k < FRAMESAMPLES_HALF; k++)
      {
        (ISACenc_obj->SaveEnc_ptr)->fre[k + (ISACenc_obj->SaveEnc_ptr)->startIdx*FRAMESAMPLES_HALF] = LP16a[k];
        (ISACenc_obj->SaveEnc_ptr)->fim[k + (ISACenc_obj->SaveEnc_ptr)->startIdx*FRAMESAMPLES_HALF] = LPandHP[k];
      }
    }

    for(k = 0; k < KLT_ORDER_GAIN; k++)
    {
      gain_lo_hiQ17[k] = WEBRTC_SPL_MUL_16_32_RSFT14(scaleQ14[idx], transcodingParam.lpcGains[k]);
      transcodingParam.lpcGains[k] = gain_lo_hiQ17[k];
    }

    ISACenc_obj->bitstr_obj.full = transcodingParam.full;
    ISACenc_obj->bitstr_obj.stream_index = transcodingParam.stream_index;
    ISACenc_obj->bitstr_obj.streamval = transcodingParam.streamval;
    ISACenc_obj->bitstr_obj.W_upper = transcodingParam.W_upper;
    ISACenc_obj->bitstr_obj.stream[transcodingParam.stream_index-1] = transcodingParam.beforeLastWord;
    ISACenc_obj->bitstr_obj.stream[transcodingParam.stream_index] = transcodingParam.lastWord;

    WebRtcIsacfix_EstCodeLpcGain(gain_lo_hiQ17, &ISACenc_obj->bitstr_obj, ISACenc_obj->SaveEnc_ptr);
    arithLenBeforeEncodingDFT = (ISACenc_obj->bitstr_obj.stream_index << 1) + (1-ISACenc_obj->bitstr_obj.full);
    status = WebRtcIsacfix_EncodeSpec(LP16a, LPandHP, &ISACenc_obj->bitstr_obj, AvgPitchGain_Q12);
    if((status <= -1) && (status != -ISAC_DISALLOWED_BITSTREAM_LENGTH))
    {
      if (frame_mode == 1 && ISACenc_obj->frame_nb == 1)
      {

        ISACenc_obj->frame_nb = 0;
      }
      return status;
    }
    iterCntr++;
  }

  if (frame_mode == 1 && ISACenc_obj->frame_nb == 0)

  {
    ISACenc_obj->frame_nb = 1;
    return 0;
  }
  else if (frame_mode == 1 && ISACenc_obj->frame_nb == 1)
  {
    ISACenc_obj->frame_nb = 0;

    if (CodingMode == 0 && (ISACenc_obj->enforceFrameSize == 0)) {
      ISACenc_obj->new_framelength = WebRtcIsacfix_GetNewFrameLength(ISACenc_obj->BottleNeck,
                                                                     ISACenc_obj->current_framesamples);
    }
  }

  stream_length = WebRtcIsacfix_EncTerminate(&ISACenc_obj->bitstr_obj);

  if(CodingMode == 0)
  {

    MinBytes = WebRtcIsacfix_GetMinBytes(&ISACenc_obj->rate_data_obj, (int16_t) stream_length,
                                         ISACenc_obj->current_framesamples, ISACenc_obj->BottleNeck, ISACenc_obj->MaxDelay);

    usefulstr_len = stream_length;

    if ((ISACenc_obj->frame_nb == 0) && (MinBytes > ISACenc_obj->payloadLimitBytes30)) {
      MinBytes = ISACenc_obj->payloadLimitBytes30;
    } else if ((ISACenc_obj->frame_nb == 1) && (MinBytes > ISACenc_obj->payloadLimitBytes60)) {
      MinBytes = ISACenc_obj->payloadLimitBytes60;
    }

    if( MinBytes > usefulstr_len + 255 ) {
      MinBytes = usefulstr_len + 255;
    }

    if (ISACenc_obj->SaveEnc_ptr != NULL) {
      (ISACenc_obj->SaveEnc_ptr)->minBytes = MinBytes;
    }

    while (stream_length < MinBytes)
    {
      RTC_DCHECK_GE(stream_length, 0);
      if (stream_length & 0x0001){
        ISACenc_obj->bitstr_seed = WEBRTC_SPL_RAND( ISACenc_obj->bitstr_seed );
        ISACenc_obj->bitstr_obj.stream[stream_length / 2] |=
            (uint16_t)(ISACenc_obj->bitstr_seed & 0xFF);
      } else {
        ISACenc_obj->bitstr_seed = WEBRTC_SPL_RAND( ISACenc_obj->bitstr_seed );
        ISACenc_obj->bitstr_obj.stream[stream_length / 2] =
            ((uint16_t)ISACenc_obj->bitstr_seed << 8);
      }
      stream_length++;
    }

    if (usefulstr_len & 0x0001) {
      ISACenc_obj->bitstr_obj.stream[usefulstr_len>>1] &= 0xFF00;
      ISACenc_obj->bitstr_obj.stream[usefulstr_len>>1] += (MinBytes - usefulstr_len) & 0x00FF;
    }
    else {
      ISACenc_obj->bitstr_obj.stream[usefulstr_len>>1] &= 0x00FF;
      ISACenc_obj->bitstr_obj.stream[usefulstr_len >> 1] +=
          ((uint16_t)((MinBytes - usefulstr_len) & 0x00FF) << 8);
    }
  }
  else
  {

    WebRtcIsacfix_UpdateRateModel(&ISACenc_obj->rate_data_obj, (int16_t) stream_length,
                                  ISACenc_obj->current_framesamples, ISACenc_obj->BottleNeck);
  }
  return stream_length;
}

int WebRtcIsacfix_EncodeStoredData(IsacFixEncoderInstance  *ISACenc_obj,
                                   int     BWnumber,
                                   float              scale)
{
  int ii;
  int status;
  int16_t BWno = (int16_t)BWnumber;
  int stream_length = 0;

  int16_t model;
  const uint16_t *Q_PitchGain_cdf_ptr[1];
  const uint16_t **cdf;
  const IsacSaveEncoderData *SaveEnc_str;
  int32_t tmpLPCcoeffs_g[KLT_ORDER_GAIN<<1];
  int16_t tmpLPCindex_g[KLT_ORDER_GAIN<<1];
  int16_t tmp_fre[FRAMESAMPLES];
  int16_t tmp_fim[FRAMESAMPLES];

  SaveEnc_str = ISACenc_obj->SaveEnc_ptr;

  if (SaveEnc_str == NULL) {
    return (-1);
  }

  if ((BWnumber < 0) || (BWnumber > 23)) {
    return -ISAC_RANGE_ERROR_BW_ESTIMATOR;
  }

  ISACenc_obj->bitstr_obj.W_upper = 0xFFFFFFFF;
  ISACenc_obj->bitstr_obj.streamval = 0;
  ISACenc_obj->bitstr_obj.stream_index = 0;
  ISACenc_obj->bitstr_obj.full = 1;

  status = WebRtcIsacfix_EncodeFrameLen(SaveEnc_str->framelength, &ISACenc_obj->bitstr_obj);
  if (status < 0) {

    return status;
  }

  status = WebRtcIsacfix_EncodeReceiveBandwidth(&BWno, &ISACenc_obj->bitstr_obj);
  if (status < 0) {
    return status;
  }

  if ((0.0 < scale) && (scale < 1.0)) {

    for (ii = 0; ii < (KLT_ORDER_GAIN*(1+SaveEnc_str->startIdx)); ii++) {
      tmpLPCcoeffs_g[ii] = (int32_t) ((scale) * (float) SaveEnc_str->LPCcoeffs_g[ii]);
    }

    for (ii = 0; ii < (FRAMESAMPLES_HALF*(1+SaveEnc_str->startIdx)); ii++) {
      tmp_fre[ii] = (int16_t) ((scale) * (float) SaveEnc_str->fre[ii]) ;
      tmp_fim[ii] = (int16_t) ((scale) * (float) SaveEnc_str->fim[ii]) ;
    }
  } else {
    for (ii = 0; ii < (KLT_ORDER_GAIN*(1+SaveEnc_str->startIdx)); ii++) {
      tmpLPCindex_g[ii] =  SaveEnc_str->LPCindex_g[ii];
    }

    for (ii = 0; ii < (FRAMESAMPLES_HALF*(1+SaveEnc_str->startIdx)); ii++) {
      tmp_fre[ii] = SaveEnc_str->fre[ii];
      tmp_fim[ii] = SaveEnc_str->fim[ii];
    }
  }

  for (ii = 0; ii <= SaveEnc_str->startIdx; ii++)
  {

    *Q_PitchGain_cdf_ptr = WebRtcIsacfix_kPitchGainCdf;
    status = WebRtcIsacfix_EncHistMulti(&ISACenc_obj->bitstr_obj, &SaveEnc_str->pitchGain_index[ii],
                                       Q_PitchGain_cdf_ptr, 1);
    if (status < 0) {
      return status;
    }

    if (SaveEnc_str->meanGain[ii] <= 819) {
      cdf = WebRtcIsacfix_kPitchLagPtrLo;
    } else if (SaveEnc_str->meanGain[ii] <= 1638) {
      cdf = WebRtcIsacfix_kPitchLagPtrMid;
    } else {
      cdf = WebRtcIsacfix_kPitchLagPtrHi;
    }
    status = WebRtcIsacfix_EncHistMulti(&ISACenc_obj->bitstr_obj,
                                       &SaveEnc_str->pitchIndex[PITCH_SUBFRAMES*ii], cdf, PITCH_SUBFRAMES);
    if (status < 0) {
      return status;
    }

    model = 0;
    status = WebRtcIsacfix_EncHistMulti(&ISACenc_obj->bitstr_obj,  &model,
                                       WebRtcIsacfix_kModelCdfPtr, 1);
    if (status < 0) {
      return status;
    }

    status = WebRtcIsacfix_EncHistMulti(&ISACenc_obj->bitstr_obj, &SaveEnc_str->LPCindex_s[KLT_ORDER_SHAPE*ii],
                                       WebRtcIsacfix_kCdfShapePtr[0], KLT_ORDER_SHAPE);
    if (status < 0) {
      return status;
    }

    if (scale < 1.0) {
      WebRtcIsacfix_TranscodeLpcCoef(&tmpLPCcoeffs_g[KLT_ORDER_GAIN*ii], &tmpLPCindex_g[KLT_ORDER_GAIN*ii]);
    }

    status = WebRtcIsacfix_EncHistMulti(&ISACenc_obj->bitstr_obj, &tmpLPCindex_g[KLT_ORDER_GAIN*ii],
                                       WebRtcIsacfix_kCdfGainPtr[0], KLT_ORDER_GAIN);
    if (status < 0) {
      return status;
    }

    status = WebRtcIsacfix_EncodeSpec(&tmp_fre[ii*FRAMESAMPLES_HALF], &tmp_fim[ii*FRAMESAMPLES_HALF],
                                      &ISACenc_obj->bitstr_obj, SaveEnc_str->AvgPitchGain[ii]);
    if (status < 0) {
      return status;
    }
  }

  stream_length = WebRtcIsacfix_EncTerminate(&ISACenc_obj->bitstr_obj);

  return stream_length;
}
