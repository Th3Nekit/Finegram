/*
 * cipher.h
 *
 * common interface to ciphers
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

#ifndef SRTP_CIPHER_H
#define SRTP_CIPHER_H

#include "srtp.h"
#include "crypto_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    srtp_direction_encrypt,
    srtp_direction_decrypt,
    srtp_direction_any
} srtp_cipher_direction_t;

typedef struct srtp_cipher_t *srtp_cipher_pointer_t;

typedef srtp_err_status_t (*srtp_cipher_alloc_func_t)(srtp_cipher_pointer_t *cp,
                                                      int key_len,
                                                      int tag_len);

typedef srtp_err_status_t (*srtp_cipher_init_func_t)(void *state,
                                                     const uint8_t *key);

typedef srtp_err_status_t (*srtp_cipher_dealloc_func_t)(
    srtp_cipher_pointer_t cp);

typedef srtp_err_status_t (*srtp_cipher_set_aad_func_t)(void *state,
                                                        const uint8_t *aad,
                                                        uint32_t aad_len);

typedef srtp_err_status_t (*srtp_cipher_encrypt_func_t)(
    void *state,
    uint8_t *buffer,
    unsigned int *octets_to_encrypt);

typedef srtp_err_status_t (*srtp_cipher_decrypt_func_t)(
    void *state,
    uint8_t *buffer,
    unsigned int *octets_to_decrypt);

typedef srtp_err_status_t (*srtp_cipher_set_iv_func_t)(
    void *state,
    uint8_t *iv,
    srtp_cipher_direction_t direction);

typedef srtp_err_status_t (*srtp_cipher_get_tag_func_t)(void *state,
                                                        uint8_t *tag,
                                                        uint32_t *len);

typedef struct srtp_cipher_test_case_t {
    int key_length_octets;
    const uint8_t *key;
    uint8_t *idx;
    unsigned int plaintext_length_octets;
    const uint8_t *plaintext;
    unsigned int ciphertext_length_octets;
    const uint8_t *ciphertext;
    int aad_length_octets;
    const uint8_t *aad;
    int tag_length_octets;
    const struct srtp_cipher_test_case_t
        *next_test_case;
} srtp_cipher_test_case_t;

typedef struct srtp_cipher_type_t {
    srtp_cipher_alloc_func_t alloc;
    srtp_cipher_dealloc_func_t dealloc;
    srtp_cipher_init_func_t init;
    srtp_cipher_set_aad_func_t set_aad;
    srtp_cipher_encrypt_func_t encrypt;
    srtp_cipher_encrypt_func_t decrypt;
    srtp_cipher_set_iv_func_t set_iv;
    srtp_cipher_get_tag_func_t get_tag;
    const char *description;
    const srtp_cipher_test_case_t *test_data;
    srtp_cipher_type_id_t id;
} srtp_cipher_type_t;

typedef struct srtp_cipher_t {
    const srtp_cipher_type_t *type;
    void *state;
    int key_len;
    int algorithm;
} srtp_cipher_t;

int srtp_cipher_get_key_length(const srtp_cipher_t *c);

srtp_err_status_t srtp_cipher_type_self_test(const srtp_cipher_type_t *ct);

srtp_err_status_t srtp_cipher_type_test(
    const srtp_cipher_type_t *ct,
    const srtp_cipher_test_case_t *test_data);

uint64_t srtp_cipher_bits_per_second(srtp_cipher_t *c,
                                     int octets_in_buffer,
                                     int num_trials);

srtp_err_status_t srtp_cipher_type_alloc(const srtp_cipher_type_t *ct,
                                         srtp_cipher_t **c,
                                         int key_len,
                                         int tlen);
srtp_err_status_t srtp_cipher_dealloc(srtp_cipher_t *c);
srtp_err_status_t srtp_cipher_init(srtp_cipher_t *c, const uint8_t *key);
srtp_err_status_t srtp_cipher_set_iv(srtp_cipher_t *c,
                                     uint8_t *iv,
                                     int direction);
srtp_err_status_t srtp_cipher_output(srtp_cipher_t *c,
                                     uint8_t *buffer,
                                     uint32_t *num_octets_to_output);
srtp_err_status_t srtp_cipher_encrypt(srtp_cipher_t *c,
                                      uint8_t *buffer,
                                      uint32_t *num_octets_to_output);
srtp_err_status_t srtp_cipher_decrypt(srtp_cipher_t *c,
                                      uint8_t *buffer,
                                      uint32_t *num_octets_to_output);
srtp_err_status_t srtp_cipher_get_tag(srtp_cipher_t *c,
                                      uint8_t *buffer,
                                      uint32_t *tag_len);
srtp_err_status_t srtp_cipher_set_aad(srtp_cipher_t *c,
                                      const uint8_t *aad,
                                      uint32_t aad_len);

srtp_err_status_t srtp_replace_cipher_type(const srtp_cipher_type_t *ct,
                                           srtp_cipher_type_id_t id);

#ifdef __cplusplus
}
#endif

#endif
