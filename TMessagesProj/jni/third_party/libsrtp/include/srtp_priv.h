/*
 * srtp_priv.h
 *
 * private internal data structures and functions for libSRTP
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */
/*
 *
 * Copyright (c) 2001-2017 Cisco Systems, Inc.
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

#ifndef SRTP_PRIV_H
#define SRTP_PRIV_H

#include "config.h"

#include "srtp.h"
#include "rdbx.h"
#include "rdb.h"
#include "integers.h"
#include "cipher.h"
#include "auth.h"
#include "aes.h"
#include "key.h"
#include "crypto_kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SRTP_VER_STRING "libsrtp2 2.1.0-pre"
#define SRTP_VERSION "2.1.0-pre"

typedef struct srtp_stream_ctx_t_ srtp_stream_ctx_t;
typedef srtp_stream_ctx_t *srtp_stream_t;

srtp_stream_t srtp_get_stream(srtp_t srtp, uint32_t ssrc);

srtp_err_status_t srtp_stream_init_keys(srtp_stream_ctx_t *srtp,
                                        srtp_master_key_t *master_key,
                                        const unsigned int current_mki_index);

srtp_err_status_t srtp_steam_init_all_master_keys(
    srtp_stream_ctx_t *srtp,
    unsigned char *key,
    srtp_master_key_t **keys,
    const unsigned int max_master_keys);

srtp_err_status_t srtp_stream_init(srtp_stream_t srtp, const srtp_policy_t *p);

typedef enum direction_t {
    dir_unknown = 0,
    dir_srtp_sender = 1,
    dir_srtp_receiver = 2
} direction_t;

typedef struct srtp_session_keys_t {
    srtp_cipher_t *rtp_cipher;
    srtp_cipher_t *rtp_xtn_hdr_cipher;
    srtp_auth_t *rtp_auth;
    srtp_cipher_t *rtcp_cipher;
    srtp_auth_t *rtcp_auth;
    uint8_t salt[SRTP_AEAD_SALT_LEN];
    uint8_t c_salt[SRTP_AEAD_SALT_LEN];
    uint8_t *mki_id;
    unsigned int mki_size;
    srtp_key_limit_ctx_t *limit;
} srtp_session_keys_t;

typedef struct srtp_stream_ctx_t_ {
    uint32_t ssrc;
    srtp_session_keys_t *session_keys;
    unsigned int num_master_keys;
    srtp_rdbx_t rtp_rdbx;
    srtp_sec_serv_t rtp_services;
    srtp_rdb_t rtcp_rdb;
    srtp_sec_serv_t rtcp_services;
    direction_t direction;
    int allow_repeat_tx;
    srtp_ekt_stream_t ekt;
    int *enc_xtn_hdr;
    int enc_xtn_hdr_count;
    uint32_t pending_roc;
    struct srtp_stream_ctx_t_ *next;
} strp_stream_ctx_t_;

typedef struct srtp_ctx_t_ {
    struct srtp_stream_ctx_t_ *stream_list;
    struct srtp_stream_ctx_t_ *stream_template;

    void *user_data;
} srtp_ctx_t_;

#ifndef WORDS_BIGENDIAN

typedef struct {
    unsigned char cc : 4;
    unsigned char x : 1;
    unsigned char p : 1;
    unsigned char version : 2;
    unsigned char pt : 7;
    unsigned char m : 1;
    uint16_t seq;
    uint32_t ts;
    uint32_t ssrc;
} srtp_hdr_t;

#else

typedef struct {
    unsigned char version : 2;
    unsigned char p : 1;
    unsigned char x : 1;
    unsigned char cc : 4;
    unsigned char m : 1;
    unsigned char pt : 7;
    uint16_t seq;
    uint32_t ts;
    uint32_t ssrc;
} srtp_hdr_t;

#endif

typedef struct {
    uint16_t profile_specific;
    uint16_t length;
} srtp_hdr_xtnd_t;

#ifndef WORDS_BIGENDIAN

typedef struct {
    unsigned char rc : 5;
    unsigned char p : 1;
    unsigned char version : 2;
    unsigned char pt : 8;
    uint16_t len;
    uint32_t ssrc;
} srtcp_hdr_t;

typedef struct {
    unsigned int index : 31;
    unsigned int e : 1;

} srtcp_trailer_t;

#else

typedef struct {
    unsigned char version : 2;
    unsigned char p : 1;
    unsigned char rc : 5;
    unsigned char pt : 8;
    uint16_t len;
    uint32_t ssrc;
} srtcp_hdr_t;

typedef struct {
    unsigned int e : 1;
    unsigned int index : 31;

} srtcp_trailer_t;

#endif

#define srtp_handle_event(srtp, strm, evnt)                                    \
    if (srtp_event_handler) {                                                  \
        srtp_event_data_t data;                                                \
        data.session = srtp;                                                   \
        data.ssrc = ntohl(strm->ssrc);                                         \
        data.event = evnt;                                                     \
        srtp_event_handler(&data);                                             \
    }

#ifdef __cplusplus
}
#endif

#endif
