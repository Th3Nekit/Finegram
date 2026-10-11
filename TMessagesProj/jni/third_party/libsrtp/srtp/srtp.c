/*
 * srtp.c
 *
 * the secure real-time transport protocol
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

// Leave this as the top level import. Ensures the existence of defines
#include "config.h"

#include "srtp_priv.h"
#include "crypto_types.h"
#include "err.h"
#include "ekt.h"
#include "alloc.h"

#ifdef OPENSSL
#include "aes_gcm_ossl.h"
#ifdef OPENSSL_KDF
#include <openssl/kdf.h>
#include "aes_icm_ossl.h"
#endif
#endif

#include <limits.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#elif defined(HAVE_WINSOCK2_H)
#include <winsock2.h>
#endif

srtp_debug_module_t mod_srtp = {
    0,
    "srtp"
};

#define octets_in_rtp_header 12
#define uint32s_in_rtp_header 3
#define octets_in_rtcp_header 8
#define uint32s_in_rtcp_header 2
#define octets_in_rtp_extn_hdr 4

static srtp_err_status_t srtp_validate_rtp_header(void *rtp_hdr,
                                                  int *pkt_octet_len)
{
    if (*pkt_octet_len < octets_in_rtp_header)
        return srtp_err_status_bad_param;

    srtp_hdr_t *hdr = (srtp_hdr_t *)rtp_hdr;

    int rtp_header_len = octets_in_rtp_header + 4 * hdr->cc;
    if (hdr->x == 1)
        rtp_header_len += octets_in_rtp_extn_hdr;

    if (*pkt_octet_len < rtp_header_len)
        return srtp_err_status_bad_param;

    if (hdr->x == 1) {
        srtp_hdr_xtnd_t *xtn_hdr =
            (srtp_hdr_xtnd_t *)((uint32_t *)hdr + uint32s_in_rtp_header +
                                hdr->cc);
        int profile_len = ntohs(xtn_hdr->length);
        rtp_header_len += profile_len * 4;

        if (*pkt_octet_len < rtp_header_len)
            return srtp_err_status_bad_param;
    }
    return srtp_err_status_ok;
}

const char *srtp_get_version_string()
{

    return SRTP_VER_STRING;
}

unsigned int srtp_get_version()
{
    unsigned int major = 0, minor = 0, micro = 0;
    unsigned int rv = 0;
    int parse_rv;

    parse_rv = sscanf(SRTP_VERSION, "%u.%u.%u", &major, &minor, &micro);
    if (parse_rv != 3) {

        return (0);
    }

    rv |= (major & 0xFF) << 24;
    rv |= (minor & 0xFF) << 16;
    rv |= micro & 0xFF;
    return rv;
}

srtp_err_status_t srtp_stream_dealloc(srtp_stream_ctx_t *stream,
                                      const srtp_stream_ctx_t *stream_template)
{
    srtp_err_status_t status;
    unsigned int i = 0;
    srtp_session_keys_t *session_keys = NULL;
    srtp_session_keys_t *template_session_keys = NULL;

    if (stream->session_keys) {
        for (i = 0; i < stream->num_master_keys; i++) {
            session_keys = &stream->session_keys[i];

            if (stream_template &&
                stream->num_master_keys == stream_template->num_master_keys) {
                template_session_keys = &stream_template->session_keys[i];
            } else {
                template_session_keys = NULL;
            }

            if (template_session_keys &&
                session_keys->rtp_cipher == template_session_keys->rtp_cipher) {

            } else if (session_keys->rtp_cipher) {
                status = srtp_cipher_dealloc(session_keys->rtp_cipher);
                if (status)
                    return status;
            }

            if (template_session_keys &&
                session_keys->rtp_auth == template_session_keys->rtp_auth) {

            } else if (session_keys->rtp_auth) {
                status = srtp_auth_dealloc(session_keys->rtp_auth);
                if (status)
                    return status;
            }

            if (template_session_keys &&
                session_keys->rtp_xtn_hdr_cipher ==
                    template_session_keys->rtp_xtn_hdr_cipher) {

            } else if (session_keys->rtp_xtn_hdr_cipher) {
                status = srtp_cipher_dealloc(session_keys->rtp_xtn_hdr_cipher);
                if (status)
                    return status;
            }

            if (template_session_keys &&
                session_keys->rtcp_cipher ==
                    template_session_keys->rtcp_cipher) {

            } else if (session_keys->rtcp_cipher) {
                status = srtp_cipher_dealloc(session_keys->rtcp_cipher);
                if (status)
                    return status;
            }

            if (template_session_keys &&
                session_keys->rtcp_auth == template_session_keys->rtcp_auth) {

            } else if (session_keys->rtcp_auth) {
                status = srtp_auth_dealloc(session_keys->rtcp_auth);
                if (status)
                    return status;
            }

            octet_string_set_to_zero(session_keys->salt, SRTP_AEAD_SALT_LEN);
            octet_string_set_to_zero(session_keys->c_salt, SRTP_AEAD_SALT_LEN);

            if (session_keys->mki_id) {
                octet_string_set_to_zero(session_keys->mki_id,
                                         session_keys->mki_size);
                srtp_crypto_free(session_keys->mki_id);
                session_keys->mki_id = NULL;
            }

            if (template_session_keys &&
                session_keys->limit == template_session_keys->limit) {

            } else if (session_keys->limit) {
                srtp_crypto_free(session_keys->limit);
            }
        }
        srtp_crypto_free(stream->session_keys);
    }

    status = srtp_rdbx_dealloc(&stream->rtp_rdbx);
    if (status)
        return status;

    if (stream_template &&
        stream->enc_xtn_hdr == stream_template->enc_xtn_hdr) {

    } else if (stream->enc_xtn_hdr) {
        srtp_crypto_free(stream->enc_xtn_hdr);
    }

    srtp_crypto_free(stream);

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_stream_alloc(srtp_stream_ctx_t **str_ptr,
                                    const srtp_policy_t *p)
{
    srtp_stream_ctx_t *str;
    srtp_err_status_t stat;
    unsigned int i = 0;
    srtp_session_keys_t *session_keys = NULL;

    str = (srtp_stream_ctx_t *)srtp_crypto_alloc(sizeof(srtp_stream_ctx_t));
    if (str == NULL)
        return srtp_err_status_alloc_fail;

    *str_ptr = str;

    if (p->key != NULL) {
        str->num_master_keys = 1;
    } else {
        str->num_master_keys = p->num_master_keys;
    }

    str->session_keys = (srtp_session_keys_t *)srtp_crypto_alloc(
        sizeof(srtp_session_keys_t) * str->num_master_keys);

    if (str->session_keys == NULL) {
        srtp_stream_dealloc(str, NULL);
        return srtp_err_status_alloc_fail;
    }

    for (i = 0; i < str->num_master_keys; i++) {
        session_keys = &str->session_keys[i];

        stat = srtp_crypto_kernel_alloc_cipher(
            p->rtp.cipher_type, &session_keys->rtp_cipher,
            p->rtp.cipher_key_len, p->rtp.auth_tag_len);
        if (stat) {
            srtp_stream_dealloc(str, NULL);
            return stat;
        }

        stat = srtp_crypto_kernel_alloc_auth(
            p->rtp.auth_type, &session_keys->rtp_auth, p->rtp.auth_key_len,
            p->rtp.auth_tag_len);
        if (stat) {
            srtp_stream_dealloc(str, NULL);
            return stat;
        }

        stat = srtp_crypto_kernel_alloc_cipher(
            p->rtcp.cipher_type, &session_keys->rtcp_cipher,
            p->rtcp.cipher_key_len, p->rtcp.auth_tag_len);
        if (stat) {
            srtp_stream_dealloc(str, NULL);
            return stat;
        }

        stat = srtp_crypto_kernel_alloc_auth(
            p->rtcp.auth_type, &session_keys->rtcp_auth, p->rtcp.auth_key_len,
            p->rtcp.auth_tag_len);
        if (stat) {
            srtp_stream_dealloc(str, NULL);
            return stat;
        }

        session_keys->mki_id = NULL;

        session_keys->limit = (srtp_key_limit_ctx_t *)srtp_crypto_alloc(
            sizeof(srtp_key_limit_ctx_t));
        if (session_keys->limit == NULL) {
            srtp_stream_dealloc(str, NULL);
            return srtp_err_status_alloc_fail;
        }
    }

    stat = srtp_ekt_alloc(&str->ekt, p->ekt);
    if (stat) {
        srtp_stream_dealloc(str, NULL);
        return stat;
    }

    if (p->enc_xtn_hdr && p->enc_xtn_hdr_count > 0) {
        srtp_cipher_type_id_t enc_xtn_hdr_cipher_type;
        int enc_xtn_hdr_cipher_key_len;

        str->enc_xtn_hdr = (int *)srtp_crypto_alloc(p->enc_xtn_hdr_count *
                                                    sizeof(p->enc_xtn_hdr[0]));
        if (!str->enc_xtn_hdr) {
            srtp_stream_dealloc(str, NULL);
            return srtp_err_status_alloc_fail;
        }
        memcpy(str->enc_xtn_hdr, p->enc_xtn_hdr,
               p->enc_xtn_hdr_count * sizeof(p->enc_xtn_hdr[0]));
        str->enc_xtn_hdr_count = p->enc_xtn_hdr_count;

        switch (p->rtp.cipher_type) {
        case SRTP_AES_GCM_128:
            enc_xtn_hdr_cipher_type = SRTP_AES_ICM_128;
            enc_xtn_hdr_cipher_key_len = SRTP_AES_ICM_128_KEY_LEN_WSALT;
            break;
        case SRTP_AES_GCM_256:
            enc_xtn_hdr_cipher_type = SRTP_AES_ICM_256;
            enc_xtn_hdr_cipher_key_len = SRTP_AES_ICM_256_KEY_LEN_WSALT;
            break;
        default:
            enc_xtn_hdr_cipher_type = p->rtp.cipher_type;
            enc_xtn_hdr_cipher_key_len = p->rtp.cipher_key_len;
            break;
        }

        for (i = 0; i < str->num_master_keys; i++) {
            session_keys = &str->session_keys[i];

            stat = srtp_crypto_kernel_alloc_cipher(
                enc_xtn_hdr_cipher_type, &session_keys->rtp_xtn_hdr_cipher,
                enc_xtn_hdr_cipher_key_len, 0);
            if (stat) {
                srtp_stream_dealloc(str, NULL);
                return stat;
            }
        }
    } else {
        for (i = 0; i < str->num_master_keys; i++) {
            session_keys = &str->session_keys[i];
            session_keys->rtp_xtn_hdr_cipher = NULL;
        }

        str->enc_xtn_hdr = NULL;
        str->enc_xtn_hdr_count = 0;
    }

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_stream_clone(const srtp_stream_ctx_t *stream_template,
                                    uint32_t ssrc,
                                    srtp_stream_ctx_t **str_ptr)
{
    srtp_err_status_t status;
    srtp_stream_ctx_t *str;
    unsigned int i = 0;
    srtp_session_keys_t *session_keys = NULL;
    const srtp_session_keys_t *template_session_keys = NULL;

    debug_print(mod_srtp, "cloning stream (SSRC: 0x%08x)", ntohl(ssrc));

    str = (srtp_stream_ctx_t *)srtp_crypto_alloc(sizeof(srtp_stream_ctx_t));
    if (str == NULL)
        return srtp_err_status_alloc_fail;
    *str_ptr = str;

    str->num_master_keys = stream_template->num_master_keys;
    str->session_keys = (srtp_session_keys_t *)srtp_crypto_alloc(
        sizeof(srtp_session_keys_t) * str->num_master_keys);

    if (str->session_keys == NULL) {
        srtp_stream_dealloc(*str_ptr, stream_template);
        *str_ptr = NULL;
        return srtp_err_status_alloc_fail;
    }

    for (i = 0; i < stream_template->num_master_keys; i++) {
        session_keys = &str->session_keys[i];
        template_session_keys = &stream_template->session_keys[i];

        session_keys->rtp_cipher = template_session_keys->rtp_cipher;
        session_keys->rtp_auth = template_session_keys->rtp_auth;
        session_keys->rtp_xtn_hdr_cipher =
            template_session_keys->rtp_xtn_hdr_cipher;
        session_keys->rtcp_cipher = template_session_keys->rtcp_cipher;
        session_keys->rtcp_auth = template_session_keys->rtcp_auth;
        session_keys->mki_size = template_session_keys->mki_size;

        if (template_session_keys->mki_size == 0) {
            session_keys->mki_id = NULL;
        } else {
            session_keys->mki_id =
                srtp_crypto_alloc(template_session_keys->mki_size);

            if (session_keys->mki_id == NULL) {
                srtp_stream_dealloc(*str_ptr, stream_template);
                *str_ptr = NULL;
                return srtp_err_status_init_fail;
            }
            memcpy(session_keys->mki_id, template_session_keys->mki_id,
                   session_keys->mki_size);
        }

        memcpy(session_keys->salt, template_session_keys->salt,
               SRTP_AEAD_SALT_LEN);
        memcpy(session_keys->c_salt, template_session_keys->c_salt,
               SRTP_AEAD_SALT_LEN);

        status = srtp_key_limit_clone(template_session_keys->limit,
                                      &session_keys->limit);
        if (status) {
            srtp_stream_dealloc(*str_ptr, stream_template);
            *str_ptr = NULL;
            return status;
        }
    }

    status = srtp_rdbx_init(
        &str->rtp_rdbx, srtp_rdbx_get_window_size(&stream_template->rtp_rdbx));
    if (status) {
        srtp_stream_dealloc(*str_ptr, stream_template);
        *str_ptr = NULL;
        return status;
    }
    srtp_rdb_init(&str->rtcp_rdb);
    str->allow_repeat_tx = stream_template->allow_repeat_tx;

    str->ssrc = ssrc;

    str->pending_roc = 0;

    str->direction = stream_template->direction;
    str->rtp_services = stream_template->rtp_services;
    str->rtcp_services = stream_template->rtcp_services;

    str->ekt = stream_template->ekt;

    str->enc_xtn_hdr = stream_template->enc_xtn_hdr;
    str->enc_xtn_hdr_count = stream_template->enc_xtn_hdr_count;

    str->next = NULL;
    return srtp_err_status_ok;
}

typedef enum {
    label_rtp_encryption = 0x00,
    label_rtp_msg_auth = 0x01,
    label_rtp_salt = 0x02,
    label_rtcp_encryption = 0x03,
    label_rtcp_msg_auth = 0x04,
    label_rtcp_salt = 0x05,
    label_rtp_header_encryption = 0x06,
    label_rtp_header_salt = 0x07
} srtp_prf_label;

#define MAX_SRTP_KEY_LEN 256

#if defined(OPENSSL) && defined(OPENSSL_KDF)
#define MAX_SRTP_AESKEY_LEN 32
#define MAX_SRTP_SALT_LEN 14

typedef struct {
    uint8_t master_key[MAX_SRTP_AESKEY_LEN];
    uint8_t master_salt[MAX_SRTP_SALT_LEN];
    const EVP_CIPHER *evp;
} srtp_kdf_t;

static srtp_err_status_t srtp_kdf_init(srtp_kdf_t *kdf,
                                       const uint8_t *key,
                                       int key_len,
                                       int salt_len)
{
    memset(kdf, 0x0, sizeof(srtp_kdf_t));

    if (key_len == 0)
        return srtp_err_status_ok;

    if ((key_len > MAX_SRTP_AESKEY_LEN) || (salt_len > MAX_SRTP_SALT_LEN)) {
        return srtp_err_status_bad_param;
    }
    switch (key_len) {
    case SRTP_AES_256_KEYSIZE:
        kdf->evp = EVP_aes_256_ctr();
        break;
    case SRTP_AES_192_KEYSIZE:
        kdf->evp = EVP_aes_192_ctr();
        break;
    case SRTP_AES_128_KEYSIZE:
        kdf->evp = EVP_aes_128_ctr();
        break;
    default:
        return srtp_err_status_bad_param;
        break;
    }
    memcpy(kdf->master_key, key, key_len);
    memcpy(kdf->master_salt, key + key_len, salt_len);
    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_kdf_generate(srtp_kdf_t *kdf,
                                           srtp_prf_label label,
                                           uint8_t *key,
                                           unsigned int length)
{
    int ret;

    if (!kdf->evp)
        return srtp_err_status_ok;
    octet_string_set_to_zero(key, length);

    ret = kdf_srtp(kdf->evp, (char *)&kdf->master_key,
                   (char *)&kdf->master_salt, NULL, NULL, label, (char *)key);
    if (ret == -1) {
        return (srtp_err_status_algo_fail);
    }

    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_kdf_clear(srtp_kdf_t *kdf)
{
    octet_string_set_to_zero(kdf->master_key, MAX_SRTP_AESKEY_LEN);
    octet_string_set_to_zero(kdf->master_salt, MAX_SRTP_SALT_LEN);
    kdf->evp = NULL;

    return srtp_err_status_ok;
}

#else

typedef struct {
    srtp_cipher_t *cipher;
} srtp_kdf_t;

static srtp_err_status_t srtp_kdf_init(srtp_kdf_t *kdf,
                                       const uint8_t *key,
                                       int key_len)
{
    srtp_cipher_type_id_t cipher_id;
    switch (key_len) {
    case SRTP_AES_ICM_256_KEY_LEN_WSALT:
        cipher_id = SRTP_AES_ICM_256;
        break;
    case SRTP_AES_ICM_192_KEY_LEN_WSALT:
        cipher_id = SRTP_AES_ICM_192;
        break;
    case SRTP_AES_ICM_128_KEY_LEN_WSALT:
        cipher_id = SRTP_AES_ICM_128;
        break;
    default:
        return srtp_err_status_bad_param;
        break;
    }

    srtp_err_status_t stat;
    stat = srtp_crypto_kernel_alloc_cipher(cipher_id, &kdf->cipher, key_len, 0);
    if (stat)
        return stat;

    stat = srtp_cipher_init(kdf->cipher, key);
    if (stat) {
        srtp_cipher_dealloc(kdf->cipher);
        return stat;
    }
    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_kdf_generate(srtp_kdf_t *kdf,
                                           srtp_prf_label label,
                                           uint8_t *key,
                                           unsigned int length)
{
    srtp_err_status_t status;
    v128_t nonce;

    v128_set_to_zero(&nonce);
    nonce.v8[7] = label;

    status = srtp_cipher_set_iv(kdf->cipher, (uint8_t *)&nonce,
                                srtp_direction_encrypt);
    if (status)
        return status;

    octet_string_set_to_zero(key, length);
    status = srtp_cipher_encrypt(kdf->cipher, key, &length);
    if (status)
        return status;

    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_kdf_clear(srtp_kdf_t *kdf)
{
    srtp_err_status_t status;
    status = srtp_cipher_dealloc(kdf->cipher);
    if (status)
        return status;
    kdf->cipher = NULL;
    return srtp_err_status_ok;
}
#endif

static inline int base_key_length(const srtp_cipher_type_t *cipher,
                                  int key_length)
{
    switch (cipher->id) {
    case SRTP_AES_ICM_128:
    case SRTP_AES_ICM_192:
    case SRTP_AES_ICM_256:

        return key_length - SRTP_SALT_LEN;
        break;
    case SRTP_AES_GCM_128:
        return key_length - SRTP_AEAD_SALT_LEN;
        break;
    case SRTP_AES_GCM_256:
        return key_length - SRTP_AEAD_SALT_LEN;
        break;
    default:
        return key_length;
        break;
    }
}

unsigned int srtp_validate_policy_master_keys(const srtp_policy_t *policy)
{
    unsigned long i = 0;

    if (policy->key == NULL) {
        if (policy->num_master_keys <= 0)
            return 0;

        if (policy->num_master_keys > SRTP_MAX_NUM_MASTER_KEYS)
            return 0;

        for (i = 0; i < policy->num_master_keys; i++) {
            if (policy->keys[i]->key == NULL)
                return 0;
            if (policy->keys[i]->mki_size > SRTP_MAX_MKI_LEN)
                return 0;
        }
    }

    return 1;
}

srtp_session_keys_t *srtp_get_session_keys_with_mki_index(
    srtp_stream_ctx_t *stream,
    unsigned int use_mki,
    unsigned int mki_index)
{
    if (use_mki) {
        if (mki_index >= stream->num_master_keys) {
            return NULL;
        }
        return &stream->session_keys[mki_index];
    }

    return &stream->session_keys[0];
}

unsigned int srtp_inject_mki(uint8_t *mki_tag_location,
                             srtp_session_keys_t *session_keys,
                             unsigned int use_mki)
{
    unsigned int mki_size = 0;

    if (use_mki) {
        mki_size = session_keys->mki_size;

        if (mki_size != 0) {

            memcpy(mki_tag_location, session_keys->mki_id, mki_size);
        }
    }

    return mki_size;
}

srtp_err_status_t srtp_stream_init_all_master_keys(
    srtp_stream_ctx_t *srtp,
    unsigned char *key,
    srtp_master_key_t **keys,
    const unsigned int max_master_keys)
{
    unsigned int i = 0;
    srtp_err_status_t status = srtp_err_status_ok;
    srtp_master_key_t single_master_key;

    if (key != NULL) {
        srtp->num_master_keys = 1;
        single_master_key.key = key;
        single_master_key.mki_id = NULL;
        single_master_key.mki_size = 0;
        status = srtp_stream_init_keys(srtp, &single_master_key, 0);
    } else {
        srtp->num_master_keys = max_master_keys;

        for (i = 0; i < srtp->num_master_keys && i < SRTP_MAX_NUM_MASTER_KEYS;
             i++) {
            status = srtp_stream_init_keys(srtp, keys[i], i);

            if (status) {
                return status;
            }
        }
    }

    return status;
}

srtp_err_status_t srtp_stream_init_keys(srtp_stream_ctx_t *srtp,
                                        srtp_master_key_t *master_key,
                                        const unsigned int current_mki_index)
{
    srtp_err_status_t stat;
    srtp_kdf_t kdf;
    uint8_t tmp_key[MAX_SRTP_KEY_LEN];
    int kdf_keylen = 30, rtp_keylen, rtcp_keylen;
    int rtp_base_key_len, rtp_salt_len;
    int rtcp_base_key_len, rtcp_salt_len;
    srtp_session_keys_t *session_keys = NULL;
    unsigned char *key = master_key->key;

    session_keys = &srtp->session_keys[current_mki_index];

#ifdef NO_64BIT_MATH
    {
        uint64_t temp;
        temp = make64(UINT_MAX, UINT_MAX);
        srtp_key_limit_set(session_keys->limit, temp);
    }
#else
    srtp_key_limit_set(session_keys->limit, 0xffffffffffffLL);
#endif

    if (master_key->mki_size != 0) {
        session_keys->mki_id = srtp_crypto_alloc(master_key->mki_size);

        if (session_keys->mki_id == NULL) {
            return srtp_err_status_init_fail;
        }
        memcpy(session_keys->mki_id, master_key->mki_id, master_key->mki_size);
    } else {
        session_keys->mki_id = NULL;
    }

    session_keys->mki_size = master_key->mki_size;

    rtp_keylen = srtp_cipher_get_key_length(session_keys->rtp_cipher);
    rtcp_keylen = srtp_cipher_get_key_length(session_keys->rtcp_cipher);
    rtp_base_key_len =
        base_key_length(session_keys->rtp_cipher->type, rtp_keylen);
    rtp_salt_len = rtp_keylen - rtp_base_key_len;

    if (rtp_keylen > kdf_keylen) {
        kdf_keylen = 46;
    }

    if (rtcp_keylen > kdf_keylen) {
        kdf_keylen = 46;
    }

    debug_print(mod_srtp, "srtp key len: %d", rtp_keylen);
    debug_print(mod_srtp, "srtcp key len: %d", rtcp_keylen);
    debug_print(mod_srtp, "base key len: %d", rtp_base_key_len);
    debug_print(mod_srtp, "kdf key len: %d", kdf_keylen);
    debug_print(mod_srtp, "rtp salt len: %d", rtp_salt_len);

    memset(tmp_key, 0x0, MAX_SRTP_KEY_LEN);
    memcpy(tmp_key, key, (rtp_base_key_len + rtp_salt_len));

#if defined(OPENSSL) && defined(OPENSSL_KDF)
    stat = srtp_kdf_init(&kdf, (const uint8_t *)tmp_key, rtp_base_key_len,
                         rtp_salt_len);
#else
    stat = srtp_kdf_init(&kdf, (const uint8_t *)tmp_key, kdf_keylen);
#endif
    if (stat) {

        octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
        return srtp_err_status_init_fail;
    }

    stat = srtp_kdf_generate(&kdf, label_rtp_encryption, tmp_key,
                             rtp_base_key_len);
    if (stat) {

        octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
        return srtp_err_status_init_fail;
    }
    debug_print(mod_srtp, "cipher key: %s",
                srtp_octet_string_hex_string(tmp_key, rtp_base_key_len));

    if (rtp_salt_len > 0) {
        debug_print(mod_srtp, "found rtp_salt_len > 0, generating salt", NULL);

        stat = srtp_kdf_generate(&kdf, label_rtp_salt,
                                 tmp_key + rtp_base_key_len, rtp_salt_len);
        if (stat) {

            octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
            return srtp_err_status_init_fail;
        }
        memcpy(session_keys->salt, tmp_key + rtp_base_key_len,
               SRTP_AEAD_SALT_LEN);
    }
    if (rtp_salt_len > 0) {
        debug_print(mod_srtp, "cipher salt: %s",
                    srtp_octet_string_hex_string(tmp_key + rtp_base_key_len,
                                                 rtp_salt_len));
    }

    stat = srtp_cipher_init(session_keys->rtp_cipher, tmp_key);
    if (stat) {

        octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
        return srtp_err_status_init_fail;
    }

    if (session_keys->rtp_xtn_hdr_cipher) {

        int rtp_xtn_hdr_keylen;
        int rtp_xtn_hdr_base_key_len;
        int rtp_xtn_hdr_salt_len;
        srtp_kdf_t tmp_kdf;
        srtp_kdf_t *xtn_hdr_kdf;

        if (session_keys->rtp_xtn_hdr_cipher->type !=
            session_keys->rtp_cipher->type) {

            uint8_t tmp_xtn_hdr_key[MAX_SRTP_KEY_LEN];
            rtp_xtn_hdr_keylen =
                srtp_cipher_get_key_length(session_keys->rtp_xtn_hdr_cipher);
            rtp_xtn_hdr_base_key_len = base_key_length(
                session_keys->rtp_xtn_hdr_cipher->type, rtp_xtn_hdr_keylen);
            rtp_xtn_hdr_salt_len =
                rtp_xtn_hdr_keylen - rtp_xtn_hdr_base_key_len;
            if (rtp_xtn_hdr_salt_len > rtp_salt_len) {
                switch (session_keys->rtp_cipher->type->id) {
                case SRTP_AES_GCM_128:
                case SRTP_AES_GCM_256:

                    rtp_xtn_hdr_salt_len = rtp_salt_len;
                    break;
                default:

                    octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
                    return srtp_err_status_bad_param;
                }
            }
            memset(tmp_xtn_hdr_key, 0x0, MAX_SRTP_KEY_LEN);
            memcpy(tmp_xtn_hdr_key, key,
                   (rtp_xtn_hdr_base_key_len + rtp_xtn_hdr_salt_len));
            xtn_hdr_kdf = &tmp_kdf;

#if defined(OPENSSL) && defined(OPENSSL_KDF)
            stat =
                srtp_kdf_init(xtn_hdr_kdf, (const uint8_t *)tmp_xtn_hdr_key,
                              rtp_xtn_hdr_base_key_len, rtp_xtn_hdr_salt_len);
#else
            stat = srtp_kdf_init(xtn_hdr_kdf, (const uint8_t *)tmp_xtn_hdr_key,
                                 kdf_keylen);
#endif
            octet_string_set_to_zero(tmp_xtn_hdr_key, MAX_SRTP_KEY_LEN);
            if (stat) {

                octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
                return srtp_err_status_init_fail;
            }
        } else {

            rtp_xtn_hdr_keylen = rtp_keylen;
            rtp_xtn_hdr_base_key_len = rtp_base_key_len;
            rtp_xtn_hdr_salt_len = rtp_salt_len;
            xtn_hdr_kdf = &kdf;
        }

        stat = srtp_kdf_generate(xtn_hdr_kdf, label_rtp_header_encryption,
                                 tmp_key, rtp_xtn_hdr_base_key_len);
        if (stat) {

            octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
            return srtp_err_status_init_fail;
        }
        debug_print(
            mod_srtp, "extensions cipher key: %s",
            srtp_octet_string_hex_string(tmp_key, rtp_xtn_hdr_base_key_len));

        if (rtp_xtn_hdr_salt_len > 0) {
            debug_print(mod_srtp,
                        "found rtp_xtn_hdr_salt_len > 0, generating salt",
                        NULL);

            stat = srtp_kdf_generate(xtn_hdr_kdf, label_rtp_header_salt,
                                     tmp_key + rtp_xtn_hdr_base_key_len,
                                     rtp_xtn_hdr_salt_len);
            if (stat) {

                octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
                return srtp_err_status_init_fail;
            }
        }
        if (rtp_xtn_hdr_salt_len > 0) {
            debug_print(
                mod_srtp, "extensions cipher salt: %s",
                srtp_octet_string_hex_string(tmp_key + rtp_xtn_hdr_base_key_len,
                                             rtp_xtn_hdr_salt_len));
        }

        stat = srtp_cipher_init(session_keys->rtp_xtn_hdr_cipher, tmp_key);
        if (stat) {

            octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
            return srtp_err_status_init_fail;
        }

        if (xtn_hdr_kdf != &kdf) {

            stat = srtp_kdf_clear(xtn_hdr_kdf);
            if (stat) {

                octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
                return srtp_err_status_init_fail;
            }
        }
    }

    stat = srtp_kdf_generate(&kdf, label_rtp_msg_auth, tmp_key,
                             srtp_auth_get_key_length(session_keys->rtp_auth));
    if (stat) {

        octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
        return srtp_err_status_init_fail;
    }
    debug_print(mod_srtp, "auth key:   %s",
                srtp_octet_string_hex_string(
                    tmp_key, srtp_auth_get_key_length(session_keys->rtp_auth)));

    stat = srtp_auth_init(session_keys->rtp_auth, tmp_key);
    if (stat) {

        octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
        return srtp_err_status_init_fail;
    }

    rtcp_base_key_len =
        base_key_length(session_keys->rtcp_cipher->type, rtcp_keylen);
    rtcp_salt_len = rtcp_keylen - rtcp_base_key_len;
    debug_print(mod_srtp, "rtcp salt len: %d", rtcp_salt_len);

    stat = srtp_kdf_generate(&kdf, label_rtcp_encryption, tmp_key,
                             rtcp_base_key_len);
    if (stat) {

        octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
        return srtp_err_status_init_fail;
    }

    if (rtcp_salt_len > 0) {
        debug_print(mod_srtp, "found rtcp_salt_len > 0, generating rtcp salt",
                    NULL);

        stat = srtp_kdf_generate(&kdf, label_rtcp_salt,
                                 tmp_key + rtcp_base_key_len, rtcp_salt_len);
        if (stat) {

            octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
            return srtp_err_status_init_fail;
        }
        memcpy(session_keys->c_salt, tmp_key + rtcp_base_key_len,
               SRTP_AEAD_SALT_LEN);
    }
    debug_print(mod_srtp, "rtcp cipher key: %s",
                srtp_octet_string_hex_string(tmp_key, rtcp_base_key_len));
    if (rtcp_salt_len > 0) {
        debug_print(mod_srtp, "rtcp cipher salt: %s",
                    srtp_octet_string_hex_string(tmp_key + rtcp_base_key_len,
                                                 rtcp_salt_len));
    }

    stat = srtp_cipher_init(session_keys->rtcp_cipher, tmp_key);
    if (stat) {

        octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
        return srtp_err_status_init_fail;
    }

    stat = srtp_kdf_generate(&kdf, label_rtcp_msg_auth, tmp_key,
                             srtp_auth_get_key_length(session_keys->rtcp_auth));
    if (stat) {

        octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
        return srtp_err_status_init_fail;
    }

    debug_print(
        mod_srtp, "rtcp auth key:   %s",
        srtp_octet_string_hex_string(
            tmp_key, srtp_auth_get_key_length(session_keys->rtcp_auth)));

    stat = srtp_auth_init(session_keys->rtcp_auth, tmp_key);
    if (stat) {

        octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
        return srtp_err_status_init_fail;
    }

    stat = srtp_kdf_clear(&kdf);
    octet_string_set_to_zero(tmp_key, MAX_SRTP_KEY_LEN);
    if (stat)
        return srtp_err_status_init_fail;

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_stream_init(srtp_stream_ctx_t *srtp,
                                   const srtp_policy_t *p)
{
    srtp_err_status_t err;

    debug_print(mod_srtp, "initializing stream (SSRC: 0x%08x)", p->ssrc.value);

    if (p->window_size != 0 &&
        (p->window_size < 64 || p->window_size >= 0x8000))
        return srtp_err_status_bad_param;

    if (p->window_size != 0)
        err = srtp_rdbx_init(&srtp->rtp_rdbx, p->window_size);
    else
        err = srtp_rdbx_init(&srtp->rtp_rdbx, 128);
    if (err)
        return err;

    srtp->ssrc = htonl(p->ssrc.value);

    srtp->pending_roc = 0;

    srtp->rtp_services = p->rtp.sec_serv;
    srtp->rtcp_services = p->rtcp.sec_serv;

    srtp->direction = dir_unknown;

    srtp_rdb_init(&srtp->rtcp_rdb);

    if (p->allow_repeat_tx != 0 && p->allow_repeat_tx != 1) {
        srtp_rdbx_dealloc(&srtp->rtp_rdbx);
        return srtp_err_status_bad_param;
    }
    srtp->allow_repeat_tx = p->allow_repeat_tx;

    err = srtp_stream_init_all_master_keys(srtp, p->key, p->keys,
                                           p->num_master_keys);
    if (err) {
        srtp_rdbx_dealloc(&srtp->rtp_rdbx);
        return err;
    }

    err = srtp_ekt_stream_init_from_policy(srtp->ekt, p->ekt);
    if (err) {
        srtp_rdbx_dealloc(&srtp->rtp_rdbx);
        return err;
    }

    return srtp_err_status_ok;
}

void srtp_event_reporter(srtp_event_data_t *data)
{
    srtp_err_report(srtp_err_level_warning, "srtp: in stream 0x%x: ",
                    data->ssrc);

    switch (data->event) {
    case event_ssrc_collision:
        srtp_err_report(srtp_err_level_warning, "\tSSRC collision\n");
        break;
    case event_key_soft_limit:
        srtp_err_report(srtp_err_level_warning,
                        "\tkey usage soft limit reached\n");
        break;
    case event_key_hard_limit:
        srtp_err_report(srtp_err_level_warning,
                        "\tkey usage hard limit reached\n");
        break;
    case event_packet_index_limit:
        srtp_err_report(srtp_err_level_warning,
                        "\tpacket index limit reached\n");
        break;
    default:
        srtp_err_report(srtp_err_level_warning,
                        "\tunknown event reported to handler\n");
    }
}

static srtp_event_handler_func_t *srtp_event_handler = srtp_event_reporter;

srtp_err_status_t srtp_install_event_handler(srtp_event_handler_func_t func)
{

    srtp_event_handler = func;
    return srtp_err_status_ok;
}

static int srtp_protect_extension_header(srtp_stream_ctx_t *stream, int id)
{
    int *enc_xtn_hdr = stream->enc_xtn_hdr;
    int count = stream->enc_xtn_hdr_count;

    if (!enc_xtn_hdr || count <= 0) {
        return 0;
    }

    while (count > 0) {
        if (*enc_xtn_hdr == id) {
            return 1;
        }

        enc_xtn_hdr++;
        count--;
    }
    return 0;
}

static srtp_err_status_t srtp_process_header_encryption(
    srtp_stream_ctx_t *stream,
    srtp_hdr_xtnd_t *xtn_hdr,
    srtp_session_keys_t *session_keys)
{
    srtp_err_status_t status;
    uint8_t keystream[257];
    int keystream_pos;
    uint8_t *xtn_hdr_data = ((uint8_t *)xtn_hdr) + octets_in_rtp_extn_hdr;
    uint8_t *xtn_hdr_end =
        xtn_hdr_data + (ntohs(xtn_hdr->length) * sizeof(uint32_t));

    if (ntohs(xtn_hdr->profile_specific) == 0xbede) {

        while (xtn_hdr_data < xtn_hdr_end) {
            uint8_t xid = (*xtn_hdr_data & 0xf0) >> 4;
            unsigned int xlen = (*xtn_hdr_data & 0x0f) + 1;
            uint32_t xlen_with_header = 1 + xlen;
            xtn_hdr_data++;

            if (xtn_hdr_data + xlen > xtn_hdr_end)
                return srtp_err_status_parse_err;

            if (xid == 15) {

                break;
            }

            status = srtp_cipher_output(session_keys->rtp_xtn_hdr_cipher,
                                        keystream, &xlen_with_header);
            if (status)
                return srtp_err_status_cipher_fail;

            if (srtp_protect_extension_header(stream, xid)) {
                keystream_pos = 1;
                while (xlen > 0) {
                    *xtn_hdr_data ^= keystream[keystream_pos++];
                    xtn_hdr_data++;
                    xlen--;
                }
            } else {
                xtn_hdr_data += xlen;
            }

            while (xtn_hdr_data < xtn_hdr_end && *xtn_hdr_data == 0) {
                xtn_hdr_data++;
            }
        }
    } else if ((ntohs(xtn_hdr->profile_specific) & 0x1fff) == 0x100) {

        while (xtn_hdr_data + 1 < xtn_hdr_end) {
            uint8_t xid = *xtn_hdr_data;
            unsigned int xlen = *(xtn_hdr_data + 1);
            uint32_t xlen_with_header = 2 + xlen;
            xtn_hdr_data += 2;

            if (xtn_hdr_data + xlen > xtn_hdr_end)
                return srtp_err_status_parse_err;

            status = srtp_cipher_output(session_keys->rtp_xtn_hdr_cipher,
                                        keystream, &xlen_with_header);
            if (status)
                return srtp_err_status_cipher_fail;

            if (xlen > 0 && srtp_protect_extension_header(stream, xid)) {
                keystream_pos = 2;
                while (xlen > 0) {
                    *xtn_hdr_data ^= keystream[keystream_pos++];
                    xtn_hdr_data++;
                    xlen--;
                }
            } else {
                xtn_hdr_data += xlen;
            }

            while (xtn_hdr_data < xtn_hdr_end && *xtn_hdr_data == 0) {
                xtn_hdr_data++;
            }
        }
    } else {

        return srtp_err_status_parse_err;
    }

    return srtp_err_status_ok;
}

static void srtp_calc_aead_iv(srtp_session_keys_t *session_keys,
                              v128_t *iv,
                              srtp_xtd_seq_num_t *seq,
                              srtp_hdr_t *hdr)
{
    v128_t in;
    v128_t salt;

#ifdef NO_64BIT_MATH
    uint32_t local_roc = ((high32(*seq) << 16) | (low32(*seq) >> 16));
    uint16_t local_seq = (uint16_t)(low32(*seq));
#else
    uint32_t local_roc = (uint32_t)(*seq >> 16);
    uint16_t local_seq = (uint16_t)*seq;
#endif

    memset(&in, 0, sizeof(v128_t));
    memset(&salt, 0, sizeof(v128_t));

    in.v16[5] = htons(local_seq);
    local_roc = htonl(local_roc);
    memcpy(&in.v16[3], &local_roc, sizeof(local_roc));

    memcpy(&in.v8[2], &hdr->ssrc, 4);
    debug_print(mod_srtp, "Pre-salted RTP IV = %s\n", v128_hex_string(&in));

    memcpy(salt.v8, session_keys->salt, SRTP_AEAD_SALT_LEN);
    debug_print(mod_srtp, "RTP SALT = %s\n", v128_hex_string(&salt));

    v128_xor(iv, &in, &salt);
}

srtp_session_keys_t *srtp_get_session_keys(srtp_stream_ctx_t *stream,
                                           uint8_t *hdr,
                                           const unsigned int *pkt_octet_len,
                                           unsigned int *mki_size)
{
    unsigned int base_mki_start_location = *pkt_octet_len;
    unsigned int mki_start_location = 0;
    unsigned int tag_len = 0;
    unsigned int i = 0;

    if (stream->session_keys[0].rtp_cipher->algorithm == SRTP_AES_GCM_128 ||
        stream->session_keys[0].rtp_cipher->algorithm == SRTP_AES_GCM_256) {
        tag_len = 0;
    } else {
        tag_len = srtp_auth_get_tag_length(stream->session_keys[0].rtp_auth);
    }

    if (tag_len > base_mki_start_location) {
        *mki_size = 0;
        return NULL;
    }

    base_mki_start_location -= tag_len;

    for (i = 0; i < stream->num_master_keys; i++) {
        if (stream->session_keys[i].mki_size != 0 &&
            stream->session_keys[i].mki_size <= base_mki_start_location) {
            *mki_size = stream->session_keys[i].mki_size;
            mki_start_location = base_mki_start_location - *mki_size;

            if (memcmp(hdr + mki_start_location, stream->session_keys[i].mki_id,
                       *mki_size) == 0) {
                return &stream->session_keys[i];
            }
        }
    }

    *mki_size = 0;
    return NULL;
}

static srtp_err_status_t srtp_estimate_index(srtp_rdbx_t *rdbx,
                                             uint32_t roc,
                                             srtp_xtd_seq_num_t *est,
                                             srtp_sequence_number_t seq,
                                             int *delta)
{
#ifdef NO_64BIT_MATH
    uint32_t internal_pkt_idx_reduced;
    uint32_t external_pkt_idx_reduced;
    uint32_t internal_roc;
    uint32_t roc_difference;
#endif

#ifdef NO_64BIT_MATH
    *est = (srtp_xtd_seq_num_t)make64(roc >> 16, (roc << 16) | seq);
    *delta = low32(est) - rdbx->index;
#else
    *est = (srtp_xtd_seq_num_t)(((uint64_t)roc) << 16) | seq;
    *delta = (int)(*est - rdbx->index);
#endif

    if (*est > rdbx->index) {
#ifdef NO_64BIT_MATH
        internal_roc = (uint32_t)(rdbx->index >> 16);
        roc_difference = roc - internal_roc;
        if (roc_difference > 1) {
            *delta = 0;
            return srtp_err_status_pkt_idx_adv;
        }

        internal_pkt_idx_reduced = (uint32_t)(rdbx->index & 0xFFFF);
        external_pkt_idx_reduced = (uint32_t)((roc_difference << 16) | seq);

        if (external_pkt_idx_reduced - internal_pkt_idx_reduced >
            seq_num_median) {
            *delta = 0;
            return srtp_err_status_pkt_idx_adv;
        }
#else
        if (*est - rdbx->index > seq_num_median) {
            *delta = 0;
            return srtp_err_status_pkt_idx_adv;
        }
#endif
    } else if (*est < rdbx->index) {
#ifdef NO_64BIT_MATH

        internal_roc = (uint32_t)(rdbx->index >> 16);
        roc_difference = internal_roc - roc;
        if (roc_difference > 1) {
            *delta = 0;
            return srtp_err_status_pkt_idx_adv;
        }

        internal_pkt_idx_reduced =
            (uint32_t)((roc_difference << 16) | rdbx->index & 0xFFFF);
        external_pkt_idx_reduced = (uint32_t)(seq);

        if (internal_pkt_idx_reduced - external_pkt_idx_reduced >
            seq_num_median) {
            *delta = 0;
            return srtp_err_status_pkt_idx_old;
        }
#else
        if (rdbx->index - *est > seq_num_median) {
            *delta = 0;
            return srtp_err_status_pkt_idx_old;
        }
#endif
    }

    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_get_est_pkt_index(srtp_hdr_t *hdr,
                                                srtp_stream_ctx_t *stream,
                                                srtp_xtd_seq_num_t *est,
                                                int *delta)
{
    srtp_err_status_t result = srtp_err_status_ok;

    if (stream->pending_roc) {
        result = srtp_estimate_index(&stream->rtp_rdbx, stream->pending_roc,
                                     est, ntohs(hdr->seq), delta);
    } else {

        *delta =
            srtp_rdbx_estimate_index(&stream->rtp_rdbx, est, ntohs(hdr->seq));
    }

#ifdef NO_64BIT_MATH
    debug_print2(mod_srtp, "estimated u_packet index: %08x%08x", high32(*est),
                 low32(*est));
#else
    debug_print(mod_srtp, "estimated u_packet index: %016llx", *est);
#endif
    return result;
}

static srtp_err_status_t srtp_protect_aead(srtp_ctx_t *ctx,
                                           srtp_stream_ctx_t *stream,
                                           void *rtp_hdr,
                                           unsigned int *pkt_octet_len,
                                           srtp_session_keys_t *session_keys,
                                           unsigned int use_mki)
{
    srtp_hdr_t *hdr = (srtp_hdr_t *)rtp_hdr;
    uint32_t *enc_start;
    int enc_octet_len = 0;
    srtp_xtd_seq_num_t est;
    int delta;
    srtp_err_status_t status;
    uint32_t tag_len;
    v128_t iv;
    unsigned int aad_len;
    srtp_hdr_xtnd_t *xtn_hdr = NULL;
    unsigned int mki_size = 0;
    uint8_t *mki_location = NULL;

    debug_print(mod_srtp, "function srtp_protect_aead", NULL);

    switch (srtp_key_limit_update(session_keys->limit)) {
    case srtp_key_event_normal:
        break;
    case srtp_key_event_hard_limit:
        srtp_handle_event(ctx, stream, event_key_hard_limit);
        return srtp_err_status_key_expired;
    case srtp_key_event_soft_limit:
    default:
        srtp_handle_event(ctx, stream, event_key_soft_limit);
        break;
    }

    tag_len = srtp_auth_get_tag_length(session_keys->rtp_auth);

    enc_start = (uint32_t *)hdr + uint32s_in_rtp_header + hdr->cc;
    if (hdr->x == 1) {
        xtn_hdr = (srtp_hdr_xtnd_t *)enc_start;
        enc_start += (ntohs(xtn_hdr->length) + 1);
    }

    if (!((uint8_t *)enc_start <= (uint8_t *)hdr + *pkt_octet_len))
        return srtp_err_status_parse_err;
    enc_octet_len =
        (int)(*pkt_octet_len - ((uint8_t *)enc_start - (uint8_t *)hdr));
    if (enc_octet_len < 0)
        return srtp_err_status_parse_err;

    delta = srtp_rdbx_estimate_index(&stream->rtp_rdbx, &est, ntohs(hdr->seq));
    status = srtp_rdbx_check(&stream->rtp_rdbx, delta);
    if (status) {
        if (status != srtp_err_status_replay_fail || !stream->allow_repeat_tx) {
            return status;
        }
    } else {
        srtp_rdbx_add_index(&stream->rtp_rdbx, delta);
    }

#ifdef NO_64BIT_MATH
    debug_print2(mod_srtp, "estimated packet index: %08x%08x", high32(est),
                 low32(est));
#else
    debug_print(mod_srtp, "estimated packet index: %016llx", est);
#endif

    srtp_calc_aead_iv(session_keys, &iv, &est, hdr);

#ifdef NO_64BIT_MATH
    est = be64_to_cpu(
        make64((high32(est) << 16) | (low32(est) >> 16), low32(est) << 16));
#else
    est = be64_to_cpu(est << 16);
#endif

    status = srtp_cipher_set_iv(session_keys->rtp_cipher, (uint8_t *)&iv,
                                srtp_direction_encrypt);
    if (!status && session_keys->rtp_xtn_hdr_cipher) {
        iv.v32[0] = 0;
        iv.v32[1] = hdr->ssrc;
        iv.v64[1] = est;
        status = srtp_cipher_set_iv(session_keys->rtp_xtn_hdr_cipher,
                                    (uint8_t *)&iv, srtp_direction_encrypt);
    }
    if (status) {
        return srtp_err_status_cipher_fail;
    }

    if (xtn_hdr && session_keys->rtp_xtn_hdr_cipher) {

        status = srtp_process_header_encryption(stream, xtn_hdr, session_keys);
        if (status) {
            return status;
        }
    }

    aad_len = (uint8_t *)enc_start - (uint8_t *)hdr;
    status =
        srtp_cipher_set_aad(session_keys->rtp_cipher, (uint8_t *)hdr, aad_len);
    if (status) {
        return (srtp_err_status_cipher_fail);
    }

    status = srtp_cipher_encrypt(session_keys->rtp_cipher, (uint8_t *)enc_start,
                                 (unsigned int *)&enc_octet_len);
    if (status) {
        return srtp_err_status_cipher_fail;
    }

    status =
        srtp_cipher_get_tag(session_keys->rtp_cipher,
                            (uint8_t *)enc_start + enc_octet_len, &tag_len);
    if (status) {
        return (srtp_err_status_cipher_fail);
    }

    mki_location = (uint8_t *)hdr + *pkt_octet_len + tag_len;
    mki_size = srtp_inject_mki(mki_location, session_keys, use_mki);

    *pkt_octet_len += tag_len;

    *pkt_octet_len += mki_size;

    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_unprotect_aead(srtp_ctx_t *ctx,
                                             srtp_stream_ctx_t *stream,
                                             int delta,
                                             srtp_xtd_seq_num_t est,
                                             void *srtp_hdr,
                                             unsigned int *pkt_octet_len,
                                             srtp_session_keys_t *session_keys,
                                             unsigned int mki_size)
{
    srtp_hdr_t *hdr = (srtp_hdr_t *)srtp_hdr;
    uint32_t *enc_start;
    unsigned int enc_octet_len = 0;
    v128_t iv;
    srtp_err_status_t status;
    int tag_len;
    unsigned int aad_len;
    srtp_hdr_xtnd_t *xtn_hdr = NULL;

    debug_print(mod_srtp, "function srtp_unprotect_aead", NULL);

#ifdef NO_64BIT_MATH
    debug_print2(mod_srtp, "estimated u_packet index: %08x%08x", high32(est),
                 low32(est));
#else
    debug_print(mod_srtp, "estimated u_packet index: %016llx", est);
#endif

    tag_len = srtp_auth_get_tag_length(session_keys->rtp_auth);

    srtp_calc_aead_iv(session_keys, &iv, &est, hdr);
    status = srtp_cipher_set_iv(session_keys->rtp_cipher, (uint8_t *)&iv,
                                srtp_direction_decrypt);
    if (!status && session_keys->rtp_xtn_hdr_cipher) {
        iv.v32[0] = 0;
        iv.v32[1] = hdr->ssrc;
#ifdef NO_64BIT_MATH
        iv.v64[1] = be64_to_cpu(
            make64((high32(est) << 16) | (low32(est) >> 16), low32(est) << 16));
#else
        iv.v64[1] = be64_to_cpu(est << 16);
#endif
        status = srtp_cipher_set_iv(session_keys->rtp_xtn_hdr_cipher,
                                    (uint8_t *)&iv, srtp_direction_encrypt);
    }
    if (status) {
        return srtp_err_status_cipher_fail;
    }

    enc_start = (uint32_t *)hdr + uint32s_in_rtp_header + hdr->cc;
    if (hdr->x == 1) {
        xtn_hdr = (srtp_hdr_xtnd_t *)enc_start;
        enc_start += (ntohs(xtn_hdr->length) + 1);
    }
    if (!((uint8_t *)enc_start <=
          (uint8_t *)hdr + (*pkt_octet_len - tag_len - mki_size)))
        return srtp_err_status_parse_err;

    enc_octet_len = (unsigned int)(*pkt_octet_len - mki_size -
                                   ((uint8_t *)enc_start - (uint8_t *)hdr));

    if (enc_octet_len < (unsigned int)tag_len) {
        return srtp_err_status_cipher_fail;
    }

    switch (srtp_key_limit_update(session_keys->limit)) {
    case srtp_key_event_normal:
        break;
    case srtp_key_event_soft_limit:
        srtp_handle_event(ctx, stream, event_key_soft_limit);
        break;
    case srtp_key_event_hard_limit:
        srtp_handle_event(ctx, stream, event_key_hard_limit);
        return srtp_err_status_key_expired;
    default:
        break;
    }

    aad_len = (uint8_t *)enc_start - (uint8_t *)hdr;
    status =
        srtp_cipher_set_aad(session_keys->rtp_cipher, (uint8_t *)hdr, aad_len);
    if (status) {
        return (srtp_err_status_cipher_fail);
    }

    status = srtp_cipher_decrypt(session_keys->rtp_cipher, (uint8_t *)enc_start,
                                 &enc_octet_len);
    if (status) {
        return status;
    }

    if (xtn_hdr && session_keys->rtp_xtn_hdr_cipher) {

        status = srtp_process_header_encryption(stream, xtn_hdr, session_keys);
        if (status) {
            return status;
        }
    }

    if (stream->direction != dir_srtp_receiver) {
        if (stream->direction == dir_unknown) {
            stream->direction = dir_srtp_receiver;
        } else {
            srtp_handle_event(ctx, stream, event_ssrc_collision);
        }
    }

    if (stream == ctx->stream_template) {
        srtp_stream_ctx_t *new_stream;

        status =
            srtp_stream_clone(ctx->stream_template, hdr->ssrc, &new_stream);
        if (status) {
            return status;
        }

        new_stream->next = ctx->stream_list;
        ctx->stream_list = new_stream;

        stream = new_stream;
    }

    srtp_rdbx_add_index(&stream->rtp_rdbx, delta);

    *pkt_octet_len -= tag_len;

    *pkt_octet_len -= mki_size;

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_protect(srtp_ctx_t *ctx,
                               void *rtp_hdr,
                               int *pkt_octet_len)
{
    return srtp_protect_mki(ctx, rtp_hdr, pkt_octet_len, 0, 0);
}

srtp_err_status_t srtp_protect_mki(srtp_ctx_t *ctx,
                                   void *rtp_hdr,
                                   int *pkt_octet_len,
                                   unsigned int use_mki,
                                   unsigned int mki_index)
{
    srtp_hdr_t *hdr = (srtp_hdr_t *)rtp_hdr;
    uint32_t *enc_start;
    uint32_t *auth_start;
    int enc_octet_len = 0;
    srtp_xtd_seq_num_t est;
    int delta;
    uint8_t *auth_tag = NULL;
    srtp_err_status_t status;
    int tag_len;
    srtp_stream_ctx_t *stream;
    uint32_t prefix_len;
    srtp_hdr_xtnd_t *xtn_hdr = NULL;
    unsigned int mki_size = 0;
    srtp_session_keys_t *session_keys = NULL;
    uint8_t *mki_location = NULL;
    int advance_packet_index = 0;

    debug_print(mod_srtp, "function srtp_protect", NULL);

    status = srtp_validate_rtp_header(rtp_hdr, pkt_octet_len);
    if (status)
        return status;

    if (*pkt_octet_len < octets_in_rtp_header)
        return srtp_err_status_bad_param;

    stream = srtp_get_stream(ctx, hdr->ssrc);
    if (stream == NULL) {
        if (ctx->stream_template != NULL) {
            srtp_stream_ctx_t *new_stream;

            status =
                srtp_stream_clone(ctx->stream_template, hdr->ssrc, &new_stream);
            if (status)
                return status;

            new_stream->next = ctx->stream_list;
            ctx->stream_list = new_stream;

            new_stream->direction = dir_srtp_sender;

            stream = new_stream;
        } else {

            return srtp_err_status_no_ctx;
        }
    }

    if (stream->direction != dir_srtp_sender) {
        if (stream->direction == dir_unknown) {
            stream->direction = dir_srtp_sender;
        } else {
            srtp_handle_event(ctx, stream, event_ssrc_collision);
        }
    }

    session_keys =
        srtp_get_session_keys_with_mki_index(stream, use_mki, mki_index);

    if (session_keys == NULL)
        return srtp_err_status_bad_mki;

    if (session_keys->rtp_cipher->algorithm == SRTP_AES_GCM_128 ||
        session_keys->rtp_cipher->algorithm == SRTP_AES_GCM_256) {
        return srtp_protect_aead(ctx, stream, rtp_hdr,
                                 (unsigned int *)pkt_octet_len, session_keys,
                                 use_mki);
    }

    switch (srtp_key_limit_update(session_keys->limit)) {
    case srtp_key_event_normal:
        break;
    case srtp_key_event_soft_limit:
        srtp_handle_event(ctx, stream, event_key_soft_limit);
        break;
    case srtp_key_event_hard_limit:
        srtp_handle_event(ctx, stream, event_key_hard_limit);
        return srtp_err_status_key_expired;
    default:
        break;
    }

    tag_len = srtp_auth_get_tag_length(session_keys->rtp_auth);

    if (stream->rtp_services & sec_serv_conf) {
        enc_start = (uint32_t *)hdr + uint32s_in_rtp_header + hdr->cc;
        if (hdr->x == 1) {
            xtn_hdr = (srtp_hdr_xtnd_t *)enc_start;
            enc_start += (ntohs(xtn_hdr->length) + 1);
        }

        if (!((uint8_t *)enc_start <= (uint8_t *)hdr + *pkt_octet_len))
            return srtp_err_status_parse_err;
        enc_octet_len =
            (int)(*pkt_octet_len - ((uint8_t *)enc_start - (uint8_t *)hdr));
        if (enc_octet_len < 0)
            return srtp_err_status_parse_err;
    } else {
        enc_start = NULL;
    }

    mki_location = (uint8_t *)hdr + *pkt_octet_len;
    mki_size = srtp_inject_mki(mki_location, session_keys, use_mki);

    if (stream->rtp_services & sec_serv_auth) {
        auth_start = (uint32_t *)hdr;
        auth_tag = (uint8_t *)hdr + *pkt_octet_len + mki_size;
    } else {
        auth_start = NULL;
        auth_tag = NULL;
    }

    status = srtp_get_est_pkt_index(hdr, stream, &est, &delta);

    if (status && (status != srtp_err_status_pkt_idx_adv))
        return status;

    if (status == srtp_err_status_pkt_idx_adv)
        advance_packet_index = 1;

    if (advance_packet_index) {
        srtp_rdbx_set_roc_seq(&stream->rtp_rdbx, (uint32_t)(est >> 16),
                              (uint16_t)(est & 0xFFFF));
        stream->pending_roc = 0;
        srtp_rdbx_add_index(&stream->rtp_rdbx, 0);
    } else {
        status = srtp_rdbx_check(&stream->rtp_rdbx, delta);
        if (status) {
            if (status != srtp_err_status_replay_fail ||
                !stream->allow_repeat_tx)
                return status;
        }
        srtp_rdbx_add_index(&stream->rtp_rdbx, delta);
    }

#ifdef NO_64BIT_MATH
    debug_print2(mod_srtp, "estimated packet index: %08x%08x", high32(est),
                 low32(est));
#else
    debug_print(mod_srtp, "estimated packet index: %016llx", est);
#endif

    if (session_keys->rtp_cipher->type->id == SRTP_AES_ICM_128 ||
        session_keys->rtp_cipher->type->id == SRTP_AES_ICM_192 ||
        session_keys->rtp_cipher->type->id == SRTP_AES_ICM_256) {
        v128_t iv;

        iv.v32[0] = 0;
        iv.v32[1] = hdr->ssrc;
#ifdef NO_64BIT_MATH
        iv.v64[1] = be64_to_cpu(
            make64((high32(est) << 16) | (low32(est) >> 16), low32(est) << 16));
#else
        iv.v64[1] = be64_to_cpu(est << 16);
#endif
        status = srtp_cipher_set_iv(session_keys->rtp_cipher, (uint8_t *)&iv,
                                    srtp_direction_encrypt);
        if (!status && session_keys->rtp_xtn_hdr_cipher) {
            status = srtp_cipher_set_iv(session_keys->rtp_xtn_hdr_cipher,
                                        (uint8_t *)&iv, srtp_direction_encrypt);
        }
    } else {
        v128_t iv;

#ifdef NO_64BIT_MATH
        iv.v32[0] = 0;
        iv.v32[1] = 0;
#else
        iv.v64[0] = 0;
#endif
        iv.v64[1] = be64_to_cpu(est);
        status = srtp_cipher_set_iv(session_keys->rtp_cipher, (uint8_t *)&iv,
                                    srtp_direction_encrypt);
        if (!status && session_keys->rtp_xtn_hdr_cipher) {
            status = srtp_cipher_set_iv(session_keys->rtp_xtn_hdr_cipher,
                                        (uint8_t *)&iv, srtp_direction_encrypt);
        }
    }
    if (status)
        return srtp_err_status_cipher_fail;

#ifdef NO_64BIT_MATH
    est = be64_to_cpu(
        make64((high32(est) << 16) | (low32(est) >> 16), low32(est) << 16));
#else
    est = be64_to_cpu(est << 16);
#endif

    if (auth_start) {
        prefix_len = srtp_auth_get_prefix_length(session_keys->rtp_auth);
        if (prefix_len) {
            status = srtp_cipher_output(session_keys->rtp_cipher, auth_tag,
                                        &prefix_len);
            if (status)
                return srtp_err_status_cipher_fail;
            debug_print(mod_srtp, "keystream prefix: %s",
                        srtp_octet_string_hex_string(auth_tag, prefix_len));
        }
    }

    if (xtn_hdr && session_keys->rtp_xtn_hdr_cipher) {

        status = srtp_process_header_encryption(stream, xtn_hdr, session_keys);
        if (status) {
            return status;
        }
    }

    if (enc_start) {
        status =
            srtp_cipher_encrypt(session_keys->rtp_cipher, (uint8_t *)enc_start,
                                (unsigned int *)&enc_octet_len);
        if (status)
            return srtp_err_status_cipher_fail;
    }

    if (auth_start) {

        status = srtp_auth_start(session_keys->rtp_auth);
        if (status)
            return status;

        status = srtp_auth_update(session_keys->rtp_auth, (uint8_t *)auth_start,
                                  *pkt_octet_len);
        if (status)
            return status;

        debug_print(mod_srtp, "estimated packet index: %016llx", est);
        status = srtp_auth_compute(session_keys->rtp_auth, (uint8_t *)&est, 4,
                                   auth_tag);
        debug_print(mod_srtp, "srtp auth tag:    %s",
                    srtp_octet_string_hex_string(auth_tag, tag_len));
        if (status)
            return srtp_err_status_auth_fail;
    }

    if (auth_tag) {

        *pkt_octet_len += tag_len;
    }

    if (use_mki) {

        *pkt_octet_len += mki_size;
    }

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_unprotect(srtp_ctx_t *ctx,
                                 void *srtp_hdr,
                                 int *pkt_octet_len)
{
    return srtp_unprotect_mki(ctx, srtp_hdr, pkt_octet_len, 0);
}

srtp_err_status_t srtp_unprotect_mki(srtp_ctx_t *ctx,
                                     void *srtp_hdr,
                                     int *pkt_octet_len,
                                     unsigned int use_mki)
{
    srtp_hdr_t *hdr = (srtp_hdr_t *)srtp_hdr;
    uint32_t *enc_start;
    uint32_t *auth_start;
    unsigned int enc_octet_len = 0;
    uint8_t *auth_tag = NULL;
    srtp_xtd_seq_num_t est;
    int delta;
    v128_t iv;
    srtp_err_status_t status;
    srtp_stream_ctx_t *stream;
    uint8_t tmp_tag[SRTP_MAX_TAG_LEN];
    uint32_t tag_len, prefix_len;
    srtp_hdr_xtnd_t *xtn_hdr = NULL;
    unsigned int mki_size = 0;
    srtp_session_keys_t *session_keys = NULL;
    int advance_packet_index = 0;
    uint32_t roc_to_set = 0;
    uint16_t seq_to_set = 0;

    debug_print(mod_srtp, "function srtp_unprotect", NULL);

    status = srtp_validate_rtp_header(srtp_hdr, pkt_octet_len);
    if (status)
        return status;

    if (*pkt_octet_len < octets_in_rtp_header)
        return srtp_err_status_bad_param;

    stream = srtp_get_stream(ctx, hdr->ssrc);
    if (stream == NULL) {
        if (ctx->stream_template != NULL) {
            stream = ctx->stream_template;
            debug_print(mod_srtp, "using provisional stream (SSRC: 0x%08x)",
                        ntohl(hdr->ssrc));

#ifdef NO_64BIT_MATH
            est = (srtp_xtd_seq_num_t)make64(0, ntohs(hdr->seq));
            delta = low32(est);
#else
            est = (srtp_xtd_seq_num_t)ntohs(hdr->seq);
            delta = (int)est;
#endif
        } else {

            return srtp_err_status_no_ctx;
        }
    } else {
        status = srtp_get_est_pkt_index(hdr, stream, &est, &delta);

        if (status && (status != srtp_err_status_pkt_idx_adv))
            return status;

        if (status == srtp_err_status_pkt_idx_adv) {
            advance_packet_index = 1;
            roc_to_set = (uint32_t)(est >> 16);
            seq_to_set = (uint16_t)(est & 0xFFFF);
        }

        if (!advance_packet_index) {
            status = srtp_rdbx_check(&stream->rtp_rdbx, delta);
            if (status)
                return status;
        }
    }

#ifdef NO_64BIT_MATH
    debug_print2(mod_srtp, "estimated u_packet index: %08x%08x", high32(est),
                 low32(est));
#else
    debug_print(mod_srtp, "estimated u_packet index: %016llx", est);
#endif

    if (use_mki) {
        session_keys = srtp_get_session_keys(
            stream, (uint8_t *)hdr, (const unsigned int *)pkt_octet_len,
            &mki_size);

        if (session_keys == NULL)
            return srtp_err_status_bad_mki;
    } else {
        session_keys = &stream->session_keys[0];
    }

    if (session_keys->rtp_cipher->algorithm == SRTP_AES_GCM_128 ||
        session_keys->rtp_cipher->algorithm == SRTP_AES_GCM_256) {
        return srtp_unprotect_aead(ctx, stream, delta, est, srtp_hdr,
                                   (unsigned int *)pkt_octet_len, session_keys,
                                   mki_size);
    }

    tag_len = srtp_auth_get_tag_length(session_keys->rtp_auth);

    if (session_keys->rtp_cipher->type->id == SRTP_AES_ICM_128 ||
        session_keys->rtp_cipher->type->id == SRTP_AES_ICM_192 ||
        session_keys->rtp_cipher->type->id == SRTP_AES_ICM_256) {

        iv.v32[0] = 0;
        iv.v32[1] = hdr->ssrc;
#ifdef NO_64BIT_MATH
        iv.v64[1] = be64_to_cpu(
            make64((high32(est) << 16) | (low32(est) >> 16), low32(est) << 16));
#else
        iv.v64[1] = be64_to_cpu(est << 16);
#endif
        status = srtp_cipher_set_iv(session_keys->rtp_cipher, (uint8_t *)&iv,
                                    srtp_direction_decrypt);
        if (!status && session_keys->rtp_xtn_hdr_cipher) {
            status = srtp_cipher_set_iv(session_keys->rtp_xtn_hdr_cipher,
                                        (uint8_t *)&iv, srtp_direction_decrypt);
        }
    } else {

#ifdef NO_64BIT_MATH
        iv.v32[0] = 0;
        iv.v32[1] = 0;
#else
        iv.v64[0] = 0;
#endif
        iv.v64[1] = be64_to_cpu(est);
        status = srtp_cipher_set_iv(session_keys->rtp_cipher, (uint8_t *)&iv,
                                    srtp_direction_decrypt);
        if (!status && session_keys->rtp_xtn_hdr_cipher) {
            status = srtp_cipher_set_iv(session_keys->rtp_xtn_hdr_cipher,
                                        (uint8_t *)&iv, srtp_direction_decrypt);
        }
    }
    if (status)
        return srtp_err_status_cipher_fail;

#ifdef NO_64BIT_MATH
    est = be64_to_cpu(
        make64((high32(est) << 16) | (low32(est) >> 16), low32(est) << 16));
#else
    est = be64_to_cpu(est << 16);
#endif

    if (stream->rtp_services & sec_serv_conf) {
        enc_start = (uint32_t *)hdr + uint32s_in_rtp_header + hdr->cc;
        if (hdr->x == 1) {
            xtn_hdr = (srtp_hdr_xtnd_t *)enc_start;
            enc_start += (ntohs(xtn_hdr->length) + 1);
        }
        if (!((uint8_t *)enc_start <=
              (uint8_t *)hdr + (*pkt_octet_len - tag_len - mki_size)))
            return srtp_err_status_parse_err;
        enc_octet_len = (uint32_t)(*pkt_octet_len - tag_len - mki_size -
                                   ((uint8_t *)enc_start - (uint8_t *)hdr));
    } else {
        enc_start = NULL;
    }

    if (stream->rtp_services & sec_serv_auth) {
        auth_start = (uint32_t *)hdr;
        auth_tag = (uint8_t *)hdr + *pkt_octet_len - tag_len;
    } else {
        auth_start = NULL;
        auth_tag = NULL;
    }

    if (auth_start) {

        if (session_keys->rtp_auth->prefix_len != 0) {
            prefix_len = srtp_auth_get_prefix_length(session_keys->rtp_auth);
            status = srtp_cipher_output(session_keys->rtp_cipher, tmp_tag,
                                        &prefix_len);
            debug_print(mod_srtp, "keystream prefix: %s",
                        srtp_octet_string_hex_string(tmp_tag, prefix_len));
            if (status)
                return srtp_err_status_cipher_fail;
        }

        status = srtp_auth_start(session_keys->rtp_auth);
        if (status)
            return status;

        status = srtp_auth_update(session_keys->rtp_auth, (uint8_t *)auth_start,
                                  *pkt_octet_len - tag_len - mki_size);

        status = srtp_auth_compute(session_keys->rtp_auth, (uint8_t *)&est, 4,
                                   tmp_tag);

        debug_print(mod_srtp, "computed auth tag:    %s",
                    srtp_octet_string_hex_string(tmp_tag, tag_len));
        debug_print(mod_srtp, "packet auth tag:      %s",
                    srtp_octet_string_hex_string(auth_tag, tag_len));
        if (status)
            return srtp_err_status_auth_fail;

        if (octet_string_is_eq(tmp_tag, auth_tag, tag_len))
            return srtp_err_status_auth_fail;
    }

    switch (srtp_key_limit_update(session_keys->limit)) {
    case srtp_key_event_normal:
        break;
    case srtp_key_event_soft_limit:
        srtp_handle_event(ctx, stream, event_key_soft_limit);
        break;
    case srtp_key_event_hard_limit:
        srtp_handle_event(ctx, stream, event_key_hard_limit);
        return srtp_err_status_key_expired;
    default:
        break;
    }

    if (xtn_hdr && session_keys->rtp_xtn_hdr_cipher) {

        status = srtp_process_header_encryption(stream, xtn_hdr, session_keys);
        if (status) {
            return status;
        }
    }

    if (enc_start) {
        status = srtp_cipher_decrypt(session_keys->rtp_cipher,
                                     (uint8_t *)enc_start, &enc_octet_len);
        if (status)
            return srtp_err_status_cipher_fail;
    }

    if (stream->direction != dir_srtp_receiver) {
        if (stream->direction == dir_unknown) {
            stream->direction = dir_srtp_receiver;
        } else {
            srtp_handle_event(ctx, stream, event_ssrc_collision);
        }
    }

    if (stream == ctx->stream_template) {
        srtp_stream_ctx_t *new_stream;

        status =
            srtp_stream_clone(ctx->stream_template, hdr->ssrc, &new_stream);
        if (status)
            return status;

        new_stream->next = ctx->stream_list;
        ctx->stream_list = new_stream;

        stream = new_stream;
    }

    if (advance_packet_index) {
        srtp_rdbx_set_roc_seq(&stream->rtp_rdbx, roc_to_set, seq_to_set);
        stream->pending_roc = 0;
        srtp_rdbx_add_index(&stream->rtp_rdbx, 0);
    } else {
        srtp_rdbx_add_index(&stream->rtp_rdbx, delta);
    }

    *pkt_octet_len -= tag_len;

    *pkt_octet_len -= mki_size;

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_init()
{
    srtp_err_status_t status;

    status = srtp_crypto_kernel_init();
    if (status)
        return status;

    status = srtp_crypto_kernel_load_debug_module(&mod_srtp);
    if (status)
        return status;

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_shutdown()
{
    srtp_err_status_t status;

    status = srtp_crypto_kernel_shutdown();
    if (status)
        return status;

    return srtp_err_status_ok;
}

#if 0

int
srtp_get_trailer_length(const srtp_stream_t s) {
  return srtp_auth_get_tag_length(s->rtp_auth);
}

#endif

srtp_stream_ctx_t *srtp_get_stream(srtp_t srtp, uint32_t ssrc)
{
    srtp_stream_ctx_t *stream;

    stream = srtp->stream_list;
    while (stream != NULL) {
        if (stream->ssrc == ssrc)
            return stream;
        stream = stream->next;
    }

    return NULL;
}

srtp_err_status_t srtp_dealloc(srtp_t session)
{
    srtp_stream_ctx_t *stream;
    srtp_err_status_t status;

    stream = session->stream_list;
    while (stream != NULL) {
        srtp_stream_t next = stream->next;
        status = srtp_stream_dealloc(stream, session->stream_template);
        if (status)
            return status;
        stream = next;
    }

    if (session->stream_template != NULL) {
        status = srtp_stream_dealloc(session->stream_template, NULL);
        if (status)
            return status;
    }

    srtp_crypto_free(session);

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_add_stream(srtp_t session, const srtp_policy_t *policy)
{
    srtp_err_status_t status;
    srtp_stream_t tmp;

    if ((session == NULL) || (policy == NULL) ||
        (!srtp_validate_policy_master_keys(policy)))
        return srtp_err_status_bad_param;

    status = srtp_stream_alloc(&tmp, policy);
    if (status) {
        return status;
    }

    status = srtp_stream_init(tmp, policy);
    if (status) {
        srtp_stream_dealloc(tmp, NULL);
        return status;
    }

    switch (policy->ssrc.type) {
    case (ssrc_any_outbound):
        if (session->stream_template) {
            srtp_stream_dealloc(tmp, NULL);
            return srtp_err_status_bad_param;
        }
        session->stream_template = tmp;
        session->stream_template->direction = dir_srtp_sender;
        break;
    case (ssrc_any_inbound):
        if (session->stream_template) {
            srtp_stream_dealloc(tmp, NULL);
            return srtp_err_status_bad_param;
        }
        session->stream_template = tmp;
        session->stream_template->direction = dir_srtp_receiver;
        break;
    case (ssrc_specific):
        tmp->next = session->stream_list;
        session->stream_list = tmp;
        break;
    case (ssrc_undefined):
    default:
        srtp_stream_dealloc(tmp, NULL);
        return srtp_err_status_bad_param;
    }

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_create(srtp_t *session,
                              const srtp_policy_t *policy)
{
    srtp_err_status_t stat;
    srtp_ctx_t *ctx;

    if (session == NULL)
        return srtp_err_status_bad_param;

    ctx = (srtp_ctx_t *)srtp_crypto_alloc(sizeof(srtp_ctx_t));
    if (ctx == NULL)
        return srtp_err_status_alloc_fail;
    *session = ctx;

    ctx->stream_template = NULL;
    ctx->stream_list = NULL;
    ctx->user_data = NULL;
    while (policy != NULL) {
        stat = srtp_add_stream(ctx, policy);
        if (stat) {

            srtp_dealloc(*session);
            *session = NULL;
            return stat;
        }

        policy = policy->next;
    }

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_remove_stream(srtp_t session, uint32_t ssrc)
{
    srtp_stream_ctx_t *stream, *last_stream;
    srtp_err_status_t status;

    if (session == NULL)
        return srtp_err_status_bad_param;

    last_stream = stream = session->stream_list;
    while ((stream != NULL) && (ssrc != stream->ssrc)) {
        last_stream = stream;
        stream = stream->next;
    }
    if (stream == NULL)
        return srtp_err_status_no_ctx;

    if (last_stream == stream)

        session->stream_list = stream->next;
    else
        last_stream->next = stream->next;

    status = srtp_stream_dealloc(stream, session->stream_template);
    if (status)
        return status;

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_update(srtp_t session, const srtp_policy_t *policy)
{
    srtp_err_status_t stat;

    if ((session == NULL) || (policy == NULL) ||
        (!srtp_validate_policy_master_keys(policy))) {
        return srtp_err_status_bad_param;
    }

    while (policy != NULL) {
        stat = srtp_update_stream(session, policy);
        if (stat) {
            return stat;
        }

        policy = policy->next;
    }
    return srtp_err_status_ok;
}

static srtp_err_status_t update_template_streams(srtp_t session,
                                                 const srtp_policy_t *policy)
{
    srtp_err_status_t status;
    srtp_stream_t new_stream_template;
    srtp_stream_t new_stream_list = NULL;

    if (session->stream_template == NULL) {
        return srtp_err_status_bad_param;
    }

    status = srtp_stream_alloc(&new_stream_template, policy);
    if (status) {
        return status;
    }

    status = srtp_stream_init(new_stream_template, policy);
    if (status) {
        srtp_crypto_free(new_stream_template);
        return status;
    }

    for (;;) {
        srtp_stream_t stream;
        uint32_t ssrc;
        srtp_xtd_seq_num_t old_index;
        srtp_rdb_t old_rtcp_rdb;

        stream = session->stream_list;
        while ((stream != NULL) &&
               (stream->session_keys[0].rtp_auth !=
                session->stream_template->session_keys[0].rtp_auth)) {
            stream = stream->next;
        }
        if (stream == NULL) {

            break;
        }

        ssrc = stream->ssrc;
        old_index = stream->rtp_rdbx.index;
        old_rtcp_rdb = stream->rtcp_rdb;

        status = srtp_remove_stream(session, ssrc);
        if (status) {

            while (new_stream_list != NULL) {
                srtp_stream_t next = new_stream_list->next;
                srtp_stream_dealloc(new_stream_list, new_stream_template);
                new_stream_list = next;
            }
            srtp_stream_dealloc(new_stream_template, NULL);
            return status;
        }

        status = srtp_stream_clone(new_stream_template, ssrc, &stream);
        if (status) {

            while (new_stream_list != NULL) {
                srtp_stream_t next = new_stream_list->next;
                srtp_stream_dealloc(new_stream_list, new_stream_template);
                new_stream_list = next;
            }
            srtp_stream_dealloc(new_stream_template, NULL);
            return status;
        }

        stream->next = new_stream_list;
        new_stream_list = stream;

        stream->rtp_rdbx.index = old_index;
        stream->rtcp_rdb = old_rtcp_rdb;
    }

    srtp_stream_dealloc(session->stream_template, NULL);

    session->stream_template = new_stream_template;

    if (new_stream_list) {
        srtp_stream_t tail = new_stream_list;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = session->stream_list;
        session->stream_list = new_stream_list;
    }
    return status;
}

static srtp_err_status_t update_stream(srtp_t session,
                                       const srtp_policy_t *policy)
{
    srtp_err_status_t status;
    srtp_xtd_seq_num_t old_index;
    srtp_rdb_t old_rtcp_rdb;
    srtp_stream_t stream;

    stream = srtp_get_stream(session, htonl(policy->ssrc.value));
    if (stream == NULL) {
        return srtp_err_status_bad_param;
    }

    old_index = stream->rtp_rdbx.index;
    old_rtcp_rdb = stream->rtcp_rdb;

    status = srtp_remove_stream(session, htonl(policy->ssrc.value));
    if (status) {
        return status;
    }

    status = srtp_add_stream(session, policy);
    if (status) {
        return status;
    }

    stream = srtp_get_stream(session, htonl(policy->ssrc.value));
    if (stream == NULL) {
        return srtp_err_status_fail;
    }

    stream->rtp_rdbx.index = old_index;
    stream->rtcp_rdb = old_rtcp_rdb;

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_update_stream(srtp_t session,
                                     const srtp_policy_t *policy)
{
    srtp_err_status_t status;

    if ((session == NULL) || (policy == NULL) ||
        (!srtp_validate_policy_master_keys(policy)))
        return srtp_err_status_bad_param;

    switch (policy->ssrc.type) {
    case (ssrc_any_outbound):
    case (ssrc_any_inbound):
        status = update_template_streams(session, policy);
        break;
    case (ssrc_specific):
        status = update_stream(session, policy);
        break;
    case (ssrc_undefined):
    default:
        return srtp_err_status_bad_param;
    }

    return status;
}

void srtp_crypto_policy_set_rtp_default(srtp_crypto_policy_t *p)
{
    p->cipher_type = SRTP_AES_ICM_128;
    p->cipher_key_len =
        SRTP_AES_ICM_128_KEY_LEN_WSALT;
    p->auth_type = SRTP_HMAC_SHA1;
    p->auth_key_len = 20;
    p->auth_tag_len = 10;
    p->sec_serv = sec_serv_conf_and_auth;
}

void srtp_crypto_policy_set_rtcp_default(srtp_crypto_policy_t *p)
{
    p->cipher_type = SRTP_AES_ICM_128;
    p->cipher_key_len =
        SRTP_AES_ICM_128_KEY_LEN_WSALT;
    p->auth_type = SRTP_HMAC_SHA1;
    p->auth_key_len = 20;
    p->auth_tag_len = 10;
    p->sec_serv = sec_serv_conf_and_auth;
}

void srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(srtp_crypto_policy_t *p)
{

    p->cipher_type = SRTP_AES_ICM_128;
    p->cipher_key_len =
        SRTP_AES_ICM_128_KEY_LEN_WSALT;
    p->auth_type = SRTP_HMAC_SHA1;
    p->auth_key_len = 20;
    p->auth_tag_len = 4;
    p->sec_serv = sec_serv_conf_and_auth;
}

void srtp_crypto_policy_set_aes_cm_128_null_auth(srtp_crypto_policy_t *p)
{

    p->cipher_type = SRTP_AES_ICM_128;
    p->cipher_key_len =
        SRTP_AES_ICM_128_KEY_LEN_WSALT;
    p->auth_type = SRTP_NULL_AUTH;
    p->auth_key_len = 0;
    p->auth_tag_len = 0;
    p->sec_serv = sec_serv_conf;
}

void srtp_crypto_policy_set_null_cipher_hmac_sha1_80(srtp_crypto_policy_t *p)
{

    p->cipher_type = SRTP_NULL_CIPHER;
    p->cipher_key_len = 0;
    p->auth_type = SRTP_HMAC_SHA1;
    p->auth_key_len = 20;
    p->auth_tag_len = 10;
    p->sec_serv = sec_serv_auth;
}

void srtp_crypto_policy_set_null_cipher_hmac_null(srtp_crypto_policy_t *p)
{

    p->cipher_type = SRTP_NULL_CIPHER;
    p->cipher_key_len = 0;
    p->auth_type = SRTP_NULL_AUTH;
    p->auth_key_len = 0;
    p->auth_tag_len = 0;
    p->sec_serv = sec_serv_none;
}

void srtp_crypto_policy_set_aes_cm_256_hmac_sha1_80(srtp_crypto_policy_t *p)
{

    p->cipher_type = SRTP_AES_ICM_256;
    p->cipher_key_len = SRTP_AES_ICM_256_KEY_LEN_WSALT;
    p->auth_type = SRTP_HMAC_SHA1;
    p->auth_key_len = 20;
    p->auth_tag_len = 10;
    p->sec_serv = sec_serv_conf_and_auth;
}

void srtp_crypto_policy_set_aes_cm_256_hmac_sha1_32(srtp_crypto_policy_t *p)
{

    p->cipher_type = SRTP_AES_ICM_256;
    p->cipher_key_len = SRTP_AES_ICM_256_KEY_LEN_WSALT;
    p->auth_type = SRTP_HMAC_SHA1;
    p->auth_key_len = 20;
    p->auth_tag_len = 4;
    p->sec_serv = sec_serv_conf_and_auth;
}

void srtp_crypto_policy_set_aes_cm_256_null_auth(srtp_crypto_policy_t *p)
{
    p->cipher_type = SRTP_AES_ICM_256;
    p->cipher_key_len = SRTP_AES_ICM_256_KEY_LEN_WSALT;
    p->auth_type = SRTP_NULL_AUTH;
    p->auth_key_len = 0;
    p->auth_tag_len = 0;
    p->sec_serv = sec_serv_conf;
}

#ifdef OPENSSL
void srtp_crypto_policy_set_aes_cm_192_hmac_sha1_80(srtp_crypto_policy_t *p)
{

    p->cipher_type = SRTP_AES_ICM_192;
    p->cipher_key_len = SRTP_AES_ICM_192_KEY_LEN_WSALT;
    p->auth_type = SRTP_HMAC_SHA1;
    p->auth_key_len = 20;
    p->auth_tag_len = 10;
    p->sec_serv = sec_serv_conf_and_auth;
}

void srtp_crypto_policy_set_aes_cm_192_hmac_sha1_32(srtp_crypto_policy_t *p)
{

    p->cipher_type = SRTP_AES_ICM_192;
    p->cipher_key_len = SRTP_AES_ICM_192_KEY_LEN_WSALT;
    p->auth_type = SRTP_HMAC_SHA1;
    p->auth_key_len = 20;
    p->auth_tag_len = 4;
    p->sec_serv = sec_serv_conf_and_auth;
}

void srtp_crypto_policy_set_aes_cm_192_null_auth(srtp_crypto_policy_t *p)
{
    p->cipher_type = SRTP_AES_ICM_192;
    p->cipher_key_len = SRTP_AES_ICM_192_KEY_LEN_WSALT;
    p->auth_type = SRTP_NULL_AUTH;
    p->auth_key_len = 0;
    p->auth_tag_len = 0;
    p->sec_serv = sec_serv_conf;
}

void srtp_crypto_policy_set_aes_gcm_128_8_auth(srtp_crypto_policy_t *p)
{
    p->cipher_type = SRTP_AES_GCM_128;
    p->cipher_key_len = SRTP_AES_GCM_128_KEY_LEN_WSALT;
    p->auth_type = SRTP_NULL_AUTH;
    p->auth_key_len = 0;
    p->auth_tag_len = 8;
    p->sec_serv = sec_serv_conf_and_auth;
}

void srtp_crypto_policy_set_aes_gcm_256_8_auth(srtp_crypto_policy_t *p)
{
    p->cipher_type = SRTP_AES_GCM_256;
    p->cipher_key_len = SRTP_AES_GCM_256_KEY_LEN_WSALT;
    p->auth_type = SRTP_NULL_AUTH;
    p->auth_key_len = 0;
    p->auth_tag_len = 8;
    p->sec_serv = sec_serv_conf_and_auth;
}

void srtp_crypto_policy_set_aes_gcm_128_8_only_auth(srtp_crypto_policy_t *p)
{
    p->cipher_type = SRTP_AES_GCM_128;
    p->cipher_key_len = SRTP_AES_GCM_128_KEY_LEN_WSALT;
    p->auth_type = SRTP_NULL_AUTH;
    p->auth_key_len = 0;
    p->auth_tag_len = 8;
    p->sec_serv = sec_serv_auth;
}

void srtp_crypto_policy_set_aes_gcm_256_8_only_auth(srtp_crypto_policy_t *p)
{
    p->cipher_type = SRTP_AES_GCM_256;
    p->cipher_key_len = SRTP_AES_GCM_256_KEY_LEN_WSALT;
    p->auth_type = SRTP_NULL_AUTH;
    p->auth_key_len = 0;
    p->auth_tag_len = 8;
    p->sec_serv = sec_serv_auth;
}

void srtp_crypto_policy_set_aes_gcm_128_16_auth(srtp_crypto_policy_t *p)
{
    p->cipher_type = SRTP_AES_GCM_128;
    p->cipher_key_len = SRTP_AES_GCM_128_KEY_LEN_WSALT;
    p->auth_type = SRTP_NULL_AUTH;
    p->auth_key_len = 0;
    p->auth_tag_len = 16;
    p->sec_serv = sec_serv_conf_and_auth;
}

void srtp_crypto_policy_set_aes_gcm_256_16_auth(srtp_crypto_policy_t *p)
{
    p->cipher_type = SRTP_AES_GCM_256;
    p->cipher_key_len = SRTP_AES_GCM_256_KEY_LEN_WSALT;
    p->auth_type = SRTP_NULL_AUTH;
    p->auth_key_len = 0;
    p->auth_tag_len = 16;
    p->sec_serv = sec_serv_conf_and_auth;
}

#endif

static srtp_err_status_t srtp_calc_aead_iv_srtcp(
    srtp_session_keys_t *session_keys,
    v128_t *iv,
    uint32_t seq_num,
    srtcp_hdr_t *hdr)
{
    v128_t in;
    v128_t salt;

    memset(&in, 0, sizeof(v128_t));
    memset(&salt, 0, sizeof(v128_t));

    in.v16[0] = 0;
    memcpy(&in.v16[1], &hdr->ssrc, 4);
    in.v16[3] = 0;

    if (seq_num & 0x80000000UL) {
        return srtp_err_status_bad_param;
    }
    in.v32[2] = htonl(seq_num);

    debug_print(mod_srtp, "Pre-salted RTCP IV = %s\n", v128_hex_string(&in));

    memcpy(salt.v8, session_keys->c_salt, 12);
    debug_print(mod_srtp, "RTCP SALT = %s\n", v128_hex_string(&salt));

    v128_xor(iv, &in, &salt);

    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_protect_rtcp_aead(
    srtp_t ctx,
    srtp_stream_ctx_t *stream,
    void *rtcp_hdr,
    unsigned int *pkt_octet_len,
    srtp_session_keys_t *session_keys,
    unsigned int use_mki)
{
    srtcp_hdr_t *hdr = (srtcp_hdr_t *)rtcp_hdr;
    uint32_t *enc_start;
    uint32_t *trailer;
    unsigned int enc_octet_len = 0;
    uint8_t *auth_tag = NULL;
    srtp_err_status_t status;
    uint32_t tag_len;
    uint32_t seq_num;
    v128_t iv;
    uint32_t tseq;
    unsigned int mki_size = 0;

    tag_len = srtp_auth_get_tag_length(session_keys->rtcp_auth);

    enc_start = (uint32_t *)hdr + uint32s_in_rtcp_header;
    enc_octet_len = *pkt_octet_len - octets_in_rtcp_header;

    trailer = (uint32_t *)((char *)enc_start + enc_octet_len + tag_len);

    if (stream->rtcp_services & sec_serv_conf) {
        *trailer = htonl(SRTCP_E_BIT);
    } else {
        enc_start = NULL;
        enc_octet_len = 0;

        *trailer = 0x00000000;
    }

    mki_size = srtp_inject_mki((uint8_t *)hdr + *pkt_octet_len + tag_len +
                                   sizeof(srtcp_trailer_t),
                               session_keys, use_mki);

    auth_tag = (uint8_t *)hdr + *pkt_octet_len;

    status = srtp_rdb_increment(&stream->rtcp_rdb);
    if (status) {
        return status;
    }
    seq_num = srtp_rdb_get_value(&stream->rtcp_rdb);
    *trailer |= htonl(seq_num);
    debug_print(mod_srtp, "srtcp index: %x", seq_num);

    status = srtp_calc_aead_iv_srtcp(session_keys, &iv, seq_num, hdr);
    if (status) {
        return srtp_err_status_cipher_fail;
    }
    status = srtp_cipher_set_iv(session_keys->rtcp_cipher, (uint8_t *)&iv,
                                srtp_direction_encrypt);
    if (status) {
        return srtp_err_status_cipher_fail;
    }

    if (enc_start) {

        status = srtp_cipher_set_aad(session_keys->rtcp_cipher, (uint8_t *)hdr,
                                     octets_in_rtcp_header);
        if (status) {
            return (srtp_err_status_cipher_fail);
        }
    } else {

        status = srtp_cipher_set_aad(session_keys->rtcp_cipher, (uint8_t *)hdr,
                                     *pkt_octet_len);
        if (status) {
            return (srtp_err_status_cipher_fail);
        }
    }

    tseq = *trailer;
    status = srtp_cipher_set_aad(session_keys->rtcp_cipher, (uint8_t *)&tseq,
                                 sizeof(srtcp_trailer_t));
    if (status) {
        return (srtp_err_status_cipher_fail);
    }

    if (enc_start) {
        status = srtp_cipher_encrypt(session_keys->rtcp_cipher,
                                     (uint8_t *)enc_start, &enc_octet_len);
        if (status) {
            return srtp_err_status_cipher_fail;
        }

        status = srtp_cipher_get_tag(session_keys->rtcp_cipher,
                                     (uint8_t *)auth_tag, &tag_len);
        if (status) {
            return (srtp_err_status_cipher_fail);
        }
        enc_octet_len += tag_len;
    } else {

        unsigned int nolen = 0;
        status = srtp_cipher_encrypt(session_keys->rtcp_cipher, NULL, &nolen);
        if (status) {
            return srtp_err_status_cipher_fail;
        }

        status = srtp_cipher_get_tag(session_keys->rtcp_cipher,
                                     (uint8_t *)auth_tag, &tag_len);
        if (status) {
            return (srtp_err_status_cipher_fail);
        }
        enc_octet_len += tag_len;
    }

    *pkt_octet_len += (tag_len + sizeof(srtcp_trailer_t));

    *pkt_octet_len += mki_size;

    return srtp_err_status_ok;
}

static srtp_err_status_t srtp_unprotect_rtcp_aead(
    srtp_t ctx,
    srtp_stream_ctx_t *stream,
    void *srtcp_hdr,
    unsigned int *pkt_octet_len,
    srtp_session_keys_t *session_keys,
    unsigned int use_mki)
{
    srtcp_hdr_t *hdr = (srtcp_hdr_t *)srtcp_hdr;
    uint32_t *enc_start;
    uint32_t *trailer;
    unsigned int enc_octet_len = 0;
    uint8_t *auth_tag = NULL;
    srtp_err_status_t status;
    int tag_len;
    unsigned int tmp_len;
    uint32_t seq_num;
    v128_t iv;
    uint32_t tseq;
    unsigned int mki_size = 0;

    tag_len = srtp_auth_get_tag_length(session_keys->rtcp_auth);

    if (use_mki) {
        mki_size = session_keys->mki_size;
    }

    trailer = (uint32_t *)((char *)hdr + *pkt_octet_len -
                           sizeof(srtcp_trailer_t) - mki_size);

    enc_octet_len = *pkt_octet_len - (octets_in_rtcp_header +
                                      sizeof(srtcp_trailer_t) + mki_size);
    auth_tag = (uint8_t *)hdr + *pkt_octet_len - tag_len - mki_size -
               sizeof(srtcp_trailer_t);

    if (*((unsigned char *)trailer) & SRTCP_E_BYTE_BIT) {
        enc_start = (uint32_t *)hdr + uint32s_in_rtcp_header;
    } else {
        enc_octet_len = 0;
        enc_start = NULL;
    }

    seq_num = ntohl(*trailer) & SRTCP_INDEX_MASK;
    debug_print(mod_srtp, "srtcp index: %x", seq_num);
    status = srtp_rdb_check(&stream->rtcp_rdb, seq_num);
    if (status) {
        return status;
    }

    status = srtp_calc_aead_iv_srtcp(session_keys, &iv, seq_num, hdr);
    if (status) {
        return srtp_err_status_cipher_fail;
    }
    status = srtp_cipher_set_iv(session_keys->rtcp_cipher, (uint8_t *)&iv,
                                srtp_direction_decrypt);
    if (status) {
        return srtp_err_status_cipher_fail;
    }

    if (enc_start) {

        status = srtp_cipher_set_aad(session_keys->rtcp_cipher, (uint8_t *)hdr,
                                     octets_in_rtcp_header);
        if (status) {
            return (srtp_err_status_cipher_fail);
        }
    } else {

        status = srtp_cipher_set_aad(
            session_keys->rtcp_cipher, (uint8_t *)hdr,
            (*pkt_octet_len - tag_len - sizeof(srtcp_trailer_t) - mki_size));
        if (status) {
            return (srtp_err_status_cipher_fail);
        }
    }

    tseq = *trailer;
    status = srtp_cipher_set_aad(session_keys->rtcp_cipher, (uint8_t *)&tseq,
                                 sizeof(srtcp_trailer_t));
    if (status) {
        return (srtp_err_status_cipher_fail);
    }

    if (enc_start) {
        status = srtp_cipher_decrypt(session_keys->rtcp_cipher,
                                     (uint8_t *)enc_start, &enc_octet_len);
        if (status) {
            return status;
        }
    } else {

        tmp_len = tag_len;
        status = srtp_cipher_decrypt(session_keys->rtcp_cipher,
                                     (uint8_t *)auth_tag, &tmp_len);
        if (status) {
            return status;
        }
    }

    *pkt_octet_len -= (tag_len + sizeof(srtcp_trailer_t) + mki_size);

    if (stream->direction != dir_srtp_receiver) {
        if (stream->direction == dir_unknown) {
            stream->direction = dir_srtp_receiver;
        } else {
            srtp_handle_event(ctx, stream, event_ssrc_collision);
        }
    }

    if (stream == ctx->stream_template) {
        srtp_stream_ctx_t *new_stream;

        status =
            srtp_stream_clone(ctx->stream_template, hdr->ssrc, &new_stream);
        if (status) {
            return status;
        }

        new_stream->next = ctx->stream_list;
        ctx->stream_list = new_stream;

        stream = new_stream;
    }

    srtp_rdb_add_index(&stream->rtcp_rdb, seq_num);

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_protect_rtcp(srtp_t ctx,
                                    void *rtcp_hdr,
                                    int *pkt_octet_len)
{
    return srtp_protect_rtcp_mki(ctx, rtcp_hdr, pkt_octet_len, 0, 0);
}

srtp_err_status_t srtp_protect_rtcp_mki(srtp_t ctx,
                                        void *rtcp_hdr,
                                        int *pkt_octet_len,
                                        unsigned int use_mki,
                                        unsigned int mki_index)
{
    srtcp_hdr_t *hdr = (srtcp_hdr_t *)rtcp_hdr;
    uint32_t *enc_start;
    uint32_t *auth_start;
    uint32_t *trailer;
    unsigned int enc_octet_len = 0;
    uint8_t *auth_tag = NULL;
    srtp_err_status_t status;
    int tag_len;
    srtp_stream_ctx_t *stream;
    uint32_t prefix_len;
    uint32_t seq_num;
    unsigned int mki_size = 0;
    srtp_session_keys_t *session_keys = NULL;

    if (*pkt_octet_len < octets_in_rtcp_header)
        return srtp_err_status_bad_param;

    stream = srtp_get_stream(ctx, hdr->ssrc);
    if (stream == NULL) {
        if (ctx->stream_template != NULL) {
            srtp_stream_ctx_t *new_stream;

            status =
                srtp_stream_clone(ctx->stream_template, hdr->ssrc, &new_stream);
            if (status)
                return status;

            new_stream->next = ctx->stream_list;
            ctx->stream_list = new_stream;

            stream = new_stream;
        } else {

            return srtp_err_status_no_ctx;
        }
    }

    if (stream->direction != dir_srtp_sender) {
        if (stream->direction == dir_unknown) {
            stream->direction = dir_srtp_sender;
        } else {
            srtp_handle_event(ctx, stream, event_ssrc_collision);
        }
    }

    session_keys =
        srtp_get_session_keys_with_mki_index(stream, use_mki, mki_index);

    if (session_keys == NULL)
        return srtp_err_status_bad_mki;

    if (session_keys->rtp_cipher->algorithm == SRTP_AES_GCM_128 ||
        session_keys->rtp_cipher->algorithm == SRTP_AES_GCM_256) {
        return srtp_protect_rtcp_aead(ctx, stream, rtcp_hdr,
                                      (unsigned int *)pkt_octet_len,
                                      session_keys, use_mki);
    }

    tag_len = srtp_auth_get_tag_length(session_keys->rtcp_auth);

    enc_start = (uint32_t *)hdr + uint32s_in_rtcp_header;
    enc_octet_len = *pkt_octet_len - octets_in_rtcp_header;

    trailer = (uint32_t *)((char *)enc_start + enc_octet_len);

    if (stream->rtcp_services & sec_serv_conf) {
        *trailer = htonl(SRTCP_E_BIT);
    } else {
        enc_start = NULL;
        enc_octet_len = 0;

        *trailer = 0x00000000;
    }

    mki_size = srtp_inject_mki((uint8_t *)hdr + *pkt_octet_len +
                                   sizeof(srtcp_trailer_t),
                               session_keys, use_mki);

    auth_start = (uint32_t *)hdr;
    auth_tag =
        (uint8_t *)hdr + *pkt_octet_len + sizeof(srtcp_trailer_t) + mki_size;

    srtp_ekt_write_data(stream->ekt, auth_tag, tag_len, pkt_octet_len,
                        srtp_rdbx_get_packet_index(&stream->rtp_rdbx));

    status = srtp_rdb_increment(&stream->rtcp_rdb);
    if (status)
        return status;
    seq_num = srtp_rdb_get_value(&stream->rtcp_rdb);
    *trailer |= htonl(seq_num);
    debug_print(mod_srtp, "srtcp index: %x", seq_num);

    if (session_keys->rtcp_cipher->type->id == SRTP_AES_ICM_128 ||
        session_keys->rtcp_cipher->type->id == SRTP_AES_ICM_192 ||
        session_keys->rtcp_cipher->type->id == SRTP_AES_ICM_256) {
        v128_t iv;

        iv.v32[0] = 0;
        iv.v32[1] = hdr->ssrc;
        iv.v32[2] = htonl(seq_num >> 16);
        iv.v32[3] = htonl(seq_num << 16);
        status = srtp_cipher_set_iv(session_keys->rtcp_cipher, (uint8_t *)&iv,
                                    srtp_direction_encrypt);

    } else {
        v128_t iv;

        iv.v32[0] = 0;
        iv.v32[1] = 0;
        iv.v32[2] = 0;
        iv.v32[3] = htonl(seq_num);
        status = srtp_cipher_set_iv(session_keys->rtcp_cipher, (uint8_t *)&iv,
                                    srtp_direction_encrypt);
    }
    if (status)
        return srtp_err_status_cipher_fail;

    if (auth_start) {

        prefix_len = srtp_auth_get_prefix_length(session_keys->rtcp_auth);
        status = srtp_cipher_output(session_keys->rtcp_cipher, auth_tag,
                                    &prefix_len);

        debug_print(mod_srtp, "keystream prefix: %s",
                    srtp_octet_string_hex_string(auth_tag, prefix_len));

        if (status)
            return srtp_err_status_cipher_fail;
    }

    if (enc_start) {
        status = srtp_cipher_encrypt(session_keys->rtcp_cipher,
                                     (uint8_t *)enc_start, &enc_octet_len);
        if (status)
            return srtp_err_status_cipher_fail;
    }

    srtp_auth_start(session_keys->rtcp_auth);

    status =
        srtp_auth_compute(session_keys->rtcp_auth, (uint8_t *)auth_start,
                          (*pkt_octet_len) + sizeof(srtcp_trailer_t), auth_tag);
    debug_print(mod_srtp, "srtcp auth tag:    %s",
                srtp_octet_string_hex_string(auth_tag, tag_len));
    if (status)
        return srtp_err_status_auth_fail;

    *pkt_octet_len += (tag_len + sizeof(srtcp_trailer_t));

    *pkt_octet_len += mki_size;

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_unprotect_rtcp(srtp_t ctx,
                                      void *srtcp_hdr,
                                      int *pkt_octet_len)
{
    return srtp_unprotect_rtcp_mki(ctx, srtcp_hdr, pkt_octet_len, 0);
}

srtp_err_status_t srtp_unprotect_rtcp_mki(srtp_t ctx,
                                          void *srtcp_hdr,
                                          int *pkt_octet_len,
                                          unsigned int use_mki)
{
    srtcp_hdr_t *hdr = (srtcp_hdr_t *)srtcp_hdr;
    uint32_t *enc_start;
    uint32_t *auth_start;
    uint32_t *trailer;
    unsigned int enc_octet_len = 0;
    uint8_t *auth_tag = NULL;
    uint8_t tmp_tag[SRTP_MAX_TAG_LEN];
    uint8_t tag_copy[SRTP_MAX_TAG_LEN];
    srtp_err_status_t status;
    unsigned int auth_len;
    int tag_len;
    srtp_stream_ctx_t *stream;
    uint32_t prefix_len;
    uint32_t seq_num;
    int e_bit_in_packet;
    int sec_serv_confidentiality;
    unsigned int mki_size = 0;
    srtp_session_keys_t *session_keys = NULL;

    if (*pkt_octet_len < 0)
        return srtp_err_status_bad_param;

    if ((unsigned int)(*pkt_octet_len) <
        octets_in_rtcp_header + sizeof(srtcp_trailer_t))
        return srtp_err_status_bad_param;

    stream = srtp_get_stream(ctx, hdr->ssrc);
    if (stream == NULL) {
        if (ctx->stream_template != NULL) {
            stream = ctx->stream_template;

            if (stream->ekt != NULL) {
                status = srtp_stream_init_from_ekt(stream, srtcp_hdr,
                                                   *pkt_octet_len);
                if (status)
                    return status;
            }

            debug_print(mod_srtp,
                        "srtcp using provisional stream (SSRC: 0x%08x)",
                        ntohl(hdr->ssrc));
        } else {

            return srtp_err_status_no_ctx;
        }
    }

    if (use_mki) {
        session_keys = srtp_get_session_keys(
            stream, (uint8_t *)hdr, (const unsigned int *)pkt_octet_len,
            &mki_size);

        if (session_keys == NULL)
            return srtp_err_status_bad_mki;
    } else {
        session_keys = &stream->session_keys[0];
    }

    tag_len = srtp_auth_get_tag_length(session_keys->rtcp_auth);

    if (*pkt_octet_len < (int)(octets_in_rtcp_header + tag_len + mki_size +
                               sizeof(srtcp_trailer_t))) {
        return srtp_err_status_bad_param;
    }

    if (session_keys->rtp_cipher->algorithm == SRTP_AES_GCM_128 ||
        session_keys->rtp_cipher->algorithm == SRTP_AES_GCM_256) {
        return srtp_unprotect_rtcp_aead(ctx, stream, srtcp_hdr,
                                        (unsigned int *)pkt_octet_len,
                                        session_keys, mki_size);
    }

    sec_serv_confidentiality = stream->rtcp_services == sec_serv_conf ||
                               stream->rtcp_services == sec_serv_conf_and_auth;

    enc_octet_len = *pkt_octet_len - (octets_in_rtcp_header + tag_len +
                                      mki_size + sizeof(srtcp_trailer_t));

    trailer = (uint32_t *)((char *)hdr + *pkt_octet_len -
                           (tag_len + mki_size + sizeof(srtcp_trailer_t)));
    e_bit_in_packet =
        (*((unsigned char *)trailer) & SRTCP_E_BYTE_BIT) == SRTCP_E_BYTE_BIT;
    if (e_bit_in_packet != sec_serv_confidentiality) {
        return srtp_err_status_cant_check;
    }
    if (sec_serv_confidentiality) {
        enc_start = (uint32_t *)hdr + uint32s_in_rtcp_header;
    } else {
        enc_octet_len = 0;
        enc_start = NULL;
    }

    auth_start = (uint32_t *)hdr;

    auth_len = *pkt_octet_len - tag_len - mki_size;
    auth_tag = (uint8_t *)hdr + auth_len + mki_size;

    if (stream->ekt) {
        auth_tag -= srtp_ekt_octets_after_base_tag(stream->ekt);
        memcpy(tag_copy, auth_tag, tag_len);
        octet_string_set_to_zero(auth_tag, tag_len);
        auth_tag = tag_copy;
        auth_len += tag_len;
    }

    seq_num = ntohl(*trailer) & SRTCP_INDEX_MASK;
    debug_print(mod_srtp, "srtcp index: %x", seq_num);
    status = srtp_rdb_check(&stream->rtcp_rdb, seq_num);
    if (status)
        return status;

    if (session_keys->rtcp_cipher->type->id == SRTP_AES_ICM_128 ||
        session_keys->rtcp_cipher->type->id == SRTP_AES_ICM_192 ||
        session_keys->rtcp_cipher->type->id == SRTP_AES_ICM_256) {
        v128_t iv;

        iv.v32[0] = 0;
        iv.v32[1] = hdr->ssrc;
        iv.v32[2] = htonl(seq_num >> 16);
        iv.v32[3] = htonl(seq_num << 16);
        status = srtp_cipher_set_iv(session_keys->rtcp_cipher, (uint8_t *)&iv,
                                    srtp_direction_decrypt);

    } else {
        v128_t iv;

        iv.v32[0] = 0;
        iv.v32[1] = 0;
        iv.v32[2] = 0;
        iv.v32[3] = htonl(seq_num);
        status = srtp_cipher_set_iv(session_keys->rtcp_cipher, (uint8_t *)&iv,
                                    srtp_direction_decrypt);
    }
    if (status)
        return srtp_err_status_cipher_fail;

    srtp_auth_start(session_keys->rtcp_auth);

    status = srtp_auth_compute(session_keys->rtcp_auth, (uint8_t *)auth_start,
                               auth_len, tmp_tag);
    debug_print(mod_srtp, "srtcp computed tag:       %s",
                srtp_octet_string_hex_string(tmp_tag, tag_len));
    if (status)
        return srtp_err_status_auth_fail;

    debug_print(mod_srtp, "srtcp tag from packet:    %s",
                srtp_octet_string_hex_string(auth_tag, tag_len));
    if (octet_string_is_eq(tmp_tag, auth_tag, tag_len))
        return srtp_err_status_auth_fail;

    prefix_len = srtp_auth_get_prefix_length(session_keys->rtcp_auth);
    if (prefix_len) {
        status = srtp_cipher_output(session_keys->rtcp_cipher, auth_tag,
                                    &prefix_len);
        debug_print(mod_srtp, "keystream prefix: %s",
                    srtp_octet_string_hex_string(auth_tag, prefix_len));
        if (status)
            return srtp_err_status_cipher_fail;
    }

    if (enc_start) {
        status = srtp_cipher_decrypt(session_keys->rtcp_cipher,
                                     (uint8_t *)enc_start, &enc_octet_len);
        if (status)
            return srtp_err_status_cipher_fail;
    }

    *pkt_octet_len -= (tag_len + sizeof(srtcp_trailer_t));

    *pkt_octet_len -= mki_size;

    *pkt_octet_len -= srtp_ekt_octets_after_base_tag(stream->ekt);

    if (stream->direction != dir_srtp_receiver) {
        if (stream->direction == dir_unknown) {
            stream->direction = dir_srtp_receiver;
        } else {
            srtp_handle_event(ctx, stream, event_ssrc_collision);
        }
    }

    if (stream == ctx->stream_template) {
        srtp_stream_ctx_t *new_stream;

        status =
            srtp_stream_clone(ctx->stream_template, hdr->ssrc, &new_stream);
        if (status)
            return status;

        new_stream->next = ctx->stream_list;
        ctx->stream_list = new_stream;

        stream = new_stream;
    }

    srtp_rdb_add_index(&stream->rtcp_rdb, seq_num);

    return srtp_err_status_ok;
}

void srtp_set_user_data(srtp_t ctx, void *data)
{
    ctx->user_data = data;
}

void *srtp_get_user_data(srtp_t ctx)
{
    return ctx->user_data;
}

srtp_err_status_t srtp_crypto_policy_set_from_profile_for_rtp(
    srtp_crypto_policy_t *policy,
    srtp_profile_t profile)
{

    switch (profile) {
    case srtp_profile_aes128_cm_sha1_80:
        srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(policy);
        break;
    case srtp_profile_aes128_cm_sha1_32:
        srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(policy);
        break;
    case srtp_profile_null_sha1_80:
        srtp_crypto_policy_set_null_cipher_hmac_sha1_80(policy);
        break;
#if defined(OPENSSL)
    case srtp_profile_aead_aes_128_gcm:
        srtp_crypto_policy_set_aes_gcm_128_16_auth(policy);
        break;
    case srtp_profile_aead_aes_256_gcm:
        srtp_crypto_policy_set_aes_gcm_256_16_auth(policy);
        break;
#endif

    case srtp_profile_null_sha1_32:
    default:
        return srtp_err_status_bad_param;
    }

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_crypto_policy_set_from_profile_for_rtcp(
    srtp_crypto_policy_t *policy,
    srtp_profile_t profile)
{

    switch (profile) {
    case srtp_profile_aes128_cm_sha1_80:
        srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(policy);
        break;
    case srtp_profile_aes128_cm_sha1_32:

        srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(policy);
        break;
    case srtp_profile_null_sha1_80:
        srtp_crypto_policy_set_null_cipher_hmac_sha1_80(policy);
        break;
#if defined(OPENSSL)
    case srtp_profile_aead_aes_128_gcm:
        srtp_crypto_policy_set_aes_gcm_128_16_auth(policy);
        break;
    case srtp_profile_aead_aes_256_gcm:
        srtp_crypto_policy_set_aes_gcm_256_16_auth(policy);
        break;
#endif

    case srtp_profile_null_sha1_32:
    default:
        return srtp_err_status_bad_param;
    }

    return srtp_err_status_ok;
}

void srtp_append_salt_to_key(uint8_t *key,
                             unsigned int bytes_in_key,
                             uint8_t *salt,
                             unsigned int bytes_in_salt)
{
    memcpy(key + bytes_in_key, salt, bytes_in_salt);
}

unsigned int srtp_profile_get_master_key_length(srtp_profile_t profile)
{
    switch (profile) {
    case srtp_profile_aes128_cm_sha1_80:
        return SRTP_AES_128_KEY_LEN;
        break;
    case srtp_profile_aes128_cm_sha1_32:
        return SRTP_AES_128_KEY_LEN;
        break;
    case srtp_profile_null_sha1_80:
        return SRTP_AES_128_KEY_LEN;
        break;
    case srtp_profile_aead_aes_128_gcm:
        return SRTP_AES_128_KEY_LEN;
        break;
    case srtp_profile_aead_aes_256_gcm:
        return SRTP_AES_256_KEY_LEN;
        break;

    case srtp_profile_null_sha1_32:
    default:
        return 0;
    }
}

unsigned int srtp_profile_get_master_salt_length(srtp_profile_t profile)
{
    switch (profile) {
    case srtp_profile_aes128_cm_sha1_80:
        return SRTP_SALT_LEN;
        break;
    case srtp_profile_aes128_cm_sha1_32:
        return SRTP_SALT_LEN;
        break;
    case srtp_profile_null_sha1_80:
        return SRTP_SALT_LEN;
        break;
    case srtp_profile_aead_aes_128_gcm:
        return SRTP_AEAD_SALT_LEN;
        break;
    case srtp_profile_aead_aes_256_gcm:
        return SRTP_AEAD_SALT_LEN;
        break;

    case srtp_profile_null_sha1_32:
    default:
        return 0;
    }
}

srtp_err_status_t stream_get_protect_trailer_length(srtp_stream_ctx_t *stream,
                                                    uint32_t is_rtp,
                                                    uint32_t use_mki,
                                                    uint32_t mki_index,
                                                    uint32_t *length)
{
    *length = 0;

    srtp_session_keys_t *session_key;

    if (use_mki) {
        if (mki_index >= stream->num_master_keys) {
            return srtp_err_status_bad_mki;
        }
        session_key = &stream->session_keys[mki_index];

        *length += session_key->mki_size;

    } else {
        session_key = &stream->session_keys[0];
    }
    if (is_rtp) {
        *length += srtp_auth_get_tag_length(session_key->rtp_auth);
    } else {
        *length += srtp_auth_get_tag_length(session_key->rtcp_auth);
        *length += sizeof(srtcp_trailer_t);
    }

    return srtp_err_status_ok;
}

srtp_err_status_t get_protect_trailer_length(srtp_t session,
                                             uint32_t is_rtp,
                                             uint32_t use_mki,
                                             uint32_t mki_index,
                                             uint32_t *length)
{
    srtp_stream_ctx_t *stream;

    if (session == NULL) {
        return srtp_err_status_bad_param;
    }

    if (session->stream_template == NULL && session->stream_list == NULL) {
        return srtp_err_status_bad_param;
    }

    *length = 0;

    stream = session->stream_template;

    if (stream != NULL) {
        stream_get_protect_trailer_length(stream, is_rtp, use_mki, mki_index,
                                          length);
    }

    stream = session->stream_list;

    while (stream != NULL) {
        uint32_t temp_length;
        if (stream_get_protect_trailer_length(stream, is_rtp, use_mki,
                                              mki_index, &temp_length) ==
            srtp_err_status_ok) {
            if (temp_length > *length) {
                *length = temp_length;
            }
        }
        stream = stream->next;
    }

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_get_protect_trailer_length(srtp_t session,
                                                  uint32_t use_mki,
                                                  uint32_t mki_index,
                                                  uint32_t *length)
{
    return get_protect_trailer_length(session, 1, use_mki, mki_index, length);
}

srtp_err_status_t srtp_get_protect_rtcp_trailer_length(srtp_t session,
                                                       uint32_t use_mki,
                                                       uint32_t mki_index,
                                                       uint32_t *length)
{
    return get_protect_trailer_length(session, 0, use_mki, mki_index, length);
}

srtp_err_status_t srtp_set_debug_module(const char *mod_name, int v)
{
    return srtp_crypto_kernel_set_debug_module(mod_name, v);
}

srtp_err_status_t srtp_list_debug_modules(void)
{
    return srtp_crypto_kernel_list_debug_modules();
}

static srtp_log_handler_func_t *srtp_log_handler = NULL;
static void *srtp_log_handler_data = NULL;

void srtp_err_handler(srtp_err_reporting_level_t level, const char *msg)
{
    if (srtp_log_handler) {
        srtp_log_level_t log_level = srtp_log_level_error;
        switch (level) {
        case srtp_err_level_error:
            log_level = srtp_log_level_error;
            break;
        case srtp_err_level_warning:
            log_level = srtp_log_level_warning;
            break;
        case srtp_err_level_info:
            log_level = srtp_log_level_info;
            break;
        case srtp_err_level_debug:
            log_level = srtp_log_level_debug;
            break;
        }

        srtp_log_handler(log_level, msg, srtp_log_handler_data);
    }
}

srtp_err_status_t srtp_install_log_handler(srtp_log_handler_func_t func,
                                           void *data)
{

    if (srtp_log_handler) {
        srtp_install_err_report_handler(NULL);
    }
    srtp_log_handler = func;
    srtp_log_handler_data = data;
    if (srtp_log_handler) {
        srtp_install_err_report_handler(srtp_err_handler);
    }
    return srtp_err_status_ok;
}

srtp_err_status_t srtp_set_stream_roc(srtp_t session,
                                      uint32_t ssrc,
                                      uint32_t roc)
{
    srtp_stream_t stream;

    stream = srtp_get_stream(session, htonl(ssrc));
    if (stream == NULL)
        return srtp_err_status_bad_param;

    stream->pending_roc = roc;

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_get_stream_roc(srtp_t session,
                                      uint32_t ssrc,
                                      uint32_t *roc)
{
    srtp_stream_t stream;

    stream = srtp_get_stream(session, htonl(ssrc));
    if (stream == NULL)
        return srtp_err_status_bad_param;

    *roc = srtp_rdbx_get_roc(&stream->rtp_rdbx);

    return srtp_err_status_ok;
}
