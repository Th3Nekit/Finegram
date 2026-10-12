/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/******************************************************************

 iLBC Speech Coder ANSI-C Source Code

 WebRtcIlbcfix_PackBits.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/pack_bits.h"

#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_PackBits(
    uint16_t *bitstream,
    iLBC_bits *enc_bits,
    int16_t mode
                             ){
  uint16_t *bitstreamPtr;
  int i, k;
  int16_t *tmpPtr;

  bitstreamPtr=bitstream;

  (*bitstreamPtr)  = ((uint16_t)enc_bits->lsf[0])<<10;
  (*bitstreamPtr) |= (enc_bits->lsf[1])<<3;
  (*bitstreamPtr) |= (enc_bits->lsf[2]&0x70)>>4;
  bitstreamPtr++;

  (*bitstreamPtr)  = ((uint16_t)enc_bits->lsf[2]&0xF)<<12;

  if (mode==20) {
    (*bitstreamPtr) |= (enc_bits->startIdx)<<10;
    (*bitstreamPtr) |= (enc_bits->state_first)<<9;
    (*bitstreamPtr) |= (enc_bits->idxForMax)<<3;
    (*bitstreamPtr) |= ((enc_bits->cb_index[0])&0x70)>>4;
    bitstreamPtr++;

    (*bitstreamPtr) = ((enc_bits->cb_index[0])&0xE)<<12;
    (*bitstreamPtr) |= ((enc_bits->gain_index[0])&0x18)<<8;
    (*bitstreamPtr) |= ((enc_bits->gain_index[1])&0x8)<<7;
    (*bitstreamPtr) |= ((enc_bits->cb_index[3])&0xFE)<<2;
    (*bitstreamPtr) |= ((enc_bits->gain_index[3])&0x10)>>2;
    (*bitstreamPtr) |= ((enc_bits->gain_index[4])&0x8)>>2;
    (*bitstreamPtr) |= ((enc_bits->gain_index[6])&0x10)>>4;
  } else {
    (*bitstreamPtr) |= (enc_bits->lsf[3])<<6;
    (*bitstreamPtr) |= (enc_bits->lsf[4]&0x7E)>>1;
    bitstreamPtr++;

    (*bitstreamPtr)  = ((uint16_t)enc_bits->lsf[4]&0x1)<<15;
    (*bitstreamPtr) |= (enc_bits->lsf[5])<<8;
    (*bitstreamPtr) |= (enc_bits->startIdx)<<5;
    (*bitstreamPtr) |= (enc_bits->state_first)<<4;
    (*bitstreamPtr) |= ((enc_bits->idxForMax)&0x3C)>>2;
    bitstreamPtr++;

    (*bitstreamPtr)  = ((uint16_t)enc_bits->idxForMax&0x3)<<14;
    (*bitstreamPtr) |= (enc_bits->cb_index[0]&0x78)<<7;
    (*bitstreamPtr) |= (enc_bits->gain_index[0]&0x10)<<5;
    (*bitstreamPtr) |= (enc_bits->gain_index[1]&0x8)<<5;
    (*bitstreamPtr) |= (enc_bits->cb_index[3]&0xFC);
    (*bitstreamPtr) |= (enc_bits->gain_index[3]&0x10)>>3;
    (*bitstreamPtr) |= (enc_bits->gain_index[4]&0x8)>>3;
  }

  bitstreamPtr++;
  tmpPtr=enc_bits->idxVec;
  for (k=0; k<3; k++) {
    (*bitstreamPtr) = 0;
    for (i=15; i>=0; i--) {
      (*bitstreamPtr) |= ((uint16_t)((*tmpPtr)&0x4)>>2)<<i;

      tmpPtr++;
    }
    bitstreamPtr++;
  }

  if (mode==20) {

    (*bitstreamPtr) = 0;
    for (i=15; i>6; i--) {
      (*bitstreamPtr) |= ((uint16_t)((*tmpPtr)&0x4)>>2)<<i;

      tmpPtr++;
    }
    (*bitstreamPtr) |= (enc_bits->gain_index[1]&0x4)<<4;
    (*bitstreamPtr) |= (enc_bits->gain_index[3]&0xC)<<2;
    (*bitstreamPtr) |= (enc_bits->gain_index[4]&0x4)<<1;
    (*bitstreamPtr) |= (enc_bits->gain_index[6]&0x8)>>1;
    (*bitstreamPtr) |= (enc_bits->gain_index[7]&0xC)>>2;

  } else {

    (*bitstreamPtr) = 0;
    for (i=15; i>5; i--) {
      (*bitstreamPtr) |= ((uint16_t)((*tmpPtr)&0x4)>>2)<<i;

      tmpPtr++;
    }
    (*bitstreamPtr) |= (enc_bits->cb_index[0]&0x6)<<3;
    (*bitstreamPtr) |= (enc_bits->gain_index[0]&0x8);
    (*bitstreamPtr) |= (enc_bits->gain_index[1]&0x4);
    (*bitstreamPtr) |= (enc_bits->cb_index[3]&0x2);
    (*bitstreamPtr) |= (enc_bits->cb_index[6]&0x80)>>7;
    bitstreamPtr++;

    (*bitstreamPtr)  = ((uint16_t)enc_bits->cb_index[6]&0x7E)<<9;
    (*bitstreamPtr) |= (enc_bits->cb_index[9]&0xFE)<<2;
    (*bitstreamPtr) |= (enc_bits->cb_index[12]&0xE0)>>5;
    bitstreamPtr++;

    (*bitstreamPtr)  = ((uint16_t)enc_bits->cb_index[12]&0x1E)<<11;
    (*bitstreamPtr) |= (enc_bits->gain_index[3]&0xC)<<8;
    (*bitstreamPtr) |= (enc_bits->gain_index[4]&0x6)<<7;
    (*bitstreamPtr) |= (enc_bits->gain_index[6]&0x18)<<3;
    (*bitstreamPtr) |= (enc_bits->gain_index[7]&0xC)<<2;
    (*bitstreamPtr) |= (enc_bits->gain_index[9]&0x10)>>1;
    (*bitstreamPtr) |= (enc_bits->gain_index[10]&0x8)>>1;
    (*bitstreamPtr) |= (enc_bits->gain_index[12]&0x10)>>3;
    (*bitstreamPtr) |= (enc_bits->gain_index[13]&0x8)>>3;
  }
  bitstreamPtr++;

  tmpPtr=enc_bits->idxVec;
  for (k=0; k<7; k++) {
    (*bitstreamPtr) = 0;
    for (i=14; i>=0; i-=2) {
      (*bitstreamPtr) |= ((uint16_t)((*tmpPtr)&0x3))<<i;
      tmpPtr++;
    }
    bitstreamPtr++;
  }

  if (mode==20) {

    (*bitstreamPtr)  = ((uint16_t)((enc_bits->idxVec[56])&0x3))<<14;
    (*bitstreamPtr) |= (((enc_bits->cb_index[0])&1))<<13;
    (*bitstreamPtr) |= ((enc_bits->cb_index[1]))<<6;
    (*bitstreamPtr) |= ((enc_bits->cb_index[2])&0x7E)>>1;
    bitstreamPtr++;

    (*bitstreamPtr) = ((uint16_t)((enc_bits->cb_index[2])&0x1))<<15;

    (*bitstreamPtr) |= ((enc_bits->gain_index[0])&0x7)<<12;
    (*bitstreamPtr) |= ((enc_bits->gain_index[1])&0x3)<<10;
    (*bitstreamPtr) |= ((enc_bits->gain_index[2]))<<7;
    (*bitstreamPtr) |= ((enc_bits->cb_index[3])&0x1)<<6;
    (*bitstreamPtr) |= ((enc_bits->cb_index[4])&0x7E)>>1;
    bitstreamPtr++;

    (*bitstreamPtr) = ((uint16_t)((enc_bits->cb_index[4])&0x1))<<15;

    (*bitstreamPtr) |= (enc_bits->cb_index[5])<<8;
    (*bitstreamPtr) |= (enc_bits->cb_index[6]);
    bitstreamPtr++;

    (*bitstreamPtr) = ((uint16_t)(enc_bits->cb_index[7]))<<8;
    (*bitstreamPtr) |= (enc_bits->cb_index[8]);
    bitstreamPtr++;

    (*bitstreamPtr) = ((uint16_t)((enc_bits->gain_index[3])&0x3))<<14;

    (*bitstreamPtr) |= ((enc_bits->gain_index[4])&0x3)<<12;
    (*bitstreamPtr) |= ((enc_bits->gain_index[5]))<<9;
    (*bitstreamPtr) |= ((enc_bits->gain_index[6])&0x7)<<6;
    (*bitstreamPtr) |= ((enc_bits->gain_index[7])&0x3)<<4;
    (*bitstreamPtr) |= (enc_bits->gain_index[8])<<1;
  } else {

    (*bitstreamPtr)  = ((uint16_t)((enc_bits->idxVec[56])&0x3))<<14;
    (*bitstreamPtr) |= (((enc_bits->idxVec[57])&0x3))<<12;
    (*bitstreamPtr) |= (((enc_bits->cb_index[0])&1))<<11;
    (*bitstreamPtr) |= ((enc_bits->cb_index[1]))<<4;
    (*bitstreamPtr) |= ((enc_bits->cb_index[2])&0x78)>>3;
    bitstreamPtr++;

    (*bitstreamPtr)  = ((uint16_t)(enc_bits->cb_index[2])&0x7)<<13;

    (*bitstreamPtr) |= ((enc_bits->gain_index[0])&0x7)<<10;
    (*bitstreamPtr) |= ((enc_bits->gain_index[1])&0x3)<<8;
    (*bitstreamPtr) |= ((enc_bits->gain_index[2])&0x7)<<5;
    (*bitstreamPtr) |= ((enc_bits->cb_index[3])&0x1)<<4;
    (*bitstreamPtr) |= ((enc_bits->cb_index[4])&0x78)>>3;
    bitstreamPtr++;

    (*bitstreamPtr)  = ((uint16_t)(enc_bits->cb_index[4])&0x7)<<13;

    (*bitstreamPtr) |= ((enc_bits->cb_index[5]))<<6;
    (*bitstreamPtr) |= ((enc_bits->cb_index[6])&0x1)<<5;
    (*bitstreamPtr) |= ((enc_bits->cb_index[7])&0xF8)>>3;
    bitstreamPtr++;

    (*bitstreamPtr)  = ((uint16_t)(enc_bits->cb_index[7])&0x7)<<13;

    (*bitstreamPtr) |= ((enc_bits->cb_index[8]))<<5;
    (*bitstreamPtr) |= ((enc_bits->cb_index[9])&0x1)<<4;
    (*bitstreamPtr) |= ((enc_bits->cb_index[10])&0xF0)>>4;
    bitstreamPtr++;

    (*bitstreamPtr)  = ((uint16_t)(enc_bits->cb_index[10])&0xF)<<12;

    (*bitstreamPtr) |= ((enc_bits->cb_index[11]))<<4;
    (*bitstreamPtr) |= ((enc_bits->cb_index[12])&0x1)<<3;
    (*bitstreamPtr) |= ((enc_bits->cb_index[13])&0xE0)>>5;
    bitstreamPtr++;

    (*bitstreamPtr)  = ((uint16_t)(enc_bits->cb_index[13])&0x1F)<<11;

    (*bitstreamPtr) |= ((enc_bits->cb_index[14]))<<3;
    (*bitstreamPtr) |= ((enc_bits->gain_index[3])&0x3)<<1;
    (*bitstreamPtr) |= ((enc_bits->gain_index[4])&0x1);
    bitstreamPtr++;

    (*bitstreamPtr)  = ((uint16_t)(enc_bits->gain_index[5]))<<13;

    (*bitstreamPtr) |= ((enc_bits->gain_index[6])&0x7)<<10;
    (*bitstreamPtr) |= ((enc_bits->gain_index[7])&0x3)<<8;
    (*bitstreamPtr) |= ((enc_bits->gain_index[8]))<<5;
    (*bitstreamPtr) |= ((enc_bits->gain_index[9])&0xF)<<1;
    (*bitstreamPtr) |= ((enc_bits->gain_index[10])&0x4)>>2;
    bitstreamPtr++;

    (*bitstreamPtr)  = ((uint16_t)(enc_bits->gain_index[10])&0x3)<<14;

    (*bitstreamPtr) |= ((enc_bits->gain_index[11]))<<11;
    (*bitstreamPtr) |= ((enc_bits->gain_index[12])&0xF)<<7;
    (*bitstreamPtr) |= ((enc_bits->gain_index[13])&0x7)<<4;
    (*bitstreamPtr) |= ((enc_bits->gain_index[14]))<<1;
  }

  return;
}
