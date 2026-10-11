/*
 * srtp.h
 *
 * interface to libsrtp
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

#ifndef SRTP_SRTP_H
#define SRTP_SRTP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SRTP_MASTER_KEY_LEN 30

#define SRTP_MAX_KEY_LEN 64

#define SRTP_MAX_TAG_LEN 16

#define SRTP_MAX_MKI_LEN 128

#define SRTP_MAX_TRAILER_LEN (SRTP_MAX_TAG_LEN + SRTP_MAX_MKI_LEN)

#define SRTP_MAX_NUM_MASTER_KEYS 16

#define SRTP_SALT_LEN 14

#define SRTP_AEAD_SALT_LEN 12

#define SRTP_AES_128_KEY_LEN 16
#define SRTP_AES_192_KEY_LEN 24
#define SRTP_AES_256_KEY_LEN 32

#define SRTP_AES_ICM_128_KEY_LEN_WSALT (SRTP_SALT_LEN + SRTP_AES_128_KEY_LEN)
#define SRTP_AES_ICM_192_KEY_LEN_WSALT (SRTP_SALT_LEN + SRTP_AES_192_KEY_LEN)
#define SRTP_AES_ICM_256_KEY_LEN_WSALT (SRTP_SALT_LEN + SRTP_AES_256_KEY_LEN)

#define SRTP_AES_GCM_128_KEY_LEN_WSALT                                         \
    (SRTP_AEAD_SALT_LEN + SRTP_AES_128_KEY_LEN)
#define SRTP_AES_GCM_192_KEY_LEN_WSALT                                         \
    (SRTP_AEAD_SALT_LEN + SRTP_AES_192_KEY_LEN)
#define SRTP_AES_GCM_256_KEY_LEN_WSALT                                         \
    (SRTP_AEAD_SALT_LEN + SRTP_AES_256_KEY_LEN)

typedef uint32_t srtp_cipher_type_id_t;

typedef uint32_t srtp_auth_type_id_t;

typedef enum {
    srtp_err_status_ok = 0,
    srtp_err_status_fail = 1,
    srtp_err_status_bad_param = 2,
    srtp_err_status_alloc_fail = 3,
    srtp_err_status_dealloc_fail = 4,
    srtp_err_status_init_fail = 5,
    srtp_err_status_terminus = 6,

    srtp_err_status_auth_fail = 7,
    srtp_err_status_cipher_fail = 8,
    srtp_err_status_replay_fail = 9,
    srtp_err_status_replay_old = 10,

    srtp_err_status_algo_fail = 11,
    srtp_err_status_no_such_op = 12,
    srtp_err_status_no_ctx = 13,
    srtp_err_status_cant_check = 14,

    srtp_err_status_key_expired = 15,
    srtp_err_status_socket_err = 16,
    srtp_err_status_signal_err = 17,
    srtp_err_status_nonce_bad = 18,
    srtp_err_status_read_fail = 19,
    srtp_err_status_write_fail = 20,
    srtp_err_status_parse_err = 21,
    srtp_err_status_encode_err = 22,
    srtp_err_status_semaphore_err = 23,
    srtp_err_status_pfkey_err = 24,
    srtp_err_status_bad_mki = 25,

    srtp_err_status_pkt_idx_old = 26,

    srtp_err_status_pkt_idx_adv = 27

} srtp_err_status_t;

typedef struct srtp_ctx_t_ srtp_ctx_t;

typedef enum {
    sec_serv_none = 0,
    sec_serv_conf = 1,
    sec_serv_auth = 2,
    sec_serv_conf_and_auth = 3
} srtp_sec_serv_t;

typedef struct srtp_crypto_policy_t {
    srtp_cipher_type_id_t cipher_type;

    int cipher_key_len;

    srtp_auth_type_id_t auth_type;

    int auth_key_len;

    int auth_tag_len;

    srtp_sec_serv_t sec_serv;

} srtp_crypto_policy_t;

typedef enum {
    ssrc_undefined = 0,
    ssrc_specific = 1,
    ssrc_any_inbound = 2,

    ssrc_any_outbound = 3

} srtp_ssrc_type_t;

typedef struct {
    srtp_ssrc_type_t type;
    unsigned int value;

} srtp_ssrc_t;

typedef struct srtp_ekt_policy_ctx_t *srtp_ekt_policy_t;

typedef struct srtp_ekt_stream_ctx_t *srtp_ekt_stream_t;

typedef struct srtp_master_key_t {
    unsigned char *key;
    unsigned char *mki_id;
    unsigned int mki_size;
} srtp_master_key_t;

typedef struct srtp_policy_t {
    srtp_ssrc_t ssrc;

    srtp_crypto_policy_t rtp;
    srtp_crypto_policy_t rtcp;
    unsigned char *key;

    srtp_master_key_t **keys;
    unsigned long num_master_keys;
    srtp_ekt_policy_t ekt;

    unsigned long window_size;

    int allow_repeat_tx;

    int *enc_xtn_hdr;
    int enc_xtn_hdr_count;

    struct srtp_policy_t *next;
} srtp_policy_t;

typedef srtp_ctx_t *srtp_t;

srtp_err_status_t srtp_init(void);

srtp_err_status_t srtp_shutdown(void);

srtp_err_status_t srtp_protect(srtp_t ctx, void *rtp_hdr, int *len_ptr);

srtp_err_status_t srtp_protect_mki(srtp_ctx_t *ctx,
                                   void *rtp_hdr,
                                   int *pkt_octet_len,
                                   unsigned int use_mki,
                                   unsigned int mki_index);

srtp_err_status_t srtp_unprotect(srtp_t ctx, void *srtp_hdr, int *len_ptr);

srtp_err_status_t srtp_unprotect_mki(srtp_t ctx,
                                     void *srtp_hdr,
                                     int *len_ptr,
                                     unsigned int use_mki);

srtp_err_status_t srtp_create(srtp_t *session, const srtp_policy_t *policy);

srtp_err_status_t srtp_add_stream(srtp_t session, const srtp_policy_t *policy);

srtp_err_status_t srtp_remove_stream(srtp_t session, unsigned int ssrc);

srtp_err_status_t srtp_update(srtp_t session, const srtp_policy_t *policy);

srtp_err_status_t srtp_update_stream(srtp_t session,
                                     const srtp_policy_t *policy);

void srtp_crypto_policy_set_rtp_default(srtp_crypto_policy_t *p);

void srtp_crypto_policy_set_rtcp_default(srtp_crypto_policy_t *p);

#define srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(p)                      \
    srtp_crypto_policy_set_rtp_default(p)

void srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(srtp_crypto_policy_t *p);

void srtp_crypto_policy_set_aes_cm_128_null_auth(srtp_crypto_policy_t *p);

void srtp_crypto_policy_set_null_cipher_hmac_sha1_80(srtp_crypto_policy_t *p);

void srtp_crypto_policy_set_null_cipher_hmac_null(srtp_crypto_policy_t *p);

void srtp_crypto_policy_set_aes_cm_256_hmac_sha1_80(srtp_crypto_policy_t *p);

void srtp_crypto_policy_set_aes_cm_256_hmac_sha1_32(srtp_crypto_policy_t *p);

void srtp_crypto_policy_set_aes_cm_256_null_auth(srtp_crypto_policy_t *p);

void srtp_crypto_policy_set_aes_cm_192_hmac_sha1_80(srtp_crypto_policy_t *p);

void srtp_crypto_policy_set_aes_cm_192_hmac_sha1_32(srtp_crypto_policy_t *p);

void srtp_crypto_policy_set_aes_cm_192_null_auth(srtp_crypto_policy_t *p);

void srtp_crypto_policy_set_aes_gcm_128_8_auth(srtp_crypto_policy_t *p);

void srtp_crypto_policy_set_aes_gcm_256_8_auth(srtp_crypto_policy_t *p);

void srtp_crypto_policy_set_aes_gcm_128_8_only_auth(srtp_crypto_policy_t *p);

void srtp_crypto_policy_set_aes_gcm_256_8_only_auth(srtp_crypto_policy_t *p);

void srtp_crypto_policy_set_aes_gcm_128_16_auth(srtp_crypto_policy_t *p);

void srtp_crypto_policy_set_aes_gcm_256_16_auth(srtp_crypto_policy_t *p);

srtp_err_status_t srtp_dealloc(srtp_t s);

typedef enum {
    srtp_profile_reserved = 0,
    srtp_profile_aes128_cm_sha1_80 = 1,
    srtp_profile_aes128_cm_sha1_32 = 2,
    srtp_profile_null_sha1_80 = 5,
    srtp_profile_null_sha1_32 = 6,
    srtp_profile_aead_aes_128_gcm = 7,
    srtp_profile_aead_aes_256_gcm = 8,
} srtp_profile_t;

srtp_err_status_t srtp_crypto_policy_set_from_profile_for_rtp(
    srtp_crypto_policy_t *policy,
    srtp_profile_t profile);

srtp_err_status_t srtp_crypto_policy_set_from_profile_for_rtcp(
    srtp_crypto_policy_t *policy,
    srtp_profile_t profile);

unsigned int srtp_profile_get_master_key_length(srtp_profile_t profile);

unsigned int srtp_profile_get_master_salt_length(srtp_profile_t profile);

void srtp_append_salt_to_key(unsigned char *key,
                             unsigned int bytes_in_key,
                             unsigned char *salt,
                             unsigned int bytes_in_salt);

srtp_err_status_t srtp_protect_rtcp(srtp_t ctx,
                                    void *rtcp_hdr,
                                    int *pkt_octet_len);

srtp_err_status_t srtp_protect_rtcp_mki(srtp_t ctx,
                                        void *rtcp_hdr,
                                        int *pkt_octet_len,
                                        unsigned int use_mki,
                                        unsigned int mki_index);

srtp_err_status_t srtp_unprotect_rtcp(srtp_t ctx,
                                      void *srtcp_hdr,
                                      int *pkt_octet_len);

srtp_err_status_t srtp_unprotect_rtcp_mki(srtp_t ctx,
                                          void *srtcp_hdr,
                                          int *pkt_octet_len,
                                          unsigned int use_mki);

void srtp_set_user_data(srtp_t ctx, void *data);

void *srtp_get_user_data(srtp_t ctx);

typedef enum {
    event_ssrc_collision,
    event_key_soft_limit,

    event_key_hard_limit,

    event_packet_index_limit

} srtp_event_t;

typedef struct srtp_event_data_t {
    srtp_t session;
    uint32_t ssrc;

    srtp_event_t event;
} srtp_event_data_t;

typedef void(srtp_event_handler_func_t)(srtp_event_data_t *data);

srtp_err_status_t srtp_install_event_handler(srtp_event_handler_func_t func);

const char *srtp_get_version_string(void);

unsigned int srtp_get_version(void);

srtp_err_status_t srtp_set_debug_module(const char *mod_name, int v);

srtp_err_status_t srtp_list_debug_modules(void);

typedef enum {
    srtp_log_level_error,
    srtp_log_level_warning,
    srtp_log_level_info,
    srtp_log_level_debug
} srtp_log_level_t;

typedef void(srtp_log_handler_func_t)(srtp_log_level_t level,
                                      const char *msg,
                                      void *data);

srtp_err_status_t srtp_install_log_handler(srtp_log_handler_func_t func,
                                           void *data);

srtp_err_status_t srtp_get_protect_trailer_length(srtp_t session,
                                                  uint32_t use_mki,
                                                  uint32_t mki_index,
                                                  uint32_t *length);

srtp_err_status_t srtp_get_protect_rtcp_trailer_length(srtp_t session,
                                                       uint32_t use_mki,
                                                       uint32_t mki_index,
                                                       uint32_t *length);

srtp_err_status_t srtp_set_stream_roc(srtp_t session,
                                      uint32_t ssrc,
                                      uint32_t roc);

srtp_err_status_t srtp_get_stream_roc(srtp_t session,
                                      uint32_t ssrc,
                                      uint32_t *roc);

#define SRTCP_E_BIT 0x80000000

#define SRTCP_E_BYTE_BIT 0x80
#define SRTCP_INDEX_MASK 0x7fffffff

#ifdef __cplusplus
}
#endif

#endif
