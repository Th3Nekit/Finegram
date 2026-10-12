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
 * ilbc.h
 *
 * This header file contains all of the API's for iLBC.
 *
 */

#ifndef MODULES_AUDIO_CODING_CODECS_ILBC_ILBC_H_
#define MODULES_AUDIO_CODING_CODECS_ILBC_ILBC_H_

#include <stddef.h>
#include <stdint.h>

typedef struct iLBC_encinst_t_ IlbcEncoderInstance;

typedef struct iLBC_decinst_t_ IlbcDecoderInstance;

#define ILBC_SPEECH 1
#define ILBC_CNG 2

#ifdef __cplusplus
extern "C" {
#endif

int16_t WebRtcIlbcfix_EncoderAssign(IlbcEncoderInstance** iLBC_encinst,
                                    int16_t* ILBCENC_inst_Addr,
                                    int16_t* size);
int16_t WebRtcIlbcfix_DecoderAssign(IlbcDecoderInstance** iLBC_decinst,
                                    int16_t* ILBCDEC_inst_Addr,
                                    int16_t* size);

int16_t WebRtcIlbcfix_EncoderCreate(IlbcEncoderInstance** iLBC_encinst);
int16_t WebRtcIlbcfix_DecoderCreate(IlbcDecoderInstance** iLBC_decinst);

int16_t WebRtcIlbcfix_EncoderFree(IlbcEncoderInstance* iLBC_encinst);
int16_t WebRtcIlbcfix_DecoderFree(IlbcDecoderInstance* iLBC_decinst);

int16_t WebRtcIlbcfix_EncoderInit(IlbcEncoderInstance* iLBCenc_inst,
                                  int16_t frameLen);

int WebRtcIlbcfix_Encode(IlbcEncoderInstance* iLBCenc_inst,
                         const int16_t* speechIn,
                         size_t len,
                         uint8_t* encoded);

int16_t WebRtcIlbcfix_DecoderInit(IlbcDecoderInstance* iLBCdec_inst,
                                  int16_t frameLen);
void WebRtcIlbcfix_DecoderInit20Ms(IlbcDecoderInstance* iLBCdec_inst);
void WebRtcIlbcfix_Decoderinit30Ms(IlbcDecoderInstance* iLBCdec_inst);

int WebRtcIlbcfix_Decode(IlbcDecoderInstance* iLBCdec_inst,
                         const uint8_t* encoded,
                         size_t len,
                         int16_t* decoded,
                         int16_t* speechType);
int WebRtcIlbcfix_Decode20Ms(IlbcDecoderInstance* iLBCdec_inst,
                             const uint8_t* encoded,
                             size_t len,
                             int16_t* decoded,
                             int16_t* speechType);
int WebRtcIlbcfix_Decode30Ms(IlbcDecoderInstance* iLBCdec_inst,
                             const uint8_t* encoded,
                             size_t len,
                             int16_t* decoded,
                             int16_t* speechType);

size_t WebRtcIlbcfix_DecodePlc(IlbcDecoderInstance* iLBCdec_inst,
                               int16_t* decoded,
                               size_t noOfLostFrames);

size_t WebRtcIlbcfix_NetEqPlc(IlbcDecoderInstance* iLBCdec_inst,
                              int16_t* decoded,
                              size_t noOfLostFrames);

void WebRtcIlbcfix_version(char* version);

#ifdef __cplusplus
}
#endif

#endif
