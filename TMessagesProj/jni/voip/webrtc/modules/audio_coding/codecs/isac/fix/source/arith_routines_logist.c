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
 * arith_routinslogist.c
 *
 * This C file contains arithmetic encode and decode logistic
 *
 */

#include "modules/audio_coding/codecs/isac/fix/source/arith_routins.h"

static const int32_t kHistEdges[51] = {
  -327680, -314573, -301466, -288359, -275252, -262144, -249037, -235930, -222823, -209716,
  -196608, -183501, -170394, -157287, -144180, -131072, -117965, -104858,  -91751,  -78644,
  -65536,  -52429,  -39322,  -26215,  -13108,       0,   13107,   26214,   39321,   52428,
  65536,   78643,   91750,  104857,  117964,  131072,  144179,  157286,  170393,  183500,
  196608,  209715,  222822,  235929,  249036,  262144,  275251,  288358,  301465,  314572,
  327680
};

static const uint16_t kCdfSlope[51] = {
  5,    5,     5,     5,     5,     5,     5,     5,    5,    5,
  5,    5,    13,    23,    47,    87,   154,   315,  700, 1088,
  2471, 6064, 14221, 21463, 36634, 36924, 19750, 13270, 5806, 2312,
  1095,  660,   316,   145,    86,    41,    32,     5,    5,    5,
  5,    5,     5,     5,     5,     5,     5,     5,    5,    2,
  0
};

static const uint16_t kCdfLogistic[51] = {
  0,     2,     4,     6,     8,    10,    12,    14,    16,    18,
  20,    22,    24,    29,    38,    57,    92,   153,   279,   559,
  994,  1983,  4408, 10097, 18682, 33336, 48105, 56005, 61313, 63636,
  64560, 64998, 65262, 65389, 65447, 65481, 65497, 65510, 65512, 65514,
  65516, 65518, 65520, 65522, 65524, 65526, 65528, 65530, 65532, 65534,
  65535
};

static __inline uint16_t WebRtcIsacfix_Piecewise(int32_t xinQ15) {
  int32_t ind;
  int32_t qtmp1;
  uint16_t qtmp2;

  qtmp1 = WEBRTC_SPL_SAT(kHistEdges[50],xinQ15,kHistEdges[0]);
  ind = WEBRTC_SPL_MUL(5, qtmp1 - kHistEdges[0]);
  ind >>= 16;

  qtmp1 = qtmp1 - kHistEdges[ind];
  qtmp2 = (uint16_t)WEBRTC_SPL_RSHIFT_U32(
      WEBRTC_SPL_UMUL_32_16(qtmp1,kCdfSlope[ind]), 15);
  return (kCdfLogistic[ind] + qtmp2);
}

int WebRtcIsacfix_EncLogisticMulti2(Bitstr_enc *streamData,
                                   int16_t *dataQ7,
                                   const uint16_t *envQ8,
                                   const int16_t lenData)
{
  uint32_t W_lower;
  uint32_t W_upper;
  uint16_t W_upper_LSB;
  uint16_t W_upper_MSB;
  uint16_t *streamPtr;
  uint16_t *maxStreamPtr;
  uint16_t *streamPtrCarry;
  uint16_t negcarry;
  uint32_t cdfLo;
  uint32_t cdfHi;
  int k;

  streamPtr = streamData->stream + streamData->stream_index;
  maxStreamPtr = streamData->stream + STREAM_MAXW16_60MS - 1;
  W_upper = streamData->W_upper;

  for (k = 0; k < lenData; k++)
  {

    cdfLo = WebRtcIsacfix_Piecewise(WEBRTC_SPL_MUL_16_U16(*dataQ7 - 64, *envQ8));
    cdfHi = WebRtcIsacfix_Piecewise(WEBRTC_SPL_MUL_16_U16(*dataQ7 + 64, *envQ8));

    while ((cdfLo + 1) >= cdfHi) {

      if (*dataQ7 > 0) {
        *dataQ7 -= 128;
        cdfHi = cdfLo;
        cdfLo = WebRtcIsacfix_Piecewise(
            WEBRTC_SPL_MUL_16_U16(*dataQ7 - 64, *envQ8));
      } else {
        *dataQ7 += 128;
        cdfLo = cdfHi;
        cdfHi = WebRtcIsacfix_Piecewise(
            WEBRTC_SPL_MUL_16_U16(*dataQ7 + 64, *envQ8));
      }
    }

    dataQ7++;

    envQ8 += (k & 1) & (k >> 1);

    W_upper_LSB = (uint16_t)W_upper;
    W_upper_MSB = (uint16_t)WEBRTC_SPL_RSHIFT_U32(W_upper, 16);
    W_lower = WEBRTC_SPL_UMUL_32_16(cdfLo, W_upper_MSB);
    W_lower += (cdfLo * W_upper_LSB) >> 16;
    W_upper = WEBRTC_SPL_UMUL_32_16(cdfHi, W_upper_MSB);
    W_upper += (cdfHi * W_upper_LSB) >> 16;

    W_upper -= ++W_lower;

    streamData->streamval += W_lower;

    if (streamData->streamval < W_lower)
    {

      streamPtrCarry = streamPtr;
      if (streamData->full == 0) {
        negcarry = *streamPtrCarry;
        negcarry += 0x0100;
        *streamPtrCarry = negcarry;
        while (!(negcarry))
        {
          negcarry = *--streamPtrCarry;
          negcarry++;
          *streamPtrCarry = negcarry;
        }
      } else {
        while (!(++(*--streamPtrCarry)));
      }
    }

    while ( !(W_upper & 0xFF000000) )
    {
      W_upper <<= 8;
      if (streamData->full == 0) {
        *streamPtr++ += (uint16_t) WEBRTC_SPL_RSHIFT_U32(
            streamData->streamval, 24);
        streamData->full = 1;
      } else {
        *streamPtr = (uint16_t)((streamData->streamval >> 24) << 8);
        streamData->full = 0;
      }

      if( streamPtr > maxStreamPtr )
        return -ISAC_DISALLOWED_BITSTREAM_LENGTH;

      streamData->streamval <<= 8;
    }
  }

  streamData->stream_index = streamPtr - streamData->stream;
  streamData->W_upper = W_upper;

  return 0;
}

int WebRtcIsacfix_DecLogisticMulti2(int16_t *dataQ7,
                                    Bitstr_dec *streamData,
                                    const int32_t *envQ8,
                                    const int16_t lenData)
{
  uint32_t    W_lower;
  uint32_t    W_upper;
  uint32_t    W_tmp;
  uint16_t    W_upper_LSB;
  uint16_t    W_upper_MSB;
  uint32_t    streamVal;
  uint16_t    cdfTmp;
  int32_t     res;
  int32_t     inSqrt;
  int32_t     newRes;
  const uint16_t *streamPtr;
  int16_t     candQ7;
  int16_t     envCount;
  uint16_t    tmpARSpecQ8 = 0;
  int             k, i;
  int offset = 0;

  streamPtr = streamData->stream + streamData->stream_index;
  W_upper = streamData->W_upper;

  if (streamData->stream_index == 0)
  {

    streamVal = (uint32_t)(*streamPtr++) << 16;
    streamVal |= *streamPtr++;

  } else {
    streamVal = streamData->streamval;
  }

  res = 1 << (WebRtcSpl_GetSizeInBits(envQ8[0]) >> 1);
  envCount = 0;

  for (k = 0; k < lenData; k += 4)
  {
    int k4;

    inSqrt = envQ8[envCount];
    i = 10;

    if (inSqrt < 0)
      inSqrt=-inSqrt;

    newRes = (inSqrt / res + res) >> 1;
    do
    {
      res = newRes;
      newRes = (inSqrt / res + res) >> 1;
    } while (newRes != res && i-- > 0);

    tmpARSpecQ8 = (uint16_t)newRes;

    for(k4 = 0; k4 < 4; k4++)
    {

      W_upper_LSB = (uint16_t) (W_upper & 0x0000FFFF);
      W_upper_MSB = (uint16_t) WEBRTC_SPL_RSHIFT_U32(W_upper, 16);

      candQ7 = - *dataQ7 + 64;
      cdfTmp = WebRtcIsacfix_Piecewise(WEBRTC_SPL_MUL_16_U16(candQ7, tmpARSpecQ8));

      W_tmp = (uint32_t)cdfTmp * W_upper_MSB;
      W_tmp += ((uint32_t)cdfTmp * (uint32_t)W_upper_LSB) >> 16;

      if (streamVal > W_tmp)
      {
        W_lower = W_tmp;
        candQ7 += 128;
        cdfTmp = WebRtcIsacfix_Piecewise(WEBRTC_SPL_MUL_16_U16(candQ7, tmpARSpecQ8));

        W_tmp = (uint32_t)cdfTmp * W_upper_MSB;
        W_tmp += ((uint32_t)cdfTmp * (uint32_t)W_upper_LSB) >> 16;

        while (streamVal > W_tmp)
        {
          W_lower = W_tmp;
          candQ7 += 128;
          cdfTmp = WebRtcIsacfix_Piecewise(
              WEBRTC_SPL_MUL_16_U16(candQ7, tmpARSpecQ8));

          W_tmp = (uint32_t)cdfTmp * W_upper_MSB;
          W_tmp += ((uint32_t)cdfTmp * (uint32_t)W_upper_LSB) >> 16;

          if (W_lower == W_tmp) {
            return -1;
          }
        }
        W_upper = W_tmp;

        *dataQ7 = candQ7 - 64;
      }
      else
      {
        W_upper = W_tmp;
        candQ7 -= 128;
        cdfTmp = WebRtcIsacfix_Piecewise(WEBRTC_SPL_MUL_16_U16(candQ7, tmpARSpecQ8));

        W_tmp = (uint32_t)cdfTmp * W_upper_MSB;
        W_tmp += ((uint32_t)cdfTmp * (uint32_t)W_upper_LSB) >> 16;

        while ( !(streamVal > W_tmp) )
        {
          W_upper = W_tmp;
          candQ7 -= 128;
          cdfTmp = WebRtcIsacfix_Piecewise(
              WEBRTC_SPL_MUL_16_U16(candQ7, tmpARSpecQ8));

          W_tmp = (uint32_t)cdfTmp * W_upper_MSB;
          W_tmp += ((uint32_t)cdfTmp * (uint32_t)W_upper_LSB) >> 16;

          if (W_upper == W_tmp){
            return -1;
          }
        }
        W_lower = W_tmp;

        *dataQ7 = candQ7 + 64;
      }

      dataQ7++;

      W_upper -= ++W_lower;

      streamVal -= W_lower;

      while ( !(W_upper & 0xFF000000) )
      {
        if (streamPtr < streamData->stream + streamData->stream_size) {

          if (streamData->full == 0) {
            streamVal = (streamVal << 8) | (*streamPtr++ & 0x00FF);
            streamData->full = 1;
          } else {
            streamVal = (streamVal << 8) | (*streamPtr >> 8);
            streamData->full = 0;
          }
        } else {

          streamVal <<= 8;
          if (streamData->full == 0) {
            offset++;
            streamData->full = 1;
          } else {
            streamData->full = 0;
          }
        }
        W_upper <<= 8;
      }
    }
    envCount++;
  }

  streamData->stream_index = streamPtr + offset - streamData->stream;
  streamData->W_upper = W_upper;
  streamData->streamval = streamVal;

  if ( W_upper > 0x01FFFFFF )
    return (streamData->stream_index*2 - 3 + !streamData->full);
  else
    return (streamData->stream_index*2 - 2 + !streamData->full);
}
