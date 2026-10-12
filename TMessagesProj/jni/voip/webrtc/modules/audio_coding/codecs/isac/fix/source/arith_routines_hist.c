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
 * arith_routinshist.c
 *
 * This C file contains arithmetic encoding and decoding.
 *
 */

#include "modules/audio_coding/codecs/isac/fix/source/arith_routins.h"

int WebRtcIsacfix_EncHistMulti(Bitstr_enc *streamData,
                               const int16_t *data,
                               const uint16_t *const *cdf,
                               const int16_t lenData)
{
  uint32_t W_lower;
  uint32_t W_upper;
  uint32_t W_upper_LSB;
  uint32_t W_upper_MSB;
  uint16_t *streamPtr;
  uint16_t negCarry;
  uint16_t *maxStreamPtr;
  uint16_t *streamPtrCarry;
  uint32_t cdfLo;
  uint32_t cdfHi;
  int k;

  streamPtr = streamData->stream + streamData->stream_index;
  maxStreamPtr = streamData->stream + STREAM_MAXW16_60MS - 1;

  W_upper = streamData->W_upper;

  for (k = lenData; k > 0; k--)
  {

    cdfLo = (uint32_t) *(*cdf + (uint32_t)*data);
    cdfHi = (uint32_t) *(*cdf++ + (uint32_t)*data++ + 1);

    W_upper_LSB = W_upper & 0x0000FFFF;
    W_upper_MSB = W_upper >> 16;
    W_lower = WEBRTC_SPL_UMUL(W_upper_MSB, cdfLo);
    W_lower += ((W_upper_LSB * cdfLo) >> 16);
    W_upper = WEBRTC_SPL_UMUL(W_upper_MSB, cdfHi);
    W_upper += ((W_upper_LSB * cdfHi) >> 16);

    W_upper -= ++W_lower;

    streamData->streamval += W_lower;

    if (streamData->streamval < W_lower)
    {

      streamPtrCarry = streamPtr;
      if (streamData->full == 0) {
        negCarry = *streamPtrCarry;
        negCarry += 0x0100;
        *streamPtrCarry = negCarry;
        while (!(negCarry))
        {
          negCarry = *--streamPtrCarry;
          negCarry++;
          *streamPtrCarry = negCarry;
        }
      } else {
        while ( !(++(*--streamPtrCarry)) );
      }
    }

    while ( !(W_upper & 0xFF000000) )
    {
      W_upper <<= 8;
      if (streamData->full == 0) {
        *streamPtr++ += (uint16_t)(streamData->streamval >> 24);
        streamData->full = 1;
      } else {
        *streamPtr = (uint16_t)((streamData->streamval >> 24) << 8);
        streamData->full = 0;
      }

      if( streamPtr > maxStreamPtr ) {
        return -ISAC_DISALLOWED_BITSTREAM_LENGTH;
      }
      streamData->streamval <<= 8;
    }
  }

  streamData->stream_index = streamPtr - streamData->stream;
  streamData->W_upper = W_upper;

  return 0;
}

int16_t WebRtcIsacfix_DecHistBisectMulti(int16_t *data,
                                         Bitstr_dec *streamData,
                                         const uint16_t *const *cdf,
                                         const uint16_t *cdfSize,
                                         const int16_t lenData)
{
  uint32_t    W_lower = 0;
  uint32_t    W_upper;
  uint32_t    W_tmp;
  uint32_t    W_upper_LSB;
  uint32_t    W_upper_MSB;
  uint32_t    streamval;
  const uint16_t *streamPtr;
  const uint16_t *cdfPtr;
  int16_t     sizeTmp;
  int             k;

  streamPtr = streamData->stream + streamData->stream_index;
  W_upper = streamData->W_upper;

  if (W_upper == 0) {
    return -2;
  }

  if (streamData->stream_index == 0)
  {

    streamval = (uint32_t)*streamPtr++ << 16;
    streamval |= *streamPtr++;
  } else {
    streamval = streamData->streamval;
  }

  for (k = lenData; k > 0; k--)
  {

    W_upper_LSB = W_upper & 0x0000FFFF;
    W_upper_MSB = W_upper >> 16;

    sizeTmp = *cdfSize++ / 2;
    cdfPtr = *cdf + (sizeTmp - 1);

    for ( ;; )
    {
      W_tmp = WEBRTC_SPL_UMUL_32_16(W_upper_MSB, *cdfPtr);
      W_tmp += (W_upper_LSB * (*cdfPtr)) >> 16;
      sizeTmp /= 2;
      if (sizeTmp == 0) {
        break;
      }

      if (streamval > W_tmp)
      {
        W_lower = W_tmp;
        cdfPtr += sizeTmp;
      } else {
        W_upper = W_tmp;
        cdfPtr -= sizeTmp;
      }
    }
    if (streamval > W_tmp)
    {
      W_lower = W_tmp;
      *data++ = cdfPtr - *cdf++;
    } else {
      W_upper = W_tmp;
      *data++ = cdfPtr - *cdf++ - 1;
    }

    W_upper -= ++W_lower;

    streamval -= W_lower;

    while ( !(W_upper & 0xFF000000) )
    {

      if (streamData->full == 0) {
        streamval = (streamval << 8) | (*streamPtr++ & 0x00FF);
        streamData->full = 1;
      } else {
        streamval = (streamval << 8) | (*streamPtr >> 8);
        streamData->full = 0;
      }
      W_upper <<= 8;
    }

    if (W_upper == 0) {
      return -2;
    }

  }

  streamData->stream_index = streamPtr - streamData->stream;
  streamData->W_upper = W_upper;
  streamData->streamval = streamval;

  if ( W_upper > 0x01FFFFFF ) {
    return (streamData->stream_index*2 - 3 + !streamData->full);
  } else {
    return (streamData->stream_index*2 - 2 + !streamData->full);
  }
}

int16_t WebRtcIsacfix_DecHistOneStepMulti(int16_t *data,
                                          Bitstr_dec *streamData,
                                          const uint16_t *const *cdf,
                                          const uint16_t *initIndex,
                                          const int16_t lenData)
{
  uint32_t    W_lower;
  uint32_t    W_upper;
  uint32_t    W_tmp;
  uint32_t    W_upper_LSB;
  uint32_t    W_upper_MSB;
  uint32_t    streamval;
  const uint16_t *streamPtr;
  const uint16_t *cdfPtr;
  int             k;

  streamPtr = streamData->stream + streamData->stream_index;
  W_upper = streamData->W_upper;

  if (W_upper == 0) {
    return -2;
  }

  if (streamData->stream_index == 0)
  {

    streamval = (uint32_t)(*streamPtr++) << 16;
    streamval |= *streamPtr++;
  } else {
    streamval = streamData->streamval;
  }

  for (k = lenData; k > 0; k--)
  {

    W_upper_LSB = W_upper & 0x0000FFFF;
    W_upper_MSB = WEBRTC_SPL_RSHIFT_U32(W_upper, 16);

    cdfPtr = *cdf + (*initIndex++);
    W_tmp = WEBRTC_SPL_UMUL_32_16(W_upper_MSB, *cdfPtr);
    W_tmp += (W_upper_LSB * (*cdfPtr)) >> 16;

    if (streamval > W_tmp)
    {
      for ( ;; )
      {
        W_lower = W_tmp;

        if (cdfPtr[0] == 65535) {
          return -3;
        }

        W_tmp = WEBRTC_SPL_UMUL_32_16(W_upper_MSB, *++cdfPtr);
        W_tmp += (W_upper_LSB * (*cdfPtr)) >> 16;

        if (streamval <= W_tmp) {
          break;
        }
      }
      W_upper = W_tmp;
      *data++ = cdfPtr - *cdf++ - 1;
    } else {
      for ( ;; )
      {
        W_upper = W_tmp;
        --cdfPtr;

        if (cdfPtr < *cdf) {
          return -3;
        }

        W_tmp = WEBRTC_SPL_UMUL_32_16(W_upper_MSB, *cdfPtr);
        W_tmp += (W_upper_LSB * (*cdfPtr)) >> 16;

        if (streamval > W_tmp) {
          break;
        }
      }
      W_lower = W_tmp;
      *data++ = cdfPtr - *cdf++;
    }

    W_upper -= ++W_lower;

    streamval -= W_lower;

    while ( !(W_upper & 0xFF000000) )
    {

      if (streamData->full == 0) {
        streamval = (streamval << 8) | (*streamPtr++ & 0x00FF);
        streamData->full = 1;
      } else {
        streamval = (streamval << 8) | (*streamPtr >> 8);
        streamData->full = 0;
      }
      W_upper <<= 8;
    }
  }

  streamData->stream_index = streamPtr - streamData->stream;
  streamData->W_upper = W_upper;
  streamData->streamval = streamval;

  if ( W_upper > 0x01FFFFFF ) {
    return (streamData->stream_index*2 - 3 + !streamData->full);
  } else {
    return (streamData->stream_index*2 - 2 + !streamData->full);
  }
}
