/*
 * ekt.h
 *
 * interface to Encrypted Key Transport for SRTP
 *
 * David McGrew
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

/*
 * EKT implementation strategy
 *
 * use stream_template approach
 *
 * in srtp_unprotect, when a new stream appears, check if template has
 * EKT defined, and if it does, then apply EKT processing
 *
 * question: will we want to allow key-sharing templates in addition
 * to EKT templates?  could define a new ssrc_type_t that's associated
 * with an EKT, e.g.  ssrc_any_ekt.
 *
 *
 */

#ifndef SRTP_EKT_H
#define SRTP_EKT_H

#ifdef __cplusplus
extern "C" {
#endif

#define SRTP_EKT_CIPHER_DEFAULT 1
#define SRTP_EKT_CIPHER_AES_128_ECB 1
#define SRTP_EKT_CIPHER_AES_192_KEY_WRAP 2
#define SRTP_EKT_CIPHER_AES_256_KEY_WRAP 3

typedef uint16_t srtp_ekt_spi_t;

unsigned srtp_ekt_octets_after_base_tag(srtp_ekt_stream_t ekt);

typedef struct srtp_ekt_policy_ctx_t {
    srtp_ekt_spi_t spi;
    uint8_t ekt_cipher_type;
    uint8_t *ekt_key;
    struct srtp_ekt_policy_ctx_t *next_ekt_policy;
} srtp_ekt_policy_ctx_t;

typedef struct srtp_ekt_data_t {
    srtp_ekt_spi_t spi;
    uint8_t ekt_cipher_type;
    srtp_aes_expanded_key_t ekt_enc_key;
    srtp_aes_expanded_key_t ekt_dec_key;
    struct ekt_data_t *next_ekt_data;
} srtp_ekt_data_t;

typedef struct srtp_ekt_stream_ctx_t {
    srtp_ekt_data_t *data;
    uint16_t isn;
    uint8_t encrypted_master_key[SRTP_MAX_KEY_LEN];
} srtp_ekt_stream_ctx_t;

srtp_err_status_t srtp_ekt_alloc(srtp_ekt_stream_t *stream_data,
                                 srtp_ekt_policy_t policy);

srtp_err_status_t srtp_ekt_stream_init(srtp_ekt_stream_t e,
                                       srtp_ekt_spi_t spi,
                                       void *ekt_key,
                                       unsigned ekt_cipher_type);

srtp_err_status_t srtp_ekt_stream_init_from_policy(srtp_ekt_stream_t e,
                                                   srtp_ekt_policy_t p);

srtp_err_status_t srtp_stream_init_from_ekt(srtp_stream_t stream,
                                            const void *srtcp_hdr,
                                            unsigned pkt_octet_len);

void srtp_ekt_write_data(srtp_ekt_stream_t ekt,
                         uint8_t *base_tag,
                         unsigned base_tag_len,
                         int *packet_len,
                         srtp_xtd_seq_num_t pkt_index);

srtp_err_status_t srtp_ekt_tag_verification_preproces(uint8_t *pkt_tag,
                                                      uint8_t *pkt_tag_copy,
                                                      unsigned tag_len);

srtp_err_status_t srtp_ekt_tag_verification_postproces(uint8_t *pkt_tag,
                                                       uint8_t *pkt_tag_copy,
                                                       unsigned tag_len);

srtp_err_status_t srtp_stream_srtcp_auth_tag_generation_preprocess(
    const srtp_stream_t *s,
    uint8_t *pkt_tag,
    unsigned pkt_octet_len);

srtp_err_status_t srtcp_auth_tag_generation_postprocess(void);

#ifdef __cplusplus
}
#endif

#endif
