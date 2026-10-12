/*
 * rdbx.h
 *
 * replay database with extended packet indices, using a rollover counter
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 *
 */

/*
 *
 * Copyright (c) 2001-2017, Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef RDBX_H
#define RDBX_H

#include "datatypes.h"
#include "err.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ROC_TEST

typedef uint16_t srtp_sequence_number_t;
typedef uint32_t srtp_rollover_counter_t;

#else

typedef unsigned char srtp_sequence_number_t;
typedef uint16_t srtp_rollover_counter_t;

#endif

#define seq_num_median (1 << (8 * sizeof(srtp_sequence_number_t) - 1))
#define seq_num_max (1 << (8 * sizeof(srtp_sequence_number_t)))

typedef uint64_t srtp_xtd_seq_num_t;

typedef struct {
    srtp_xtd_seq_num_t index;
    bitvector_t bitmask;
} srtp_rdbx_t;

srtp_err_status_t srtp_rdbx_init(srtp_rdbx_t *rdbx, unsigned long ws);

srtp_err_status_t srtp_rdbx_dealloc(srtp_rdbx_t *rdbx);

int32_t srtp_rdbx_estimate_index(const srtp_rdbx_t *rdbx,
                                 srtp_xtd_seq_num_t *guess,
                                 srtp_sequence_number_t s);

srtp_err_status_t srtp_rdbx_check(const srtp_rdbx_t *rdbx, int difference);

srtp_err_status_t srtp_rdbx_add_index(srtp_rdbx_t *rdbx, int delta);

srtp_err_status_t srtp_rdbx_set_roc(srtp_rdbx_t *rdbx, uint32_t roc);

srtp_xtd_seq_num_t srtp_rdbx_get_packet_index(const srtp_rdbx_t *rdbx);

unsigned long srtp_rdbx_get_window_size(const srtp_rdbx_t *rdbx);

void srtp_index_init(srtp_xtd_seq_num_t *pi);

void srtp_index_advance(srtp_xtd_seq_num_t *pi, srtp_sequence_number_t s);

int32_t srtp_index_guess(const srtp_xtd_seq_num_t *local,
                         srtp_xtd_seq_num_t *guess,
                         srtp_sequence_number_t s);

uint32_t srtp_rdbx_get_roc(const srtp_rdbx_t *rdbx);

srtp_err_status_t srtp_rdbx_set_roc_seq(srtp_rdbx_t *rdbx,
                                        uint32_t roc,
                                        uint16_t seq);

#ifdef __cplusplus
}
#endif

#endif
