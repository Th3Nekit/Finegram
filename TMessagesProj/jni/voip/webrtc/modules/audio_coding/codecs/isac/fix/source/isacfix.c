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
 * isacfix.c
 *
 * This C file contains the functions for the ISAC API
 *
 */

#include "modules/audio_coding/codecs/isac/fix/include/isacfix.h"

#include <stdlib.h>

#include "rtc_base/checks.h"
#include "modules/audio_coding/codecs/isac/fix/source/bandwidth_estimator.h"
#include "modules/audio_coding/codecs/isac/fix/source/codec.h"
#include "modules/audio_coding/codecs/isac/fix/source/entropy_coding.h"
#include "modules/audio_coding/codecs/isac/fix/source/filterbank_internal.h"
#include "modules/audio_coding/codecs/isac/fix/source/lpc_masking_model.h"
#include "modules/audio_coding/codecs/isac/fix/source/structs.h"

FilterMaLoopFix WebRtcIsacfix_FilterMaLoopFix;
Spec2Time WebRtcIsacfix_Spec2Time;
Time2Spec WebRtcIsacfix_Time2Spec;
MatrixProduct1 WebRtcIsacfix_MatrixProduct1;
MatrixProduct2 WebRtcIsacfix_MatrixProduct2;

static void InitializeDecoderBitstream(size_t stream_size_bytes,
                                       Bitstr_dec* bitstream) {
  bitstream->W_upper = 0xFFFFFFFF;
  bitstream->streamval = 0;
  bitstream->stream_index = 0;
  bitstream->full = 1;
  bitstream->stream_size = (stream_size_bytes + 1) >> 1;
  memset(bitstream->stream, 0, sizeof(bitstream->stream));
}

int16_t WebRtcIsacfix_Create(ISACFIX_MainStruct **ISAC_main_inst)
{
  ISACFIX_SubStruct *tempo;
  tempo = malloc(1 * sizeof(ISACFIX_SubStruct));
  *ISAC_main_inst = (ISACFIX_MainStruct *)tempo;
  if (*ISAC_main_inst!=NULL) {
    (*(ISACFIX_SubStruct**)ISAC_main_inst)->errorcode = 0;
    (*(ISACFIX_SubStruct**)ISAC_main_inst)->initflag = 0;
    (*(ISACFIX_SubStruct**)ISAC_main_inst)->ISACenc_obj.SaveEnc_ptr = NULL;
    WebRtcIsacfix_InitBandwidthEstimator(&tempo->bwestimator_obj);
    return(0);
  } else {
    return(-1);
  }
}

int16_t WebRtcIsacfix_CreateInternal(ISACFIX_MainStruct *ISAC_main_inst)
{
  ISACFIX_SubStruct *ISAC_inst;

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  ISAC_inst->ISACenc_obj.SaveEnc_ptr = malloc(1 * sizeof(IsacSaveEncoderData));

  if (ISAC_inst->ISACenc_obj.SaveEnc_ptr!=NULL) {
    return(0);
  } else {
    return(-1);
  }
}

int16_t WebRtcIsacfix_Free(ISACFIX_MainStruct *ISAC_main_inst)
{
  free(ISAC_main_inst);
  return(0);
}

int16_t WebRtcIsacfix_FreeInternal(ISACFIX_MainStruct *ISAC_main_inst)
{
  ISACFIX_SubStruct *ISAC_inst;

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  free(ISAC_inst->ISACenc_obj.SaveEnc_ptr);

  return(0);
}

#if defined(WEBRTC_HAS_NEON)
static void WebRtcIsacfix_InitNeon(void) {
  WebRtcIsacfix_AutocorrFix = WebRtcIsacfix_AutocorrNeon;
  WebRtcIsacfix_FilterMaLoopFix = WebRtcIsacfix_FilterMaLoopNeon;
  WebRtcIsacfix_Spec2Time = WebRtcIsacfix_Spec2TimeNeon;
  WebRtcIsacfix_Time2Spec = WebRtcIsacfix_Time2SpecNeon;
  WebRtcIsacfix_AllpassFilter2FixDec16 =
      WebRtcIsacfix_AllpassFilter2FixDec16Neon;
  WebRtcIsacfix_MatrixProduct1 = WebRtcIsacfix_MatrixProduct1Neon;
  WebRtcIsacfix_MatrixProduct2 = WebRtcIsacfix_MatrixProduct2Neon;
}
#endif

#if defined(MIPS32_LE)
static void WebRtcIsacfix_InitMIPS(void) {
  WebRtcIsacfix_AutocorrFix = WebRtcIsacfix_AutocorrMIPS;
  WebRtcIsacfix_FilterMaLoopFix = WebRtcIsacfix_FilterMaLoopMIPS;
  WebRtcIsacfix_Spec2Time = WebRtcIsacfix_Spec2TimeMIPS;
  WebRtcIsacfix_Time2Spec = WebRtcIsacfix_Time2SpecMIPS;
  WebRtcIsacfix_MatrixProduct1 = WebRtcIsacfix_MatrixProduct1MIPS;
  WebRtcIsacfix_MatrixProduct2 = WebRtcIsacfix_MatrixProduct2MIPS;
#if defined(MIPS_DSP_R1_LE)
  WebRtcIsacfix_AllpassFilter2FixDec16 =
      WebRtcIsacfix_AllpassFilter2FixDec16MIPS;
  WebRtcIsacfix_HighpassFilterFixDec32 =
      WebRtcIsacfix_HighpassFilterFixDec32MIPS;
#endif
#if defined(MIPS_DSP_R2_LE)
  WebRtcIsacfix_CalculateResidualEnergy =
      WebRtcIsacfix_CalculateResidualEnergyMIPS;
#endif
}
#endif

static void InitFunctionPointers(void) {
  WebRtcIsacfix_AutocorrFix = WebRtcIsacfix_AutocorrC;
  WebRtcIsacfix_FilterMaLoopFix = WebRtcIsacfix_FilterMaLoopC;
  WebRtcIsacfix_CalculateResidualEnergy =
      WebRtcIsacfix_CalculateResidualEnergyC;
  WebRtcIsacfix_AllpassFilter2FixDec16 = WebRtcIsacfix_AllpassFilter2FixDec16C;
  WebRtcIsacfix_HighpassFilterFixDec32 = WebRtcIsacfix_HighpassFilterFixDec32C;
  WebRtcIsacfix_Time2Spec = WebRtcIsacfix_Time2SpecC;
  WebRtcIsacfix_Spec2Time = WebRtcIsacfix_Spec2TimeC;
  WebRtcIsacfix_MatrixProduct1 = WebRtcIsacfix_MatrixProduct1C;
  WebRtcIsacfix_MatrixProduct2 = WebRtcIsacfix_MatrixProduct2C;

#if defined(WEBRTC_HAS_NEON)
  WebRtcIsacfix_InitNeon();
#endif

#if defined(MIPS32_LE)
  WebRtcIsacfix_InitMIPS();
#endif
}

int16_t WebRtcIsacfix_EncoderInit(ISACFIX_MainStruct *ISAC_main_inst,
                                  int16_t  CodingMode)
{
  int k;
  int16_t statusInit;
  ISACFIX_SubStruct *ISAC_inst;

  statusInit = 0;

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  ISAC_inst->initflag |= 2;

  if (CodingMode == 0)

    ISAC_inst->ISACenc_obj.new_framelength  = INITIAL_FRAMESAMPLES;
  else if (CodingMode == 1)

    ISAC_inst->ISACenc_obj.new_framelength = 480;
  else {
    ISAC_inst->errorcode = ISAC_DISALLOWED_CODING_MODE;
    statusInit = -1;
  }

  ISAC_inst->CodingMode = CodingMode;

  WebRtcIsacfix_InitMaskingEnc(&ISAC_inst->ISACenc_obj.maskfiltstr_obj);
  WebRtcIsacfix_InitPreFilterbank(&ISAC_inst->ISACenc_obj.prefiltbankstr_obj);
  WebRtcIsacfix_InitPitchFilter(&ISAC_inst->ISACenc_obj.pitchfiltstr_obj);
  WebRtcIsacfix_InitPitchAnalysis(&ISAC_inst->ISACenc_obj.pitchanalysisstr_obj);

  WebRtcIsacfix_InitRateModel(&ISAC_inst->ISACenc_obj.rate_data_obj);

  ISAC_inst->ISACenc_obj.buffer_index   = 0;
  ISAC_inst->ISACenc_obj.frame_nb    = 0;
  ISAC_inst->ISACenc_obj.BottleNeck      = 32000;
  ISAC_inst->ISACenc_obj.MaxDelay    = 10;
  ISAC_inst->ISACenc_obj.current_framesamples = 0;
  ISAC_inst->ISACenc_obj.s2nr     = 0;
  ISAC_inst->ISACenc_obj.MaxBits    = 0;
  ISAC_inst->ISACenc_obj.bitstr_seed   = 4447;
  ISAC_inst->ISACenc_obj.payloadLimitBytes30  = STREAM_MAXW16_30MS << 1;
  ISAC_inst->ISACenc_obj.payloadLimitBytes60  = STREAM_MAXW16_60MS << 1;
  ISAC_inst->ISACenc_obj.maxPayloadBytes      = STREAM_MAXW16_60MS << 1;
  ISAC_inst->ISACenc_obj.maxRateInBytes       = STREAM_MAXW16_30MS << 1;
  ISAC_inst->ISACenc_obj.enforceFrameSize     = 0;

  for (k=0; k<STREAM_MAXW16_60MS; k++){
    ISAC_inst->ISACenc_obj.bitstr_obj.stream[k] = 0;
  }

  InitFunctionPointers();

  return statusInit;
}

static void read_be16(const uint8_t* src, size_t nbytes, uint16_t* dest) {
  size_t i;
  for (i = 0; i < nbytes / 2; ++i)
    dest[i] = src[2 * i] << 8 | src[2 * i + 1];
  if (nbytes % 2 == 1)
    dest[nbytes / 2] = src[nbytes - 1] << 8;
}

static void write_be16(const uint16_t* src, size_t nbytes, uint8_t* dest) {
  size_t i;
  for (i = 0; i < nbytes / 2; ++i) {
    dest[2 * i] = src[i] >> 8;
    dest[2 * i + 1] = src[i];
  }
  if (nbytes % 2 == 1) {
    dest[nbytes - 1] = src[nbytes / 2] >> 8;
    dest[nbytes] = 0;
  }
}

int WebRtcIsacfix_Encode(ISACFIX_MainStruct *ISAC_main_inst,
                         const int16_t    *speechIn,
                         uint8_t* encoded)
{
  ISACFIX_SubStruct *ISAC_inst;
  int stream_len;

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  if ((ISAC_inst->initflag & 2) != 2) {
    ISAC_inst->errorcode = ISAC_ENCODER_NOT_INITIATED;
    return (-1);
  }

  stream_len = WebRtcIsacfix_EncodeImpl((int16_t*)speechIn,
                                        &ISAC_inst->ISACenc_obj,
                                        &ISAC_inst->bwestimator_obj,
                                        ISAC_inst->CodingMode);
  if (stream_len<0) {
    ISAC_inst->errorcode = -(int16_t)stream_len;
    return -1;
  }

  write_be16(ISAC_inst->ISACenc_obj.bitstr_obj.stream, (size_t)stream_len,
             encoded);
  return stream_len;

}

int16_t WebRtcIsacfix_GetNewBitStream(ISACFIX_MainStruct *ISAC_main_inst,
                                      int16_t      bweIndex,
                                      float              scale,
                                      uint8_t* encoded)
{
  ISACFIX_SubStruct *ISAC_inst;
  int16_t stream_len;

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  if ((ISAC_inst->initflag & 2) != 2) {
    ISAC_inst->errorcode = ISAC_ENCODER_NOT_INITIATED;
    return (-1);
  }

  stream_len = WebRtcIsacfix_EncodeStoredData(&ISAC_inst->ISACenc_obj,
                                              bweIndex,
                                              scale);
  if (stream_len<0) {
    ISAC_inst->errorcode = - stream_len;
    return -1;
  }

  write_be16(ISAC_inst->ISACenc_obj.bitstr_obj.stream, stream_len, encoded);
  return stream_len;
}

void WebRtcIsacfix_DecoderInit(ISACFIX_MainStruct *ISAC_main_inst)
{
  ISACFIX_SubStruct *ISAC_inst;

  InitFunctionPointers();

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  ISAC_inst->initflag |= 1;

  WebRtcIsacfix_InitMaskingDec(&ISAC_inst->ISACdec_obj.maskfiltstr_obj);
  WebRtcIsacfix_InitPostFilterbank(&ISAC_inst->ISACdec_obj.postfiltbankstr_obj);
  WebRtcIsacfix_InitPitchFilter(&ISAC_inst->ISACdec_obj.pitchfiltstr_obj);

  WebRtcIsacfix_InitPlc( &ISAC_inst->ISACdec_obj.plcstr_obj );
}

int16_t WebRtcIsacfix_UpdateBwEstimate1(ISACFIX_MainStruct *ISAC_main_inst,
                                        const uint8_t* encoded,
                                        size_t packet_size,
                                        uint16_t rtp_seq_number,
                                        uint32_t arr_ts)
{
  ISACFIX_SubStruct *ISAC_inst;
  Bitstr_dec streamdata;
  int16_t err;
  const size_t kRequiredEncodedLenBytes = 10;

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  if (packet_size == 0) {

    ISAC_inst->errorcode = ISAC_EMPTY_PACKET;
    return -1;
  } else if (packet_size > (STREAM_MAXW16<<1)) {

    ISAC_inst->errorcode = ISAC_LENGTH_MISMATCH;
    return -1;
  }

  if ((ISAC_inst->initflag & 1) != 1) {
    ISAC_inst->errorcode = ISAC_DECODER_NOT_INITIATED;
    return (-1);
  }

  InitializeDecoderBitstream(packet_size, &streamdata);

  read_be16(encoded, kRequiredEncodedLenBytes, streamdata.stream);

  err = WebRtcIsacfix_EstimateBandwidth(&ISAC_inst->bwestimator_obj,
                                        &streamdata,
                                        packet_size,
                                        rtp_seq_number,
                                        0,
                                        arr_ts);

  if (err < 0)
  {

    ISAC_inst->errorcode = -err;
    return -1;
  }

  return 0;
}

int16_t WebRtcIsacfix_UpdateBwEstimate(ISACFIX_MainStruct *ISAC_main_inst,
                                       const uint8_t* encoded,
                                       size_t packet_size,
                                       uint16_t rtp_seq_number,
                                       uint32_t send_ts,
                                       uint32_t arr_ts)
{
  ISACFIX_SubStruct *ISAC_inst;
  Bitstr_dec streamdata;
  int16_t err;
  const size_t kRequiredEncodedLenBytes = 10;

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  if (packet_size == 0) {

    ISAC_inst->errorcode = ISAC_EMPTY_PACKET;
    return -1;
  } else if (packet_size < kRequiredEncodedLenBytes) {
    ISAC_inst->errorcode = ISAC_PACKET_TOO_SHORT;
    return -1;
  } else if (packet_size > (STREAM_MAXW16<<1)) {

    ISAC_inst->errorcode = ISAC_LENGTH_MISMATCH;
    return -1;
  }

  if ((ISAC_inst->initflag & 1) != 1) {
    ISAC_inst->errorcode = ISAC_DECODER_NOT_INITIATED;
    return (-1);
  }

  InitializeDecoderBitstream(packet_size, &streamdata);

  read_be16(encoded, kRequiredEncodedLenBytes, streamdata.stream);

  err = WebRtcIsacfix_EstimateBandwidth(&ISAC_inst->bwestimator_obj,
                                        &streamdata,
                                        packet_size,
                                        rtp_seq_number,
                                        send_ts,
                                        arr_ts);

  if (err < 0)
  {

    ISAC_inst->errorcode = -err;
    return -1;
  }

  return 0;
}

int WebRtcIsacfix_Decode(ISACFIX_MainStruct* ISAC_main_inst,
                         const uint8_t* encoded,
                         size_t len,
                         int16_t* decoded,
                         int16_t* speechType)
{
  ISACFIX_SubStruct *ISAC_inst;

  size_t number_of_samples;
  int declen_int = 0;
  size_t declen;

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  if ((ISAC_inst->initflag & 1) != 1) {
    ISAC_inst->errorcode = ISAC_DECODER_NOT_INITIATED;
    return (-1);
  }

  if (len == 0) {

    ISAC_inst->errorcode = ISAC_EMPTY_PACKET;
    return -1;
  } else if (len > (STREAM_MAXW16<<1)) {

    ISAC_inst->errorcode = ISAC_LENGTH_MISMATCH;
    return -1;
  }

  InitializeDecoderBitstream(len, &ISAC_inst->ISACdec_obj.bitstr_obj);

  read_be16(encoded, len, ISAC_inst->ISACdec_obj.bitstr_obj.stream);

  *speechType=1;

  declen_int = WebRtcIsacfix_DecodeImpl(decoded, &ISAC_inst->ISACdec_obj,
                                        &number_of_samples);
  if (declen_int < 0) {

    ISAC_inst->errorcode = -(int16_t)declen_int;
    memset(decoded, 0, sizeof(int16_t) * MAX_FRAMESAMPLES);
    return -1;
  }
  declen = (size_t)declen_int;

  if (declen & 1) {
    if (len != declen &&
        len != declen +
            ((ISAC_inst->ISACdec_obj.bitstr_obj.stream[declen >> 1]) & 0xFF)) {
      ISAC_inst->errorcode = ISAC_LENGTH_MISMATCH;
      memset(decoded, 0, sizeof(int16_t) * number_of_samples);
      return -1;
    }
  } else {
    if (len != declen &&
        len != declen +
            ((ISAC_inst->ISACdec_obj.bitstr_obj.stream[declen >> 1]) >> 8)) {
      ISAC_inst->errorcode = ISAC_LENGTH_MISMATCH;
      memset(decoded, 0, sizeof(int16_t) * number_of_samples);
      return -1;
    }
  }

  return (int)number_of_samples;
}

size_t WebRtcIsacfix_DecodePlc(ISACFIX_MainStruct* ISAC_main_inst,
                               int16_t* decoded,
                               size_t noOfLostFrames)
{

  size_t no_of_samples, declen, k;
  int16_t outframe16[MAX_FRAMESAMPLES];

  ISACFIX_SubStruct *ISAC_inst;

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  if (noOfLostFrames > 2) {
    noOfLostFrames = 2;
  }
  k = 0;
  declen = 0;
  while( noOfLostFrames > 0 )
  {
    WebRtcIsacfix_DecodePlcImpl(&(outframe16[k*480]), &ISAC_inst->ISACdec_obj,
                                &no_of_samples);
    declen += no_of_samples;
    noOfLostFrames--;
    k++;
  }

  for (k=0;k<declen;k++) {
    decoded[k] = outframe16[k];
  }

  return declen;
}

int16_t WebRtcIsacfix_Control(ISACFIX_MainStruct *ISAC_main_inst,
                              int16_t rate,
                              int framesize)
{
  ISACFIX_SubStruct *ISAC_inst;

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  if (ISAC_inst->CodingMode == 0)
  {

    ISAC_inst->errorcode = ISAC_MODE_MISMATCH;
    return -1;
  }

  if (rate >= 10000 && rate <= 32000)
    ISAC_inst->ISACenc_obj.BottleNeck = rate;
  else {
    ISAC_inst->errorcode = ISAC_DISALLOWED_BOTTLENECK;
    return -1;
  }

  if (framesize  == 30 || framesize == 60)
    ISAC_inst->ISACenc_obj.new_framelength = (int16_t)((FS/1000) * framesize);
  else {
    ISAC_inst->errorcode = ISAC_DISALLOWED_FRAME_LENGTH;
    return -1;
  }

  return 0;
}

void WebRtcIsacfix_SetInitialBweBottleneck(ISACFIX_MainStruct* ISAC_main_inst,
                                           int bottleneck_bits_per_second) {
  ISACFIX_SubStruct* inst = (ISACFIX_SubStruct*)ISAC_main_inst;
  RTC_DCHECK_GE(bottleneck_bits_per_second, 10000);
  RTC_DCHECK_LE(bottleneck_bits_per_second, 32000);
  inst->bwestimator_obj.sendBwAvg = ((uint32_t)bottleneck_bits_per_second) << 7;
}

int16_t WebRtcIsacfix_ControlBwe(ISACFIX_MainStruct *ISAC_main_inst,
                                 int16_t rateBPS,
                                 int frameSizeMs,
                                 int16_t enforceFrameSize)
{
  ISACFIX_SubStruct *ISAC_inst;

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  if ((ISAC_inst->initflag & 2) != 2) {
    ISAC_inst->errorcode = ISAC_ENCODER_NOT_INITIATED;
    return (-1);
  }

  if (ISAC_inst->CodingMode != 0) {
    ISAC_inst->errorcode = ISAC_MODE_MISMATCH;
    return (-1);
  }

  ISAC_inst->ISACenc_obj.enforceFrameSize = (enforceFrameSize != 0)? 1:0;

  if ((rateBPS >= 10000) && (rateBPS <= 32000)) {
    ISAC_inst->bwestimator_obj.sendBwAvg = (((uint32_t)rateBPS) << 7);
  } else if (rateBPS != 0) {
    ISAC_inst->errorcode = ISAC_DISALLOWED_BOTTLENECK;
    return -1;
  }

  if ((frameSizeMs  == 30) || (frameSizeMs == 60)) {
    ISAC_inst->ISACenc_obj.new_framelength = (int16_t)((FS/1000) * frameSizeMs);
  } else {
    ISAC_inst->errorcode = ISAC_DISALLOWED_FRAME_LENGTH;
    return -1;
  }

  return 0;
}

int16_t WebRtcIsacfix_GetDownLinkBwIndex(ISACFIX_MainStruct* ISAC_main_inst,
                                         int16_t*     rateIndex)
{
  ISACFIX_SubStruct *ISAC_inst;

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  *rateIndex = WebRtcIsacfix_GetDownlinkBwIndexImpl(&ISAC_inst->bwestimator_obj);

  return 0;
}

int16_t WebRtcIsacfix_UpdateUplinkBw(ISACFIX_MainStruct* ISAC_main_inst,
                                     int16_t     rateIndex)
{
  int16_t err = 0;
  ISACFIX_SubStruct *ISAC_inst;

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  err = WebRtcIsacfix_UpdateUplinkBwRec(&ISAC_inst->bwestimator_obj, rateIndex);
  if (err < 0) {
    ISAC_inst->errorcode = -err;
    return (-1);
  }

  return 0;
}

int16_t WebRtcIsacfix_ReadFrameLen(const uint8_t* encoded,
                                   size_t encoded_len_bytes,
                                   size_t* frameLength)
{
  Bitstr_dec streamdata;
  int16_t err;
  const size_t kRequiredEncodedLenBytes = 10;

  if (encoded_len_bytes < kRequiredEncodedLenBytes) {
    return -1;
  }

  InitializeDecoderBitstream(encoded_len_bytes, &streamdata);

  read_be16(encoded, kRequiredEncodedLenBytes, streamdata.stream);

  err = WebRtcIsacfix_DecodeFrameLen(&streamdata, frameLength);
  if (err<0)
    return err;

  return 0;
}

int16_t WebRtcIsacfix_ReadBwIndex(const uint8_t* encoded,
                                  size_t encoded_len_bytes,
                                  int16_t* rateIndex)
{
  Bitstr_dec streamdata;
  int16_t err;
  const size_t kRequiredEncodedLenBytes = 10;

  if (encoded_len_bytes < kRequiredEncodedLenBytes) {
    return -1;
  }

  InitializeDecoderBitstream(encoded_len_bytes, &streamdata);

  read_be16(encoded, kRequiredEncodedLenBytes, streamdata.stream);

  size_t frameLength;
  err = WebRtcIsacfix_DecodeFrameLen(&streamdata, &frameLength);
  if (err<0)
    return err;

  err = WebRtcIsacfix_DecodeSendBandwidth(&streamdata, rateIndex);
  if (err<0)
    return err;

  return 0;
}

int16_t WebRtcIsacfix_GetErrorCode(ISACFIX_MainStruct *ISAC_main_inst)
{
  ISACFIX_SubStruct *ISAC_inst;

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  return ISAC_inst->errorcode;
}

int32_t WebRtcIsacfix_GetUplinkBw(ISACFIX_MainStruct *ISAC_main_inst)
{
  ISACFIX_SubStruct *ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;
  BwEstimatorstr * bw = (BwEstimatorstr*)&(ISAC_inst->bwestimator_obj);

  return (int32_t) WebRtcIsacfix_GetUplinkBandwidth(bw);
}

int16_t WebRtcIsacfix_GetNewFrameLen(ISACFIX_MainStruct *ISAC_main_inst)
{
  ISACFIX_SubStruct *ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;
  return ISAC_inst->ISACenc_obj.new_framelength;
}

int16_t WebRtcIsacfix_SetMaxPayloadSize(ISACFIX_MainStruct *ISAC_main_inst,
                                        int16_t maxPayloadBytes)
{
  ISACFIX_SubStruct *ISAC_inst;

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  if((maxPayloadBytes < 100) || (maxPayloadBytes > 400))
  {

    return -1;
  }
  else
  {

    ISAC_inst->ISACenc_obj.maxPayloadBytes = maxPayloadBytes;

    if (maxPayloadBytes < ISAC_inst->ISACenc_obj.maxRateInBytes) {
      ISAC_inst->ISACenc_obj.payloadLimitBytes30 = maxPayloadBytes;
    } else {
      ISAC_inst->ISACenc_obj.payloadLimitBytes30 = ISAC_inst->ISACenc_obj.maxRateInBytes;
    }

    if ( maxPayloadBytes < (ISAC_inst->ISACenc_obj.maxRateInBytes << 1)) {
      ISAC_inst->ISACenc_obj.payloadLimitBytes60 = maxPayloadBytes;
    } else {
      ISAC_inst->ISACenc_obj.payloadLimitBytes60 = (ISAC_inst->ISACenc_obj.maxRateInBytes << 1);
    }
  }
  return 0;
}

int16_t WebRtcIsacfix_SetMaxRate(ISACFIX_MainStruct *ISAC_main_inst,
                                 int32_t maxRate)
{
  ISACFIX_SubStruct *ISAC_inst;
  int16_t maxRateInBytes;

  ISAC_inst = (ISACFIX_SubStruct *)ISAC_main_inst;

  if((maxRate < 32000) || (maxRate > 53400))
  {

    return -1;
  }
  else
  {

    maxRateInBytes = (int16_t)( WebRtcSpl_DivW32W16ResW16(WEBRTC_SPL_MUL(maxRate, 3), 800) );

    ISAC_inst->ISACenc_obj.maxRateInBytes = maxRateInBytes;

    if (maxRateInBytes < ISAC_inst->ISACenc_obj.maxPayloadBytes) {
      ISAC_inst->ISACenc_obj.payloadLimitBytes30 = maxRateInBytes;
    } else {
      ISAC_inst->ISACenc_obj.payloadLimitBytes30 = ISAC_inst->ISACenc_obj.maxPayloadBytes;
    }

    if ( (maxRateInBytes << 1) < ISAC_inst->ISACenc_obj.maxPayloadBytes) {
      ISAC_inst->ISACenc_obj.payloadLimitBytes60 = (maxRateInBytes << 1);
    } else {
      ISAC_inst->ISACenc_obj.payloadLimitBytes60 = ISAC_inst->ISACenc_obj.maxPayloadBytes;
    }
  }

  return 0;
}

void WebRtcIsacfix_version(char *version)
{
  strcpy(version, "3.6.0");
}
