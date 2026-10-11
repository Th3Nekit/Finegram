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
 * arith_routins.h
 *
 * Functions for arithmetic coding.
 *
 */

#ifndef MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_ARITH_ROUTINS_H_
#define MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_ARITH_ROUTINS_H_

#include "modules/audio_coding/codecs/isac/fix/source/structs.h"

int WebRtcIsacfix_EncLogisticMulti2(Bitstr_enc* streamData,
                                    int16_t* dataQ7,
                                    const uint16_t* env,
                                    int16_t lenData);

int16_t WebRtcIsacfix_EncTerminate(Bitstr_enc* streamData);

int WebRtcIsacfix_DecLogisticMulti2(int16_t* data,
                                    Bitstr_dec* streamData,
                                    const int32_t* env,
                                    int16_t lenData);

int WebRtcIsacfix_EncHistMulti(Bitstr_enc* streamData,
                               const int16_t* data,
                               const uint16_t* const* cdf,
                               int16_t lenData);

int16_t WebRtcIsacfix_DecHistBisectMulti(int16_t* data,
                                         Bitstr_dec* streamData,
                                         const uint16_t* const* cdf,
                                         const uint16_t* cdfSize,
                                         int16_t lenData);

int16_t WebRtcIsacfix_DecHistOneStepMulti(int16_t* data,
                                          Bitstr_dec* streamData,
                                          const uint16_t* const* cdf,
                                          const uint16_t* initIndex,
                                          int16_t lenData);

#endif
