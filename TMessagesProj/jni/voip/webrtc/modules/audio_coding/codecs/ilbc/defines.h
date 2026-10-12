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

 define.h

******************************************************************/
#ifndef MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_DEFINES_H_
#define MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_DEFINES_H_

#include <stdint.h>
#include <string.h>

#include "common_audio/signal_processing/include/signal_processing_library.h"

#define FS 8000
#define BLOCKL_20MS 160
#define BLOCKL_30MS 240
#define BLOCKL_MAX 240
#define NSUB_20MS 4
#define NSUB_30MS 6
#define NSUB_MAX 6
#define NASUB_20MS 2
#define NASUB_30MS 4
#define NASUB_MAX 4
#define SUBL 40
#define STATE_LEN 80
#define STATE_SHORT_LEN_30MS 58
#define STATE_SHORT_LEN_20MS 57

#define LPC_FILTERORDER 10
#define LPC_LOOKBACK 60
#define LPC_N_20MS 1
#define LPC_N_30MS 2
#define LPC_N_MAX 2
#define LPC_ASYMDIFF 20
#define LSF_NSPLIT 3
#define LSF_NUMBER_OF_STEPS 4
#define LPC_HALFORDER 5
#define COS_GRID_POINTS 60

#define CB_NSTAGES 3
#define CB_EXPAND 2
#define CB_MEML 147
#define CB_FILTERLEN (2 * 4)
#define CB_HALFFILTERLEN 4
#define CB_RESRANGE 34
#define CB_MAXGAIN_FIXQ6 83
#define CB_MAXGAIN_FIXQ14 21299

#define ENH_BLOCKL 80
#define ENH_BLOCKL_HALF (ENH_BLOCKL / 2)
#define ENH_HL                                                         \
  3

#define ENH_SLOP                    \
  2

#define ENH_PLOCSL                                                          \
  8

#define ENH_OVERHANG 2
#define ENH_UPS0 4
#define ENH_FL0 3
#define ENH_FLO_MULT2_PLUS1 7
#define ENH_VECTL (ENH_BLOCKL + 2 * ENH_FL0)
#define ENH_CORRDIM (2 * ENH_SLOP + 1)
#define ENH_NBLOCKS (BLOCKL / ENH_BLOCKL)
#define ENH_NBLOCKS_EXTRA 5
#define ENH_NBLOCKS_TOT 8
#define ENH_BUFL (ENH_NBLOCKS_TOT) * ENH_BLOCKL
#define ENH_BUFL_FILTEROVERHEAD 3
#define ENH_A0 819
#define ENH_A0_MINUS_A0A0DIV4 848256041
#define ENH_A0DIV2 26843546

#define FILTERORDER_DS_PLUS1 7
#define DELAY_DS 3
#define FACTOR_DS 2

#define NO_OF_BYTES_20MS 38
#define NO_OF_BYTES_30MS 50
#define NO_OF_WORDS_20MS 19
#define NO_OF_WORDS_30MS 25
#define STATE_BITS 3
#define BYTE_LEN 8
#define ULP_CLASSES 3

#define TWO_PI_FIX 25736

#define ST_MEM_L_TBL 85
#define MEM_LF_TBL 147

typedef struct iLBC_bits_t_ {
  int16_t lsf[LSF_NSPLIT * LPC_N_MAX];
  int16_t cb_index[CB_NSTAGES * (NASUB_MAX + 1)];

  int16_t gain_index[CB_NSTAGES * (NASUB_MAX + 1)];

  size_t idxForMax;
  int16_t state_first;
  int16_t idxVec[STATE_SHORT_LEN_30MS];
  int16_t firstbits;
  size_t startIdx;
} iLBC_bits;

typedef struct IlbcEncoder_ {

  int16_t mode;

  size_t blockl;
  size_t nsub;
  int16_t nasub;
  size_t no_of_bytes, no_of_words;
  int16_t lpc_n;
  size_t state_short_len;

  int16_t anaMem[LPC_FILTERORDER];

  int16_t lsfold[LPC_FILTERORDER];
  int16_t lsfdeqold[LPC_FILTERORDER];

  int16_t lpc_buffer[LPC_LOOKBACK + BLOCKL_MAX];

  int16_t hpimemx[2];
  int16_t hpimemy[4];

#ifdef SPLIT_10MS
  int16_t weightdenumbuf[66];
  int16_t past_samples[160];
  uint16_t bytes[25];
  int16_t section;
  int16_t Nfor_flag;
  int16_t Nback_flag;
  int16_t start_pos;
  size_t diff;
#endif

} IlbcEncoder;

typedef struct IlbcDecoder_ {

  int16_t mode;

  size_t blockl;
  size_t nsub;
  int16_t nasub;
  size_t no_of_bytes, no_of_words;
  int16_t lpc_n;
  size_t state_short_len;

  int16_t syntMem[LPC_FILTERORDER];

  int16_t lsfdeqold[LPC_FILTERORDER];

  size_t last_lag;

  int consPLICount, prev_enh_pl;
  int16_t perSquare;

  int16_t prevScale, prevPLI;
  size_t prevLag;
  int16_t prevLpc[LPC_FILTERORDER + 1];
  int16_t prevResidual[NSUB_MAX * SUBL];
  int16_t seed;

  int16_t old_syntdenum[(LPC_FILTERORDER + 1) * NSUB_MAX];

  int16_t hpimemx[2];
  int16_t hpimemy[4];

  int use_enhancer;
  int16_t enh_buf[ENH_BUFL + ENH_BUFL_FILTEROVERHEAD];
  size_t enh_period[ENH_NBLOCKS_TOT];

} IlbcDecoder;

#endif
