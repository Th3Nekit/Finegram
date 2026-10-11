/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/******************************************************************

 iLBC Speech Coder ANSI-C Source Code

 WebRtcIlbcfix_UnpackBits.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/unpack_bits.h"

#include "modules/audio_coding/codecs/ilbc/defines.h"

int16_t WebRtcIlbcfix_UnpackBits(
    const uint16_t *bitstream,
    iLBC_bits *enc_bits,
    int16_t mode
                                        ) {
  const uint16_t *bitstreamPtr;
  int i, k;
  int16_t *tmpPtr;

  bitstreamPtr=bitstream;

  enc_bits->lsf[0]  =  (*bitstreamPtr)>>10;
  enc_bits->lsf[1]  = ((*bitstreamPtr)>>3)&0x7F;
  enc_bits->lsf[2]  = ((*bitstreamPtr)&0x7)<<4;
  bitstreamPtr++;

  enc_bits->lsf[2] |= ((*bitstreamPtr)>>12)&0xF;

  if (mode==20) {
    enc_bits->startIdx             = ((*bitstreamPtr)>>10)&0x3;
    enc_bits->state_first          = ((*bitstreamPtr)>>9)&0x1;
    enc_bits->idxForMax            = ((*bitstreamPtr)>>3)&0x3F;
    enc_bits->cb_index[0]          = ((*bitstreamPtr)&0x7)<<4;
    bitstreamPtr++;

    enc_bits->cb_index[0]         |= ((*bitstreamPtr)>>12)&0xE;
    enc_bits->gain_index[0]        = ((*bitstreamPtr)>>8)&0x18;
    enc_bits->gain_index[1]        = ((*bitstreamPtr)>>7)&0x8;
    enc_bits->cb_index[3]          = ((*bitstreamPtr)>>2)&0xFE;
    enc_bits->gain_index[3]        = ((*bitstreamPtr)<<2)&0x10;
    enc_bits->gain_index[4]        = ((*bitstreamPtr)<<2)&0x8;
    enc_bits->gain_index[6]        = ((*bitstreamPtr)<<4)&0x10;
  } else {
    enc_bits->lsf[3]               = ((*bitstreamPtr)>>6)&0x3F;
    enc_bits->lsf[4]               = ((*bitstreamPtr)<<1)&0x7E;
    bitstreamPtr++;

    enc_bits->lsf[4]              |= ((*bitstreamPtr)>>15)&0x1;
    enc_bits->lsf[5]               = ((*bitstreamPtr)>>8)&0x7F;
    enc_bits->startIdx             = ((*bitstreamPtr)>>5)&0x7;
    enc_bits->state_first          = ((*bitstreamPtr)>>4)&0x1;
    enc_bits->idxForMax            = ((*bitstreamPtr)<<2)&0x3C;
    bitstreamPtr++;

    enc_bits->idxForMax           |= ((*bitstreamPtr)>>14)&0x3;
    enc_bits->cb_index[0]        = ((*bitstreamPtr)>>7)&0x78;
    enc_bits->gain_index[0]        = ((*bitstreamPtr)>>5)&0x10;
    enc_bits->gain_index[1]        = ((*bitstreamPtr)>>5)&0x8;
    enc_bits->cb_index[3]          = ((*bitstreamPtr))&0xFC;
    enc_bits->gain_index[3]        = ((*bitstreamPtr)<<3)&0x10;
    enc_bits->gain_index[4]        = ((*bitstreamPtr)<<3)&0x8;
  }

  bitstreamPtr++;
  tmpPtr=enc_bits->idxVec;
  for (k=0; k<3; k++) {
    for (i=15; i>=0; i--) {
      (*tmpPtr)                  = (((*bitstreamPtr)>>i)<<2)&0x4;

      tmpPtr++;
    }
    bitstreamPtr++;
  }

  if (mode==20) {

    for (i=15; i>6; i--) {
      (*tmpPtr)                  = (((*bitstreamPtr)>>i)<<2)&0x4;

      tmpPtr++;
    }
    enc_bits->gain_index[1]       |= ((*bitstreamPtr)>>4)&0x4;
    enc_bits->gain_index[3]       |= ((*bitstreamPtr)>>2)&0xC;
    enc_bits->gain_index[4]       |= ((*bitstreamPtr)>>1)&0x4;
    enc_bits->gain_index[6]       |= ((*bitstreamPtr)<<1)&0x8;
    enc_bits->gain_index[7]        = ((*bitstreamPtr)<<2)&0xC;

  } else {

    for (i=15; i>5; i--) {
      (*tmpPtr)                  = (((*bitstreamPtr)>>i)<<2)&0x4;

      tmpPtr++;
    }
    enc_bits->cb_index[0]         |= ((*bitstreamPtr)>>3)&0x6;
    enc_bits->gain_index[0]       |= ((*bitstreamPtr))&0x8;
    enc_bits->gain_index[1]       |= ((*bitstreamPtr))&0x4;
    enc_bits->cb_index[3]         |= ((*bitstreamPtr))&0x2;
    enc_bits->cb_index[6]          = ((*bitstreamPtr)<<7)&0x80;
    bitstreamPtr++;

    enc_bits->cb_index[6]         |= ((*bitstreamPtr)>>9)&0x7E;
    enc_bits->cb_index[9]          = ((*bitstreamPtr)>>2)&0xFE;
    enc_bits->cb_index[12]         = ((*bitstreamPtr)<<5)&0xE0;
    bitstreamPtr++;

    enc_bits->cb_index[12]         |= ((*bitstreamPtr)>>11)&0x1E;
    enc_bits->gain_index[3]       |= ((*bitstreamPtr)>>8)&0xC;
    enc_bits->gain_index[4]       |= ((*bitstreamPtr)>>7)&0x6;
    enc_bits->gain_index[6]        = ((*bitstreamPtr)>>3)&0x18;
    enc_bits->gain_index[7]        = ((*bitstreamPtr)>>2)&0xC;
    enc_bits->gain_index[9]        = ((*bitstreamPtr)<<1)&0x10;
    enc_bits->gain_index[10]       = ((*bitstreamPtr)<<1)&0x8;
    enc_bits->gain_index[12]       = ((*bitstreamPtr)<<3)&0x10;
    enc_bits->gain_index[13]       = ((*bitstreamPtr)<<3)&0x8;
  }
  bitstreamPtr++;

  tmpPtr=enc_bits->idxVec;
  for (k=0; k<7; k++) {
    for (i=14; i>=0; i-=2) {
      (*tmpPtr)                 |= ((*bitstreamPtr)>>i)&0x3;
      tmpPtr++;
    }
    bitstreamPtr++;
  }

  if (mode==20) {

    enc_bits->idxVec[56]          |= ((*bitstreamPtr)>>14)&0x3;
    enc_bits->cb_index[0]         |= ((*bitstreamPtr)>>13)&0x1;
    enc_bits->cb_index[1]          = ((*bitstreamPtr)>>6)&0x7F;
    enc_bits->cb_index[2]          = ((*bitstreamPtr)<<1)&0x7E;
    bitstreamPtr++;

    enc_bits->cb_index[2]         |= ((*bitstreamPtr)>>15)&0x1;
    enc_bits->gain_index[0]       |= ((*bitstreamPtr)>>12)&0x7;
    enc_bits->gain_index[1]       |= ((*bitstreamPtr)>>10)&0x3;
    enc_bits->gain_index[2]        = ((*bitstreamPtr)>>7)&0x7;
    enc_bits->cb_index[3]         |= ((*bitstreamPtr)>>6)&0x1;
    enc_bits->cb_index[4]          = ((*bitstreamPtr)<<1)&0x7E;
    bitstreamPtr++;

    enc_bits->cb_index[4]         |= ((*bitstreamPtr)>>15)&0x1;
    enc_bits->cb_index[5]          = ((*bitstreamPtr)>>8)&0x7F;
    enc_bits->cb_index[6]          = ((*bitstreamPtr))&0xFF;
    bitstreamPtr++;

    enc_bits->cb_index[7]          = (*bitstreamPtr)>>8;
    enc_bits->cb_index[8]          = (*bitstreamPtr)&0xFF;
    bitstreamPtr++;

    enc_bits->gain_index[3]       |= ((*bitstreamPtr)>>14)&0x3;
    enc_bits->gain_index[4]       |= ((*bitstreamPtr)>>12)&0x3;
    enc_bits->gain_index[5]        = ((*bitstreamPtr)>>9)&0x7;
    enc_bits->gain_index[6]       |= ((*bitstreamPtr)>>6)&0x7;
    enc_bits->gain_index[7]       |= ((*bitstreamPtr)>>4)&0x3;
    enc_bits->gain_index[8]        = ((*bitstreamPtr)>>1)&0x7;
  } else {

    enc_bits->idxVec[56]          |= ((*bitstreamPtr)>>14)&0x3;
    enc_bits->idxVec[57]          |= ((*bitstreamPtr)>>12)&0x3;
    enc_bits->cb_index[0]         |= ((*bitstreamPtr)>>11)&1;
    enc_bits->cb_index[1]          = ((*bitstreamPtr)>>4)&0x7F;
    enc_bits->cb_index[2]          = ((*bitstreamPtr)<<3)&0x78;
    bitstreamPtr++;

    enc_bits->cb_index[2]         |= ((*bitstreamPtr)>>13)&0x7;
    enc_bits->gain_index[0]       |= ((*bitstreamPtr)>>10)&0x7;
    enc_bits->gain_index[1]       |= ((*bitstreamPtr)>>8)&0x3;
    enc_bits->gain_index[2]        = ((*bitstreamPtr)>>5)&0x7;
    enc_bits->cb_index[3]         |= ((*bitstreamPtr)>>4)&0x1;
    enc_bits->cb_index[4]          = ((*bitstreamPtr)<<3)&0x78;
    bitstreamPtr++;

    enc_bits->cb_index[4]         |= ((*bitstreamPtr)>>13)&0x7;
    enc_bits->cb_index[5]          = ((*bitstreamPtr)>>6)&0x7F;
    enc_bits->cb_index[6]         |= ((*bitstreamPtr)>>5)&0x1;
    enc_bits->cb_index[7]          = ((*bitstreamPtr)<<3)&0xF8;
    bitstreamPtr++;

    enc_bits->cb_index[7]         |= ((*bitstreamPtr)>>13)&0x7;
    enc_bits->cb_index[8]          = ((*bitstreamPtr)>>5)&0xFF;
    enc_bits->cb_index[9]         |= ((*bitstreamPtr)>>4)&0x1;
    enc_bits->cb_index[10]         = ((*bitstreamPtr)<<4)&0xF0;
    bitstreamPtr++;

    enc_bits->cb_index[10]        |= ((*bitstreamPtr)>>12)&0xF;
    enc_bits->cb_index[11]         = ((*bitstreamPtr)>>4)&0xFF;
    enc_bits->cb_index[12]        |= ((*bitstreamPtr)>>3)&0x1;
    enc_bits->cb_index[13]         = ((*bitstreamPtr)<<5)&0xE0;
    bitstreamPtr++;

    enc_bits->cb_index[13]        |= ((*bitstreamPtr)>>11)&0x1F;
    enc_bits->cb_index[14]         = ((*bitstreamPtr)>>3)&0xFF;
    enc_bits->gain_index[3]       |= ((*bitstreamPtr)>>1)&0x3;
    enc_bits->gain_index[4]       |= ((*bitstreamPtr)&0x1);
    bitstreamPtr++;

    enc_bits->gain_index[5]        = ((*bitstreamPtr)>>13)&0x7;
    enc_bits->gain_index[6]       |= ((*bitstreamPtr)>>10)&0x7;
    enc_bits->gain_index[7]       |= ((*bitstreamPtr)>>8)&0x3;
    enc_bits->gain_index[8]        = ((*bitstreamPtr)>>5)&0x7;
    enc_bits->gain_index[9]       |= ((*bitstreamPtr)>>1)&0xF;
    enc_bits->gain_index[10]      |= ((*bitstreamPtr)<<2)&0x4;
    bitstreamPtr++;

    enc_bits->gain_index[10]      |= ((*bitstreamPtr)>>14)&0x3;
    enc_bits->gain_index[11]       = ((*bitstreamPtr)>>11)&0x7;
    enc_bits->gain_index[12]      |= ((*bitstreamPtr)>>7)&0xF;
    enc_bits->gain_index[13]      |= ((*bitstreamPtr)>>4)&0x7;
    enc_bits->gain_index[14]       = ((*bitstreamPtr)>>1)&0x7;
  }

  if (((*bitstreamPtr)&0x1) == 1) {
    return(1);
  } else {
    return(0);
  }
}
