/*
 * crypto_kernel.h
 *
 * header for the cryptographic kernel
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */
/*
 *
 * Copyright(c) 2001-2017 Cisco Systems, Inc.
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

#ifndef CRYPTO_KERNEL
#define CRYPTO_KERNEL

#include "cipher.h"
#include "auth.h"
#include "err.h"
#include "crypto_types.h"
#include "key.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    srtp_crypto_kernel_state_insecure,
    srtp_crypto_kernel_state_secure
} srtp_crypto_kernel_state_t;

typedef struct srtp_kernel_cipher_type {
    srtp_cipher_type_id_t id;
    const srtp_cipher_type_t *cipher_type;
    struct srtp_kernel_cipher_type *next;
} srtp_kernel_cipher_type_t;

typedef struct srtp_kernel_auth_type {
    srtp_auth_type_id_t id;
    const srtp_auth_type_t *auth_type;
    struct srtp_kernel_auth_type *next;
} srtp_kernel_auth_type_t;

typedef struct srtp_kernel_debug_module {
    srtp_debug_module_t *mod;
    struct srtp_kernel_debug_module *next;
} srtp_kernel_debug_module_t;

typedef struct {
    srtp_crypto_kernel_state_t state;
    srtp_kernel_cipher_type_t *cipher_type_list;
    srtp_kernel_auth_type_t *auth_type_list;
    srtp_kernel_debug_module_t
        *debug_module_list;
} srtp_crypto_kernel_t;

srtp_err_status_t srtp_crypto_kernel_init(void);

srtp_err_status_t srtp_crypto_kernel_shutdown(void);

srtp_err_status_t srtp_crypto_kernel_status(void);

srtp_err_status_t srtp_crypto_kernel_list_debug_modules(void);

srtp_err_status_t srtp_crypto_kernel_load_cipher_type(
    const srtp_cipher_type_t *ct,
    srtp_cipher_type_id_t id);

srtp_err_status_t srtp_crypto_kernel_load_auth_type(const srtp_auth_type_t *ct,
                                                    srtp_auth_type_id_t id);

srtp_err_status_t srtp_crypto_kernel_load_debug_module(
    srtp_debug_module_t *new_dm);

srtp_err_status_t srtp_crypto_kernel_alloc_cipher(srtp_cipher_type_id_t id,
                                                  srtp_cipher_pointer_t *cp,
                                                  int key_len,
                                                  int tag_len);

srtp_err_status_t srtp_crypto_kernel_alloc_auth(srtp_auth_type_id_t id,
                                                srtp_auth_pointer_t *ap,
                                                int key_len,
                                                int tag_len);

srtp_err_status_t srtp_crypto_kernel_set_debug_module(const char *mod_name,
                                                      int v);

#ifdef __cplusplus
}
#endif

#endif
