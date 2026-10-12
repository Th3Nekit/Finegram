/*
 * rdbx.c
 *
 * a replay database with extended range, using a rollover counter
 *
 * David A. McGrew
 * Cisco Systems, Inc.
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

#include "config.h"

#include "rdbx.h"

void srtp_index_init(srtp_xtd_seq_num_t *pi)
{
#ifdef NO_64BIT_MATH
    *pi = make64(0, 0);
#else
    *pi = 0;
#endif
}

void srtp_index_advance(srtp_xtd_seq_num_t *pi, srtp_sequence_number_t s)
{
#ifdef NO_64BIT_MATH

    *pi = make64(high32(*pi) + (s > ~low32(*pi) ? 1 : 0), low32(*pi) + s);
#else
    *pi += s;
#endif
}

int32_t srtp_index_guess(const srtp_xtd_seq_num_t *local,
                         srtp_xtd_seq_num_t *guess,
                         srtp_sequence_number_t s)
{
#ifdef NO_64BIT_MATH
    uint32_t local_roc = ((high32(*local) << 16) | (low32(*local) >> 16));
    uint16_t local_seq = (uint16_t)(low32(*local));
#else
    uint32_t local_roc = (uint32_t)(*local >> 16);
    uint16_t local_seq = (uint16_t)*local;
#endif
    uint32_t guess_roc;
    uint16_t guess_seq;
    int32_t difference;

    if (local_seq < seq_num_median) {
        if (s - local_seq > seq_num_median) {
            guess_roc = local_roc - 1;
            difference = s - local_seq - seq_num_max;
        } else {
            guess_roc = local_roc;
            difference = s - local_seq;
        }
    } else {
        if (local_seq - seq_num_median > s) {
            guess_roc = local_roc + 1;
            difference = s - local_seq + seq_num_max;
        } else {
            guess_roc = local_roc;
            difference = s - local_seq;
        }
    }
    guess_seq = s;

#ifdef NO_64BIT_MATH
    *guess = make64(guess_roc >> 16, (guess_roc << 16) | guess_seq);
#else
    *guess = (((uint64_t)guess_roc) << 16) | guess_seq;
#endif

    return difference;
}

srtp_err_status_t srtp_rdbx_init(srtp_rdbx_t *rdbx, unsigned long ws)
{
    if (ws == 0) {
        return srtp_err_status_bad_param;
    }

    if (bitvector_alloc(&rdbx->bitmask, ws) != 0) {
        return srtp_err_status_alloc_fail;
    }

    srtp_index_init(&rdbx->index);

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_rdbx_dealloc(srtp_rdbx_t *rdbx)
{
    bitvector_dealloc(&rdbx->bitmask);

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_rdbx_set_roc(srtp_rdbx_t *rdbx, uint32_t roc)
{
    bitvector_set_to_zero(&rdbx->bitmask);

#ifdef NO_64BIT_MATH
#error not yet implemented
#else

    if (roc < (rdbx->index >> 16)) {
        return srtp_err_status_replay_old;
    }

    rdbx->index &= 0xffff;
    rdbx->index |= ((uint64_t)roc) << 16;
#endif

    return srtp_err_status_ok;
}

srtp_xtd_seq_num_t srtp_rdbx_get_packet_index(const srtp_rdbx_t *rdbx)
{
    return rdbx->index;
}

unsigned long srtp_rdbx_get_window_size(const srtp_rdbx_t *rdbx)
{
    return bitvector_get_length(&rdbx->bitmask);
}

srtp_err_status_t srtp_rdbx_check(const srtp_rdbx_t *rdbx, int delta)
{
    if (delta > 0) {
        return srtp_err_status_ok;
    } else if ((int)(bitvector_get_length(&rdbx->bitmask) - 1) + delta < 0) {

        return srtp_err_status_replay_old;
    } else if (bitvector_get_bit(
                   &rdbx->bitmask,
                   (int)(bitvector_get_length(&rdbx->bitmask) - 1) + delta) ==
               1) {

        return srtp_err_status_replay_fail;
    }

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_rdbx_add_index(srtp_rdbx_t *rdbx, int delta)
{
    if (delta > 0) {

        srtp_index_advance(&rdbx->index, delta);
        bitvector_left_shift(&rdbx->bitmask, delta);
        bitvector_set_bit(&rdbx->bitmask,
                          bitvector_get_length(&rdbx->bitmask) - 1);
    } else {

        bitvector_set_bit(&rdbx->bitmask,
                          bitvector_get_length(&rdbx->bitmask) - 1 + delta);
    }

    return srtp_err_status_ok;
}

int32_t srtp_rdbx_estimate_index(const srtp_rdbx_t *rdbx,
                                 srtp_xtd_seq_num_t *guess,
                                 srtp_sequence_number_t s)
{

#ifdef NO_64BIT_MATH

    if (high32(rdbx->index) > 0 || low32(rdbx->index) > seq_num_median)
#else
    if (rdbx->index > seq_num_median)
#endif
    {
        return srtp_index_guess(&rdbx->index, guess, s);
    }

#ifdef NO_64BIT_MATH
    *guess = make64(0, (uint32_t)s);
#else
    *guess = s;
#endif

#ifdef NO_64BIT_MATH
    return s - (uint16_t)low32(rdbx->index);
#else
    return s - (uint16_t)rdbx->index;
#endif
}

uint32_t srtp_rdbx_get_roc(const srtp_rdbx_t *rdbx)
{
    uint32_t roc;

#ifdef NO_64BIT_MATH
    roc = ((high32(rdbx->index) << 16) | (low32(rdbx->index) >> 16));
#else
    roc = (uint32_t)(rdbx->index >> 16);
#endif

    return roc;
}

srtp_err_status_t srtp_rdbx_set_roc_seq(srtp_rdbx_t *rdbx,
                                        uint32_t roc,
                                        uint16_t seq)
{
#ifdef NO_64BIT_MATH
#error not yet implemented
#else

    if (roc < (rdbx->index >> 16)) {
        return srtp_err_status_replay_old;
    }

    rdbx->index = seq;
    rdbx->index |= ((uint64_t)roc) << 16;
#endif

    bitvector_set_to_zero(&rdbx->bitmask);

    return srtp_err_status_ok;
}
