// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
// Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OPENSSL_HEADER_X509_H
#define OPENSSL_HEADER_X509_H

#include <openssl/base.h>

#include <time.h>

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/cipher.h>
#include <openssl/conf.h>
#include <openssl/dh.h>
#include <openssl/dsa.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/lhash.h>
#include <openssl/obj.h>
#include <openssl/pkcs7.h>
#include <openssl/pool.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/stack.h>
#include <openssl/thread.h>
#include <openssl/x509v3_errors.h>

#if defined(__cplusplus)
extern "C" {
#endif

DEFINE_STACK_OF(X509)

DECLARE_ASN1_ITEM(X509)

OPENSSL_EXPORT int X509_up_ref(X509 *x509);

OPENSSL_EXPORT STACK_OF(X509) *X509_chain_up_ref(STACK_OF(X509) *chain);

OPENSSL_EXPORT X509 *X509_dup(X509 *x509);

OPENSSL_EXPORT void X509_free(X509 *x509);

OPENSSL_EXPORT X509 *d2i_X509(X509 **out, const uint8_t **inp, long len);

OPENSSL_EXPORT X509 *X509_parse_from_buffer(CRYPTO_BUFFER *buf);

OPENSSL_EXPORT int i2d_X509(X509 *x509, uint8_t **outp);

#define X509_VERSION_1 0
#define X509_VERSION_2 1
#define X509_VERSION_3 2

OPENSSL_EXPORT long X509_get_version(const X509 *x509);

OPENSSL_EXPORT const ASN1_INTEGER *X509_get0_serialNumber(const X509 *x509);

OPENSSL_EXPORT const ASN1_TIME *X509_get0_notBefore(const X509 *x509);

OPENSSL_EXPORT const ASN1_TIME *X509_get0_notAfter(const X509 *x509);

OPENSSL_EXPORT X509_NAME *X509_get_issuer_name(const X509 *x509);

OPENSSL_EXPORT X509_NAME *X509_get_subject_name(const X509 *x509);

OPENSSL_EXPORT X509_PUBKEY *X509_get_X509_PUBKEY(const X509 *x509);

OPENSSL_EXPORT EVP_PKEY *X509_get0_pubkey(const X509 *x509);

OPENSSL_EXPORT EVP_PKEY *X509_get_pubkey(const X509 *x509);

OPENSSL_EXPORT ASN1_BIT_STRING *X509_get0_pubkey_bitstr(const X509 *x509);

OPENSSL_EXPORT int X509_check_private_key(const X509 *x509,
                                          const EVP_PKEY *pkey);

OPENSSL_EXPORT void X509_get0_uids(const X509 *x509,
                                   const ASN1_BIT_STRING **out_issuer_uid,
                                   const ASN1_BIT_STRING **out_subject_uid);

#define EXFLAG_BCONS 0x1

#define EXFLAG_KUSAGE 0x2

#define EXFLAG_XKUSAGE 0x4

#define EXFLAG_CA 0x10

#define EXFLAG_SI 0x20

#define EXFLAG_V1 0x40

#define EXFLAG_INVALID 0x80

#define EXFLAG_SET 0x100

#define EXFLAG_CRITICAL 0x200

#define EXFLAG_SS 0x2000

OPENSSL_EXPORT uint32_t X509_get_extension_flags(X509 *x509);

OPENSSL_EXPORT long X509_get_pathlen(X509 *x509);

#define X509v3_KU_DIGITAL_SIGNATURE 0x0080
#define X509v3_KU_NON_REPUDIATION 0x0040
#define X509v3_KU_KEY_ENCIPHERMENT 0x0020
#define X509v3_KU_DATA_ENCIPHERMENT 0x0010
#define X509v3_KU_KEY_AGREEMENT 0x0008
#define X509v3_KU_KEY_CERT_SIGN 0x0004
#define X509v3_KU_CRL_SIGN 0x0002
#define X509v3_KU_ENCIPHER_ONLY 0x0001
#define X509v3_KU_DECIPHER_ONLY 0x8000

OPENSSL_EXPORT uint32_t X509_get_key_usage(X509 *x509);

#define XKU_SSL_SERVER 0x1
#define XKU_SSL_CLIENT 0x2
#define XKU_SMIME 0x4
#define XKU_CODE_SIGN 0x8
#define XKU_SGC 0x10
#define XKU_OCSP_SIGN 0x20
#define XKU_TIMESTAMP 0x40
#define XKU_DVCS 0x80
#define XKU_ANYEKU 0x100

OPENSSL_EXPORT uint32_t X509_get_extended_key_usage(X509 *x509);

OPENSSL_EXPORT const ASN1_OCTET_STRING *X509_get0_subject_key_id(X509 *x509);

OPENSSL_EXPORT const ASN1_OCTET_STRING *X509_get0_authority_key_id(X509 *x509);

DEFINE_STACK_OF(GENERAL_NAME)
typedef STACK_OF(GENERAL_NAME) GENERAL_NAMES;

OPENSSL_EXPORT const GENERAL_NAMES *X509_get0_authority_issuer(X509 *x509);

OPENSSL_EXPORT const ASN1_INTEGER *X509_get0_authority_serial(X509 *x509);

OPENSSL_EXPORT const STACK_OF(X509_EXTENSION) *X509_get0_extensions(
    const X509 *x509);

OPENSSL_EXPORT int X509_get_ext_count(const X509 *x);

OPENSSL_EXPORT int X509_get_ext_by_NID(const X509 *x, int nid, int lastpos);

OPENSSL_EXPORT int X509_get_ext_by_OBJ(const X509 *x, const ASN1_OBJECT *obj,
                                       int lastpos);

OPENSSL_EXPORT int X509_get_ext_by_critical(const X509 *x, int crit,
                                            int lastpos);

OPENSSL_EXPORT X509_EXTENSION *X509_get_ext(const X509 *x, int loc);

OPENSSL_EXPORT void *X509_get_ext_d2i(const X509 *x509, int nid,
                                      int *out_critical, int *out_idx);

OPENSSL_EXPORT const X509_ALGOR *X509_get0_tbs_sigalg(const X509 *x509);

OPENSSL_EXPORT void X509_get0_signature(const ASN1_BIT_STRING **out_sig,
                                        const X509_ALGOR **out_alg,
                                        const X509 *x509);

OPENSSL_EXPORT int X509_get_signature_nid(const X509 *x509);

OPENSSL_EXPORT int i2d_X509_tbs(X509 *x509, unsigned char **outp);

OPENSSL_EXPORT int X509_verify(X509 *x509, EVP_PKEY *pkey);

OPENSSL_EXPORT STACK_OF(OPENSSL_STRING) *X509_get1_email(const X509 *x509);

OPENSSL_EXPORT STACK_OF(OPENSSL_STRING) *X509_get1_ocsp(const X509 *x509);

OPENSSL_EXPORT void X509_email_free(STACK_OF(OPENSSL_STRING) *sk);

OPENSSL_EXPORT int X509_cmp(const X509 *a, const X509 *b);

OPENSSL_EXPORT X509 *X509_new(void);

OPENSSL_EXPORT int X509_set_version(X509 *x509, long version);

OPENSSL_EXPORT int X509_set_serialNumber(X509 *x509,
                                         const ASN1_INTEGER *serial);

OPENSSL_EXPORT int X509_set1_notBefore(X509 *x509, const ASN1_TIME *tm);

OPENSSL_EXPORT int X509_set1_notAfter(X509 *x509, const ASN1_TIME *tm);

OPENSSL_EXPORT ASN1_TIME *X509_getm_notBefore(X509 *x509);

OPENSSL_EXPORT ASN1_TIME *X509_getm_notAfter(X509 *x);

OPENSSL_EXPORT int X509_set_issuer_name(X509 *x509, X509_NAME *name);

OPENSSL_EXPORT int X509_set_subject_name(X509 *x509, X509_NAME *name);

OPENSSL_EXPORT int X509_set_pubkey(X509 *x509, EVP_PKEY *pkey);

OPENSSL_EXPORT X509_EXTENSION *X509_delete_ext(X509 *x, int loc);

OPENSSL_EXPORT int X509_add_ext(X509 *x, const X509_EXTENSION *ex, int loc);

OPENSSL_EXPORT int X509_add1_ext_i2d(X509 *x, int nid, void *value, int crit,
                                     unsigned long flags);

OPENSSL_EXPORT int X509_sign(X509 *x509, EVP_PKEY *pkey, const EVP_MD *md);

OPENSSL_EXPORT int X509_sign_ctx(X509 *x509, EVP_MD_CTX *ctx);

OPENSSL_EXPORT int i2d_re_X509_tbs(X509 *x509, unsigned char **outp);

OPENSSL_EXPORT int X509_set1_signature_algo(X509 *x509, const X509_ALGOR *algo);

OPENSSL_EXPORT int X509_set1_signature_value(X509 *x509, const uint8_t *sig,
                                             size_t sig_len);

OPENSSL_EXPORT int i2d_X509_AUX(X509 *x509, uint8_t **outp);

OPENSSL_EXPORT X509 *d2i_X509_AUX(X509 **x509, const uint8_t **inp,
                                  long length);

OPENSSL_EXPORT int X509_alias_set1(X509 *x509, const uint8_t *name,
                                   ossl_ssize_t len);

OPENSSL_EXPORT int X509_keyid_set1(X509 *x509, const uint8_t *id,
                                   ossl_ssize_t len);

OPENSSL_EXPORT const uint8_t *X509_alias_get0(const X509 *x509, int *out_len);

OPENSSL_EXPORT const uint8_t *X509_keyid_get0(const X509 *x509, int *out_len);

OPENSSL_EXPORT int X509_add1_trust_object(X509 *x509, const ASN1_OBJECT *obj);

OPENSSL_EXPORT int X509_add1_reject_object(X509 *x509, const ASN1_OBJECT *obj);

OPENSSL_EXPORT void X509_trust_clear(X509 *x509);

OPENSSL_EXPORT void X509_reject_clear(X509 *x509);

DEFINE_STACK_OF(X509_CRL)
DEFINE_STACK_OF(X509_REVOKED)

OPENSSL_EXPORT int X509_CRL_up_ref(X509_CRL *crl);

OPENSSL_EXPORT X509_CRL *X509_CRL_dup(X509_CRL *crl);

OPENSSL_EXPORT void X509_CRL_free(X509_CRL *crl);

OPENSSL_EXPORT X509_CRL *d2i_X509_CRL(X509_CRL **out, const uint8_t **inp,
                                      long len);

OPENSSL_EXPORT int i2d_X509_CRL(X509_CRL *crl, uint8_t **outp);

OPENSSL_EXPORT int X509_CRL_match(const X509_CRL *a, const X509_CRL *b);

#define X509_CRL_VERSION_1 0
#define X509_CRL_VERSION_2 1

OPENSSL_EXPORT long X509_CRL_get_version(const X509_CRL *crl);

OPENSSL_EXPORT const ASN1_TIME *X509_CRL_get0_lastUpdate(const X509_CRL *crl);

OPENSSL_EXPORT const ASN1_TIME *X509_CRL_get0_nextUpdate(const X509_CRL *crl);

OPENSSL_EXPORT X509_NAME *X509_CRL_get_issuer(const X509_CRL *crl);

OPENSSL_EXPORT int X509_CRL_get0_by_serial(X509_CRL *crl, X509_REVOKED **out,
                                           const ASN1_INTEGER *serial);

OPENSSL_EXPORT int X509_CRL_get0_by_cert(X509_CRL *crl, X509_REVOKED **out,
                                         X509 *x509);

OPENSSL_EXPORT STACK_OF(X509_REVOKED) *X509_CRL_get_REVOKED(X509_CRL *crl);

OPENSSL_EXPORT const STACK_OF(X509_EXTENSION) *X509_CRL_get0_extensions(
    const X509_CRL *crl);

OPENSSL_EXPORT int X509_CRL_get_ext_count(const X509_CRL *x);

OPENSSL_EXPORT int X509_CRL_get_ext_by_NID(const X509_CRL *x, int nid,
                                           int lastpos);

OPENSSL_EXPORT int X509_CRL_get_ext_by_OBJ(const X509_CRL *x,
                                           const ASN1_OBJECT *obj, int lastpos);

OPENSSL_EXPORT int X509_CRL_get_ext_by_critical(const X509_CRL *x, int crit,
                                                int lastpos);

OPENSSL_EXPORT X509_EXTENSION *X509_CRL_get_ext(const X509_CRL *x, int loc);

OPENSSL_EXPORT void *X509_CRL_get_ext_d2i(const X509_CRL *crl, int nid,
                                          int *out_critical, int *out_idx);

OPENSSL_EXPORT void X509_CRL_get0_signature(const X509_CRL *crl,
                                            const ASN1_BIT_STRING **out_sig,
                                            const X509_ALGOR **out_alg);

OPENSSL_EXPORT int X509_CRL_get_signature_nid(const X509_CRL *crl);

OPENSSL_EXPORT int i2d_X509_CRL_tbs(X509_CRL *crl, unsigned char **outp);

OPENSSL_EXPORT int X509_CRL_verify(X509_CRL *crl, EVP_PKEY *pkey);

OPENSSL_EXPORT X509_CRL *X509_CRL_new(void);

OPENSSL_EXPORT int X509_CRL_set_version(X509_CRL *crl, long version);

OPENSSL_EXPORT int X509_CRL_set_issuer_name(X509_CRL *crl, X509_NAME *name);

OPENSSL_EXPORT int X509_CRL_set1_lastUpdate(X509_CRL *crl, const ASN1_TIME *tm);

OPENSSL_EXPORT int X509_CRL_set1_nextUpdate(X509_CRL *crl, const ASN1_TIME *tm);

OPENSSL_EXPORT int X509_CRL_add0_revoked(X509_CRL *crl, X509_REVOKED *rev);

OPENSSL_EXPORT int X509_CRL_sort(X509_CRL *crl);

OPENSSL_EXPORT X509_EXTENSION *X509_CRL_delete_ext(X509_CRL *x, int loc);

OPENSSL_EXPORT int X509_CRL_add_ext(X509_CRL *x, const X509_EXTENSION *ex,
                                    int loc);

OPENSSL_EXPORT int X509_CRL_add1_ext_i2d(X509_CRL *x, int nid, void *value,
                                         int crit, unsigned long flags);

OPENSSL_EXPORT int X509_CRL_sign(X509_CRL *crl, EVP_PKEY *pkey,
                                 const EVP_MD *md);

OPENSSL_EXPORT int X509_CRL_sign_ctx(X509_CRL *crl, EVP_MD_CTX *ctx);

OPENSSL_EXPORT int i2d_re_X509_CRL_tbs(X509_CRL *crl, unsigned char **outp);

OPENSSL_EXPORT int X509_CRL_set1_signature_algo(X509_CRL *crl,
                                                const X509_ALGOR *algo);

OPENSSL_EXPORT int X509_CRL_set1_signature_value(X509_CRL *crl,
                                                 const uint8_t *sig,
                                                 size_t sig_len);

OPENSSL_EXPORT X509_REVOKED *X509_REVOKED_new(void);

OPENSSL_EXPORT void X509_REVOKED_free(X509_REVOKED *rev);

OPENSSL_EXPORT X509_REVOKED *d2i_X509_REVOKED(X509_REVOKED **out,
                                              const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_X509_REVOKED(const X509_REVOKED *alg, uint8_t **outp);

OPENSSL_EXPORT X509_REVOKED *X509_REVOKED_dup(const X509_REVOKED *rev);

OPENSSL_EXPORT const ASN1_INTEGER *X509_REVOKED_get0_serialNumber(
    const X509_REVOKED *revoked);

OPENSSL_EXPORT int X509_REVOKED_set_serialNumber(X509_REVOKED *revoked,
                                                 const ASN1_INTEGER *serial);

OPENSSL_EXPORT const ASN1_TIME *X509_REVOKED_get0_revocationDate(
    const X509_REVOKED *revoked);

OPENSSL_EXPORT int X509_REVOKED_set_revocationDate(X509_REVOKED *revoked,
                                                   const ASN1_TIME *tm);

OPENSSL_EXPORT const STACK_OF(X509_EXTENSION) *X509_REVOKED_get0_extensions(
    const X509_REVOKED *r);

OPENSSL_EXPORT int X509_REVOKED_get_ext_count(const X509_REVOKED *x);

OPENSSL_EXPORT int X509_REVOKED_get_ext_by_NID(const X509_REVOKED *x, int nid,
                                               int lastpos);

OPENSSL_EXPORT int X509_REVOKED_get_ext_by_OBJ(const X509_REVOKED *x,
                                               const ASN1_OBJECT *obj,
                                               int lastpos);

OPENSSL_EXPORT int X509_REVOKED_get_ext_by_critical(const X509_REVOKED *x,
                                                    int crit, int lastpos);

OPENSSL_EXPORT X509_EXTENSION *X509_REVOKED_get_ext(const X509_REVOKED *x,
                                                    int loc);

OPENSSL_EXPORT X509_EXTENSION *X509_REVOKED_delete_ext(X509_REVOKED *x,
                                                       int loc);

OPENSSL_EXPORT int X509_REVOKED_add_ext(X509_REVOKED *x,
                                        const X509_EXTENSION *ex, int loc);

OPENSSL_EXPORT void *X509_REVOKED_get_ext_d2i(const X509_REVOKED *revoked,
                                              int nid, int *out_critical,
                                              int *out_idx);

OPENSSL_EXPORT int X509_REVOKED_add1_ext_i2d(X509_REVOKED *x, int nid,
                                             void *value, int crit,
                                             unsigned long flags);

OPENSSL_EXPORT X509_REQ *X509_REQ_dup(X509_REQ *req);

OPENSSL_EXPORT void X509_REQ_free(X509_REQ *req);

OPENSSL_EXPORT X509_REQ *d2i_X509_REQ(X509_REQ **out, const uint8_t **inp,
                                      long len);

OPENSSL_EXPORT int i2d_X509_REQ(X509_REQ *req, uint8_t **outp);

#define X509_REQ_VERSION_1 0

OPENSSL_EXPORT long X509_REQ_get_version(const X509_REQ *req);

OPENSSL_EXPORT X509_NAME *X509_REQ_get_subject_name(const X509_REQ *req);

OPENSSL_EXPORT EVP_PKEY *X509_REQ_get0_pubkey(const X509_REQ *req);

OPENSSL_EXPORT EVP_PKEY *X509_REQ_get_pubkey(const X509_REQ *req);

OPENSSL_EXPORT int X509_REQ_check_private_key(const X509_REQ *req,
                                              const EVP_PKEY *pkey);

OPENSSL_EXPORT int X509_REQ_get_attr_count(const X509_REQ *req);

OPENSSL_EXPORT X509_ATTRIBUTE *X509_REQ_get_attr(const X509_REQ *req, int loc);

OPENSSL_EXPORT int X509_REQ_get_attr_by_NID(const X509_REQ *req, int nid,
                                            int lastpos);

OPENSSL_EXPORT int X509_REQ_get_attr_by_OBJ(const X509_REQ *req,
                                            const ASN1_OBJECT *obj,
                                            int lastpos);

OPENSSL_EXPORT int X509_REQ_extension_nid(int nid);

OPENSSL_EXPORT STACK_OF(X509_EXTENSION) *X509_REQ_get_extensions(
    const X509_REQ *req);

OPENSSL_EXPORT void X509_REQ_get0_signature(const X509_REQ *req,
                                            const ASN1_BIT_STRING **out_sig,
                                            const X509_ALGOR **out_alg);

OPENSSL_EXPORT int X509_REQ_get_signature_nid(const X509_REQ *req);

OPENSSL_EXPORT int X509_REQ_verify(X509_REQ *req, EVP_PKEY *pkey);

OPENSSL_EXPORT STACK_OF(OPENSSL_STRING) *X509_REQ_get1_email(
    const X509_REQ *req);

OPENSSL_EXPORT X509_REQ *X509_REQ_new(void);

OPENSSL_EXPORT int X509_REQ_set_version(X509_REQ *req, long version);

OPENSSL_EXPORT int X509_REQ_set_subject_name(X509_REQ *req, X509_NAME *name);

OPENSSL_EXPORT int X509_REQ_set_pubkey(X509_REQ *req, EVP_PKEY *pkey);

OPENSSL_EXPORT X509_ATTRIBUTE *X509_REQ_delete_attr(X509_REQ *req, int loc);

OPENSSL_EXPORT int X509_REQ_add1_attr(X509_REQ *req,
                                      const X509_ATTRIBUTE *attr);

OPENSSL_EXPORT int X509_REQ_add1_attr_by_OBJ(X509_REQ *req,
                                             const ASN1_OBJECT *obj,
                                             int attrtype,
                                             const unsigned char *data,
                                             int len);

OPENSSL_EXPORT int X509_REQ_add1_attr_by_NID(X509_REQ *req, int nid,
                                             int attrtype,
                                             const unsigned char *data,
                                             int len);

OPENSSL_EXPORT int X509_REQ_add1_attr_by_txt(X509_REQ *req,
                                             const char *attrname, int attrtype,
                                             const unsigned char *data,
                                             int len);

OPENSSL_EXPORT int X509_REQ_add_extensions_nid(
    X509_REQ *req, const STACK_OF(X509_EXTENSION) *exts, int nid);

OPENSSL_EXPORT int X509_REQ_add_extensions(
    X509_REQ *req, const STACK_OF(X509_EXTENSION) *exts);

OPENSSL_EXPORT int X509_REQ_sign(X509_REQ *req, EVP_PKEY *pkey,
                                 const EVP_MD *md);

OPENSSL_EXPORT int X509_REQ_sign_ctx(X509_REQ *req, EVP_MD_CTX *ctx);

OPENSSL_EXPORT int i2d_re_X509_REQ_tbs(X509_REQ *req, uint8_t **outp);

OPENSSL_EXPORT int X509_REQ_set1_signature_algo(X509_REQ *req,
                                                const X509_ALGOR *algo);

OPENSSL_EXPORT int X509_REQ_set1_signature_value(X509_REQ *req,
                                                 const uint8_t *sig,
                                                 size_t sig_len);

DEFINE_STACK_OF(X509_NAME_ENTRY)
DEFINE_STACK_OF(X509_NAME)

DECLARE_ASN1_ITEM(X509_NAME)

OPENSSL_EXPORT X509_NAME *X509_NAME_new(void);

OPENSSL_EXPORT void X509_NAME_free(X509_NAME *name);

OPENSSL_EXPORT X509_NAME *d2i_X509_NAME(X509_NAME **out, const uint8_t **inp,
                                        long len);

OPENSSL_EXPORT int i2d_X509_NAME(X509_NAME *in, uint8_t **outp);

OPENSSL_EXPORT X509_NAME *X509_NAME_dup(X509_NAME *name);

OPENSSL_EXPORT int X509_NAME_cmp(const X509_NAME *a, const X509_NAME *b);

OPENSSL_EXPORT int X509_NAME_get0_der(X509_NAME *name, const uint8_t **out_der,
                                      size_t *out_der_len);

OPENSSL_EXPORT int X509_NAME_set(X509_NAME **xn, X509_NAME *name);

OPENSSL_EXPORT int X509_NAME_entry_count(const X509_NAME *name);

OPENSSL_EXPORT int X509_NAME_get_index_by_NID(const X509_NAME *name, int nid,
                                              int lastpos);

OPENSSL_EXPORT int X509_NAME_get_index_by_OBJ(const X509_NAME *name,
                                              const ASN1_OBJECT *obj,
                                              int lastpos);

OPENSSL_EXPORT X509_NAME_ENTRY *X509_NAME_get_entry(const X509_NAME *name,
                                                    int loc);

OPENSSL_EXPORT X509_NAME_ENTRY *X509_NAME_delete_entry(X509_NAME *name,
                                                       int loc);

OPENSSL_EXPORT int X509_NAME_add_entry(X509_NAME *name,
                                       const X509_NAME_ENTRY *entry, int loc,
                                       int set);

OPENSSL_EXPORT int X509_NAME_add_entry_by_OBJ(X509_NAME *name,
                                              const ASN1_OBJECT *obj, int type,
                                              const uint8_t *bytes,
                                              ossl_ssize_t len, int loc,
                                              int set);

OPENSSL_EXPORT int X509_NAME_add_entry_by_NID(X509_NAME *name, int nid,
                                              int type, const uint8_t *bytes,
                                              ossl_ssize_t len, int loc,
                                              int set);

OPENSSL_EXPORT int X509_NAME_add_entry_by_txt(X509_NAME *name,
                                              const char *field, int type,
                                              const uint8_t *bytes,
                                              ossl_ssize_t len, int loc,
                                              int set);

OPENSSL_EXPORT X509_NAME_ENTRY *X509_NAME_ENTRY_new(void);

OPENSSL_EXPORT void X509_NAME_ENTRY_free(X509_NAME_ENTRY *entry);

OPENSSL_EXPORT X509_NAME_ENTRY *X509_NAME_ENTRY_dup(
    const X509_NAME_ENTRY *entry);

OPENSSL_EXPORT ASN1_OBJECT *X509_NAME_ENTRY_get_object(
    const X509_NAME_ENTRY *entry);

OPENSSL_EXPORT int X509_NAME_ENTRY_set_object(X509_NAME_ENTRY *entry,
                                              const ASN1_OBJECT *obj);

OPENSSL_EXPORT ASN1_STRING *X509_NAME_ENTRY_get_data(
    const X509_NAME_ENTRY *entry);

OPENSSL_EXPORT int X509_NAME_ENTRY_set_data(X509_NAME_ENTRY *entry, int type,
                                            const uint8_t *bytes,
                                            ossl_ssize_t len);

OPENSSL_EXPORT int X509_NAME_ENTRY_set(const X509_NAME_ENTRY *entry);

OPENSSL_EXPORT X509_NAME_ENTRY *X509_NAME_ENTRY_create_by_OBJ(
    X509_NAME_ENTRY **out, const ASN1_OBJECT *obj, int type,
    const uint8_t *bytes, ossl_ssize_t len);

OPENSSL_EXPORT X509_NAME_ENTRY *X509_NAME_ENTRY_create_by_NID(
    X509_NAME_ENTRY **out, int nid, int type, const uint8_t *bytes,
    ossl_ssize_t len);

OPENSSL_EXPORT X509_NAME_ENTRY *X509_NAME_ENTRY_create_by_txt(
    X509_NAME_ENTRY **out, const char *field, int type, const uint8_t *bytes,
    ossl_ssize_t len);

OPENSSL_EXPORT X509_PUBKEY *X509_PUBKEY_new(void);

OPENSSL_EXPORT void X509_PUBKEY_free(X509_PUBKEY *key);

OPENSSL_EXPORT X509_PUBKEY *d2i_X509_PUBKEY(X509_PUBKEY **out,
                                            const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_X509_PUBKEY(const X509_PUBKEY *key, uint8_t **outp);

OPENSSL_EXPORT int X509_PUBKEY_set(X509_PUBKEY **x, EVP_PKEY *pkey);

OPENSSL_EXPORT EVP_PKEY *X509_PUBKEY_get0(const X509_PUBKEY *key);

OPENSSL_EXPORT EVP_PKEY *X509_PUBKEY_get(const X509_PUBKEY *key);

OPENSSL_EXPORT int X509_PUBKEY_set0_param(X509_PUBKEY *pub, ASN1_OBJECT *obj,
                                          int param_type, void *param_value,
                                          uint8_t *key, int key_len);

OPENSSL_EXPORT int X509_PUBKEY_get0_param(ASN1_OBJECT **out_obj,
                                          const uint8_t **out_key,
                                          int *out_key_len,
                                          X509_ALGOR **out_alg,
                                          X509_PUBKEY *pub);

OPENSSL_EXPORT const ASN1_BIT_STRING *X509_PUBKEY_get0_public_key(
    const X509_PUBKEY *pub);

DECLARE_ASN1_ITEM(X509_EXTENSION)

OPENSSL_EXPORT X509_EXTENSION *X509_EXTENSION_new(void);

OPENSSL_EXPORT void X509_EXTENSION_free(X509_EXTENSION *ex);

OPENSSL_EXPORT X509_EXTENSION *d2i_X509_EXTENSION(X509_EXTENSION **out,
                                                  const uint8_t **inp,
                                                  long len);

OPENSSL_EXPORT int i2d_X509_EXTENSION(const X509_EXTENSION *ex, uint8_t **outp);

OPENSSL_EXPORT X509_EXTENSION *X509_EXTENSION_dup(const X509_EXTENSION *ex);

OPENSSL_EXPORT X509_EXTENSION *X509_EXTENSION_create_by_NID(
    X509_EXTENSION **ex, int nid, int crit, const ASN1_OCTET_STRING *data);

OPENSSL_EXPORT X509_EXTENSION *X509_EXTENSION_create_by_OBJ(
    X509_EXTENSION **ex, const ASN1_OBJECT *obj, int crit,
    const ASN1_OCTET_STRING *data);

OPENSSL_EXPORT ASN1_OBJECT *X509_EXTENSION_get_object(const X509_EXTENSION *ex);

OPENSSL_EXPORT ASN1_OCTET_STRING *X509_EXTENSION_get_data(
    const X509_EXTENSION *ne);

OPENSSL_EXPORT int X509_EXTENSION_get_critical(const X509_EXTENSION *ex);

OPENSSL_EXPORT int X509_EXTENSION_set_object(X509_EXTENSION *ex,
                                             const ASN1_OBJECT *obj);

OPENSSL_EXPORT int X509_EXTENSION_set_critical(X509_EXTENSION *ex, int crit);

OPENSSL_EXPORT int X509_EXTENSION_set_data(X509_EXTENSION *ex,
                                           const ASN1_OCTET_STRING *data);

DEFINE_STACK_OF(X509_EXTENSION)
typedef STACK_OF(X509_EXTENSION) X509_EXTENSIONS;

OPENSSL_EXPORT X509_EXTENSIONS *d2i_X509_EXTENSIONS(X509_EXTENSIONS **out,
                                                    const uint8_t **inp,
                                                    long len);

OPENSSL_EXPORT int i2d_X509_EXTENSIONS(const X509_EXTENSIONS *alg,
                                       uint8_t **outp);

OPENSSL_EXPORT int X509v3_get_ext_count(const STACK_OF(X509_EXTENSION) *x);

OPENSSL_EXPORT int X509v3_get_ext_by_NID(const STACK_OF(X509_EXTENSION) *x,
                                         int nid, int lastpos);

OPENSSL_EXPORT int X509v3_get_ext_by_OBJ(const STACK_OF(X509_EXTENSION) *x,
                                         const ASN1_OBJECT *obj, int lastpos);

OPENSSL_EXPORT int X509v3_get_ext_by_critical(const STACK_OF(X509_EXTENSION) *x,
                                              int crit, int lastpos);

OPENSSL_EXPORT X509_EXTENSION *X509v3_get_ext(const STACK_OF(X509_EXTENSION) *x,
                                              int loc);

OPENSSL_EXPORT X509_EXTENSION *X509v3_delete_ext(STACK_OF(X509_EXTENSION) *x,
                                                 int loc);

OPENSSL_EXPORT STACK_OF(X509_EXTENSION) *X509v3_add_ext(
    STACK_OF(X509_EXTENSION) **x, const X509_EXTENSION *ex, int loc);

OPENSSL_EXPORT void *X509V3_EXT_d2i(const X509_EXTENSION *ext);

OPENSSL_EXPORT void *X509V3_get_d2i(const STACK_OF(X509_EXTENSION) *extensions,
                                    int nid, int *out_critical, int *out_idx);

OPENSSL_EXPORT int X509V3_EXT_free(int nid, void *ext_data);

OPENSSL_EXPORT X509_EXTENSION *X509V3_EXT_i2d(int ext_nid, int crit,
                                              void *ext_struc);

#define X509V3_ADD_OP_MASK 0xfL

#define X509V3_ADD_DEFAULT 0L

#define X509V3_ADD_APPEND 1L

#define X509V3_ADD_REPLACE 2L

#define X509V3_ADD_REPLACE_EXISTING 3L

#define X509V3_ADD_KEEP_EXISTING 4L

#define X509V3_ADD_DELETE 5L

#define X509V3_ADD_SILENT 0x10

OPENSSL_EXPORT int X509V3_add1_i2d(STACK_OF(X509_EXTENSION) **x, int nid,
                                   void *value, int crit, unsigned long flags);

struct BASIC_CONSTRAINTS_st {
  ASN1_BOOLEAN ca;
  ASN1_INTEGER *pathlen;
}                        ;

DECLARE_ASN1_ITEM(BASIC_CONSTRAINTS)

OPENSSL_EXPORT BASIC_CONSTRAINTS *BASIC_CONSTRAINTS_new(void);

OPENSSL_EXPORT void BASIC_CONSTRAINTS_free(BASIC_CONSTRAINTS *bcons);

OPENSSL_EXPORT BASIC_CONSTRAINTS *d2i_BASIC_CONSTRAINTS(BASIC_CONSTRAINTS **out,
                                                        const uint8_t **inp,
                                                        long len);

OPENSSL_EXPORT int i2d_BASIC_CONSTRAINTS(const BASIC_CONSTRAINTS *bcons,
                                         uint8_t **outp);

typedef STACK_OF(ASN1_OBJECT) EXTENDED_KEY_USAGE;

DECLARE_ASN1_ITEM(EXTENDED_KEY_USAGE)

OPENSSL_EXPORT EXTENDED_KEY_USAGE *EXTENDED_KEY_USAGE_new(void);

OPENSSL_EXPORT void EXTENDED_KEY_USAGE_free(EXTENDED_KEY_USAGE *eku);

OPENSSL_EXPORT EXTENDED_KEY_USAGE *d2i_EXTENDED_KEY_USAGE(
    EXTENDED_KEY_USAGE **out, const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_EXTENDED_KEY_USAGE(const EXTENDED_KEY_USAGE *eku,
                                          uint8_t **outp);

typedef struct otherName_st {
  ASN1_OBJECT *type_id;
  ASN1_TYPE *value;
} OTHERNAME;

typedef struct EDIPartyName_st {
  ASN1_STRING *nameAssigner;
  ASN1_STRING *partyName;
} EDIPARTYNAME;

#define GEN_OTHERNAME 0
#define GEN_EMAIL 1
#define GEN_DNS 2
#define GEN_X400 3
#define GEN_DIRNAME 4
#define GEN_EDIPARTY 5
#define GEN_URI 6
#define GEN_IPADD 7
#define GEN_RID 8

struct GENERAL_NAME_st {
  int type;
  union {
    char *ptr;
    OTHERNAME *otherName;
    ASN1_IA5STRING *rfc822Name;
    ASN1_IA5STRING *dNSName;
    ASN1_STRING *x400Address;
    X509_NAME *directoryName;
    EDIPARTYNAME *ediPartyName;
    ASN1_IA5STRING *uniformResourceIdentifier;
    ASN1_OCTET_STRING *iPAddress;
    ASN1_OBJECT *registeredID;

    ASN1_OCTET_STRING *ip;
    X509_NAME *dirn;
    ASN1_IA5STRING *ia5;
    ASN1_OBJECT *rid;
  } d;
}                   ;

OPENSSL_EXPORT GENERAL_NAME *GENERAL_NAME_new(void);

OPENSSL_EXPORT void GENERAL_NAME_free(GENERAL_NAME *gen);

OPENSSL_EXPORT GENERAL_NAME *d2i_GENERAL_NAME(GENERAL_NAME **out,
                                              const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_GENERAL_NAME(GENERAL_NAME *in, uint8_t **outp);

OPENSSL_EXPORT GENERAL_NAME *GENERAL_NAME_dup(GENERAL_NAME *gen);

OPENSSL_EXPORT GENERAL_NAMES *GENERAL_NAMES_new(void);

OPENSSL_EXPORT void GENERAL_NAMES_free(GENERAL_NAMES *gens);

OPENSSL_EXPORT GENERAL_NAMES *d2i_GENERAL_NAMES(GENERAL_NAMES **out,
                                                const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_GENERAL_NAMES(GENERAL_NAMES *in, uint8_t **outp);

OPENSSL_EXPORT OTHERNAME *OTHERNAME_new(void);

OPENSSL_EXPORT void OTHERNAME_free(OTHERNAME *name);

OPENSSL_EXPORT EDIPARTYNAME *EDIPARTYNAME_new(void);

OPENSSL_EXPORT void EDIPARTYNAME_free(EDIPARTYNAME *name);

OPENSSL_EXPORT void GENERAL_NAME_set0_value(GENERAL_NAME *gen, int type,
                                            void *value);

OPENSSL_EXPORT void *GENERAL_NAME_get0_value(const GENERAL_NAME *gen,
                                             int *out_type);

OPENSSL_EXPORT int GENERAL_NAME_set0_othername(GENERAL_NAME *gen,
                                               ASN1_OBJECT *oid,
                                               ASN1_TYPE *value);

OPENSSL_EXPORT int GENERAL_NAME_get0_otherName(const GENERAL_NAME *gen,
                                               ASN1_OBJECT **out_oid,
                                               ASN1_TYPE **out_value);

struct AUTHORITY_KEYID_st {
  ASN1_OCTET_STRING *keyid;
  GENERAL_NAMES *issuer;
  ASN1_INTEGER *serial;
}                      ;

DECLARE_ASN1_ITEM(AUTHORITY_KEYID)

OPENSSL_EXPORT AUTHORITY_KEYID *AUTHORITY_KEYID_new(void);

OPENSSL_EXPORT void AUTHORITY_KEYID_free(AUTHORITY_KEYID *akid);

OPENSSL_EXPORT AUTHORITY_KEYID *d2i_AUTHORITY_KEYID(AUTHORITY_KEYID **out,
                                                    const uint8_t **inp,
                                                    long len);

OPENSSL_EXPORT int i2d_AUTHORITY_KEYID(AUTHORITY_KEYID *akid, uint8_t **outp);

typedef struct GENERAL_SUBTREE_st {
  GENERAL_NAME *base;
  ASN1_INTEGER *minimum;
  ASN1_INTEGER *maximum;
} GENERAL_SUBTREE;

DEFINE_STACK_OF(GENERAL_SUBTREE)

OPENSSL_EXPORT GENERAL_SUBTREE *GENERAL_SUBTREE_new(void);

OPENSSL_EXPORT void GENERAL_SUBTREE_free(GENERAL_SUBTREE *subtree);

struct NAME_CONSTRAINTS_st {
  STACK_OF(GENERAL_SUBTREE) *permittedSubtrees;
  STACK_OF(GENERAL_SUBTREE) *excludedSubtrees;
}                       ;

DECLARE_ASN1_ITEM(NAME_CONSTRAINTS)

OPENSSL_EXPORT NAME_CONSTRAINTS *NAME_CONSTRAINTS_new(void);

OPENSSL_EXPORT void NAME_CONSTRAINTS_free(NAME_CONSTRAINTS *ncons);

typedef struct ACCESS_DESCRIPTION_st {
  ASN1_OBJECT *method;
  GENERAL_NAME *location;
} ACCESS_DESCRIPTION;

DEFINE_STACK_OF(ACCESS_DESCRIPTION)

OPENSSL_EXPORT ACCESS_DESCRIPTION *ACCESS_DESCRIPTION_new(void);

OPENSSL_EXPORT void ACCESS_DESCRIPTION_free(ACCESS_DESCRIPTION *desc);

typedef STACK_OF(ACCESS_DESCRIPTION) AUTHORITY_INFO_ACCESS;

DECLARE_ASN1_ITEM(AUTHORITY_INFO_ACCESS)

OPENSSL_EXPORT AUTHORITY_INFO_ACCESS *AUTHORITY_INFO_ACCESS_new(void);

OPENSSL_EXPORT void AUTHORITY_INFO_ACCESS_free(AUTHORITY_INFO_ACCESS *aia);

OPENSSL_EXPORT AUTHORITY_INFO_ACCESS *d2i_AUTHORITY_INFO_ACCESS(
    AUTHORITY_INFO_ACCESS **out, const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_AUTHORITY_INFO_ACCESS(AUTHORITY_INFO_ACCESS *aia,
                                             uint8_t **outp);

typedef struct DIST_POINT_NAME_st {
  int type;
  union {
    GENERAL_NAMES *fullname;
    STACK_OF(X509_NAME_ENTRY) *relativename;
  } name;

  X509_NAME *dpname;
} DIST_POINT_NAME;

OPENSSL_EXPORT DIST_POINT_NAME *DIST_POINT_NAME_new(void);

OPENSSL_EXPORT void DIST_POINT_NAME_free(DIST_POINT_NAME *name);

struct DIST_POINT_st {
  DIST_POINT_NAME *distpoint;
  ASN1_BIT_STRING *reasons;
  GENERAL_NAMES *CRLissuer;
}                 ;

DEFINE_STACK_OF(DIST_POINT)

OPENSSL_EXPORT DIST_POINT *DIST_POINT_new(void);

OPENSSL_EXPORT void DIST_POINT_free(DIST_POINT *dp);

typedef STACK_OF(DIST_POINT) CRL_DIST_POINTS;

DECLARE_ASN1_ITEM(CRL_DIST_POINTS)

OPENSSL_EXPORT CRL_DIST_POINTS *CRL_DIST_POINTS_new(void);

OPENSSL_EXPORT void CRL_DIST_POINTS_free(CRL_DIST_POINTS *crldp);

OPENSSL_EXPORT CRL_DIST_POINTS *d2i_CRL_DIST_POINTS(CRL_DIST_POINTS **out,
                                                    const uint8_t **inp,
                                                    long len);

OPENSSL_EXPORT int i2d_CRL_DIST_POINTS(CRL_DIST_POINTS *crldp, uint8_t **outp);

struct ISSUING_DIST_POINT_st {
  DIST_POINT_NAME *distpoint;
  ASN1_BOOLEAN onlyuser;
  ASN1_BOOLEAN onlyCA;
  ASN1_BIT_STRING *onlysomereasons;
  ASN1_BOOLEAN indirectCRL;
  ASN1_BOOLEAN onlyattr;
}                         ;

DECLARE_ASN1_ITEM(ISSUING_DIST_POINT)

OPENSSL_EXPORT ISSUING_DIST_POINT *ISSUING_DIST_POINT_new(void);

OPENSSL_EXPORT void ISSUING_DIST_POINT_free(ISSUING_DIST_POINT *idp);

OPENSSL_EXPORT ISSUING_DIST_POINT *d2i_ISSUING_DIST_POINT(
    ISSUING_DIST_POINT **out, const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_ISSUING_DIST_POINT(ISSUING_DIST_POINT *idp,
                                          uint8_t **outp);

typedef struct NOTICEREF_st {
  ASN1_STRING *organization;
  STACK_OF(ASN1_INTEGER) *noticenos;
} NOTICEREF;

OPENSSL_EXPORT NOTICEREF *NOTICEREF_new(void);

OPENSSL_EXPORT void NOTICEREF_free(NOTICEREF *ref);

typedef struct USERNOTICE_st {
  NOTICEREF *noticeref;
  ASN1_STRING *exptext;
} USERNOTICE;

OPENSSL_EXPORT USERNOTICE *USERNOTICE_new(void);

OPENSSL_EXPORT void USERNOTICE_free(USERNOTICE *notice);

typedef struct POLICYQUALINFO_st {
  ASN1_OBJECT *pqualid;
  union {
    ASN1_IA5STRING *cpsuri;
    USERNOTICE *usernotice;
    ASN1_TYPE *other;
  } d;
} POLICYQUALINFO;

DEFINE_STACK_OF(POLICYQUALINFO)

OPENSSL_EXPORT POLICYQUALINFO *POLICYQUALINFO_new(void);

OPENSSL_EXPORT void POLICYQUALINFO_free(POLICYQUALINFO *info);

typedef struct POLICYINFO_st {
  ASN1_OBJECT *policyid;
  STACK_OF(POLICYQUALINFO) *qualifiers;
} POLICYINFO;

DEFINE_STACK_OF(POLICYINFO)

OPENSSL_EXPORT POLICYINFO *POLICYINFO_new(void);

OPENSSL_EXPORT void POLICYINFO_free(POLICYINFO *info);

typedef STACK_OF(POLICYINFO) CERTIFICATEPOLICIES;

DECLARE_ASN1_ITEM(CERTIFICATEPOLICIES)

OPENSSL_EXPORT CERTIFICATEPOLICIES *CERTIFICATEPOLICIES_new(void);

OPENSSL_EXPORT void CERTIFICATEPOLICIES_free(CERTIFICATEPOLICIES *policies);

OPENSSL_EXPORT CERTIFICATEPOLICIES *d2i_CERTIFICATEPOLICIES(
    CERTIFICATEPOLICIES **out, const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_CERTIFICATEPOLICIES(const CERTIFICATEPOLICIES *policies,
                                           uint8_t **outp);

typedef struct POLICY_MAPPING_st {
  ASN1_OBJECT *issuerDomainPolicy;
  ASN1_OBJECT *subjectDomainPolicy;
} POLICY_MAPPING;

DEFINE_STACK_OF(POLICY_MAPPING)

OPENSSL_EXPORT POLICY_MAPPING *POLICY_MAPPING_new(void);

OPENSSL_EXPORT void POLICY_MAPPING_free(POLICY_MAPPING *mapping);

typedef STACK_OF(POLICY_MAPPING) POLICY_MAPPINGS;

DECLARE_ASN1_ITEM(POLICY_MAPPINGS)

typedef struct POLICY_CONSTRAINTS_st {
  ASN1_INTEGER *requireExplicitPolicy;
  ASN1_INTEGER *inhibitPolicyMapping;
} POLICY_CONSTRAINTS;

DECLARE_ASN1_ITEM(POLICY_CONSTRAINTS)

OPENSSL_EXPORT POLICY_CONSTRAINTS *POLICY_CONSTRAINTS_new(void);

OPENSSL_EXPORT void POLICY_CONSTRAINTS_free(POLICY_CONSTRAINTS *pcons);

DEFINE_STACK_OF(X509_ALGOR)

DECLARE_ASN1_ITEM(X509_ALGOR)

OPENSSL_EXPORT X509_ALGOR *X509_ALGOR_new(void);

OPENSSL_EXPORT X509_ALGOR *X509_ALGOR_dup(const X509_ALGOR *alg);

OPENSSL_EXPORT void X509_ALGOR_free(X509_ALGOR *alg);

OPENSSL_EXPORT X509_ALGOR *d2i_X509_ALGOR(X509_ALGOR **out, const uint8_t **inp,
                                          long len);

OPENSSL_EXPORT int i2d_X509_ALGOR(const X509_ALGOR *alg, uint8_t **outp);

OPENSSL_EXPORT int X509_ALGOR_set0(X509_ALGOR *alg, ASN1_OBJECT *obj,
                                   int param_type, void *param_value);

OPENSSL_EXPORT void X509_ALGOR_get0(const ASN1_OBJECT **out_obj,
                                    int *out_param_type,
                                    const void **out_param_value,
                                    const X509_ALGOR *alg);

OPENSSL_EXPORT int X509_ALGOR_set_md(X509_ALGOR *alg, const EVP_MD *md);

OPENSSL_EXPORT int X509_ALGOR_cmp(const X509_ALGOR *a, const X509_ALGOR *b);

DEFINE_STACK_OF(X509_ATTRIBUTE)

OPENSSL_EXPORT X509_ATTRIBUTE *X509_ATTRIBUTE_new(void);

OPENSSL_EXPORT X509_ATTRIBUTE *X509_ATTRIBUTE_dup(const X509_ATTRIBUTE *attr);

OPENSSL_EXPORT void X509_ATTRIBUTE_free(X509_ATTRIBUTE *attr);

OPENSSL_EXPORT X509_ATTRIBUTE *d2i_X509_ATTRIBUTE(X509_ATTRIBUTE **out,
                                                  const uint8_t **inp,
                                                  long len);

OPENSSL_EXPORT int i2d_X509_ATTRIBUTE(const X509_ATTRIBUTE *alg,
                                      uint8_t **outp);

OPENSSL_EXPORT X509_ATTRIBUTE *X509_ATTRIBUTE_create(int nid, int attrtype,
                                                     void *value);

OPENSSL_EXPORT X509_ATTRIBUTE *X509_ATTRIBUTE_create_by_NID(
    X509_ATTRIBUTE **attr, int nid, int attrtype, const void *data, int len);

OPENSSL_EXPORT X509_ATTRIBUTE *X509_ATTRIBUTE_create_by_OBJ(
    X509_ATTRIBUTE **attr, const ASN1_OBJECT *obj, int attrtype,
    const void *data, int len);

OPENSSL_EXPORT X509_ATTRIBUTE *X509_ATTRIBUTE_create_by_txt(
    X509_ATTRIBUTE **attr, const char *attrname, int type,
    const unsigned char *bytes, int len);

OPENSSL_EXPORT int X509_ATTRIBUTE_set1_object(X509_ATTRIBUTE *attr,
                                              const ASN1_OBJECT *obj);

OPENSSL_EXPORT int X509_ATTRIBUTE_set1_data(X509_ATTRIBUTE *attr, int attrtype,
                                            const void *data, int len);

OPENSSL_EXPORT void *X509_ATTRIBUTE_get0_data(X509_ATTRIBUTE *attr, int idx,
                                              int attrtype, void *unused);

OPENSSL_EXPORT int X509_ATTRIBUTE_count(const X509_ATTRIBUTE *attr);

OPENSSL_EXPORT ASN1_OBJECT *X509_ATTRIBUTE_get0_object(X509_ATTRIBUTE *attr);

OPENSSL_EXPORT ASN1_TYPE *X509_ATTRIBUTE_get0_type(X509_ATTRIBUTE *attr,
                                                   int idx);

OPENSSL_EXPORT X509_STORE *X509_STORE_new(void);

OPENSSL_EXPORT int X509_STORE_up_ref(X509_STORE *store);

OPENSSL_EXPORT void X509_STORE_free(X509_STORE *store);

OPENSSL_EXPORT int X509_STORE_add_cert(X509_STORE *store, X509 *x509);

OPENSSL_EXPORT int X509_STORE_add_crl(X509_STORE *store, X509_CRL *crl);

OPENSSL_EXPORT X509_VERIFY_PARAM *X509_STORE_get0_param(X509_STORE *store);

OPENSSL_EXPORT int X509_STORE_set1_param(X509_STORE *store,
                                         const X509_VERIFY_PARAM *param);

OPENSSL_EXPORT int X509_STORE_set_flags(X509_STORE *store, unsigned long flags);

OPENSSL_EXPORT int X509_STORE_set_depth(X509_STORE *store, int depth);

OPENSSL_EXPORT int X509_STORE_set_purpose(X509_STORE *store, int purpose);

OPENSSL_EXPORT int X509_STORE_set_trust(X509_STORE *store, int trust);

#define X509_LU_NONE 0
#define X509_LU_X509 1
#define X509_LU_CRL 2
#define X509_LU_PKEY 3

DEFINE_STACK_OF(X509_OBJECT)

OPENSSL_EXPORT X509_OBJECT *X509_OBJECT_new(void);

OPENSSL_EXPORT void X509_OBJECT_free(X509_OBJECT *obj);

OPENSSL_EXPORT int X509_OBJECT_get_type(const X509_OBJECT *obj);

OPENSSL_EXPORT X509 *X509_OBJECT_get0_X509(const X509_OBJECT *obj);

OPENSSL_EXPORT STACK_OF(X509_OBJECT) *X509_STORE_get1_objects(
    X509_STORE *store);

OPENSSL_EXPORT X509_STORE_CTX *X509_STORE_CTX_new(void);

OPENSSL_EXPORT void X509_STORE_CTX_free(X509_STORE_CTX *ctx);

OPENSSL_EXPORT int X509_STORE_CTX_init(X509_STORE_CTX *ctx, X509_STORE *store,
                                       X509 *x509, STACK_OF(X509) *chain);

OPENSSL_EXPORT int X509_verify_cert(X509_STORE_CTX *ctx);

OPENSSL_EXPORT STACK_OF(X509) *X509_STORE_CTX_get0_chain(
    const X509_STORE_CTX *ctx);

OPENSSL_EXPORT STACK_OF(X509) *X509_STORE_CTX_get1_chain(
    const X509_STORE_CTX *ctx);

#define X509_V_OK 0
#define X509_V_ERR_UNSPECIFIED 1
#define X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT 2
#define X509_V_ERR_UNABLE_TO_GET_CRL 3
#define X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE 4
#define X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE 5
#define X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY 6
#define X509_V_ERR_CERT_SIGNATURE_FAILURE 7
#define X509_V_ERR_CRL_SIGNATURE_FAILURE 8
#define X509_V_ERR_CERT_NOT_YET_VALID 9
#define X509_V_ERR_CERT_HAS_EXPIRED 10
#define X509_V_ERR_CRL_NOT_YET_VALID 11
#define X509_V_ERR_CRL_HAS_EXPIRED 12
#define X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD 13
#define X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD 14
#define X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD 15
#define X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD 16
#define X509_V_ERR_OUT_OF_MEM 17
#define X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT 18
#define X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN 19
#define X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY 20
#define X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE 21
#define X509_V_ERR_CERT_CHAIN_TOO_LONG 22
#define X509_V_ERR_CERT_REVOKED 23
#define X509_V_ERR_INVALID_CA 24
#define X509_V_ERR_PATH_LENGTH_EXCEEDED 25
#define X509_V_ERR_INVALID_PURPOSE 26
#define X509_V_ERR_CERT_UNTRUSTED 27
#define X509_V_ERR_CERT_REJECTED 28
#define X509_V_ERR_SUBJECT_ISSUER_MISMATCH 29
#define X509_V_ERR_AKID_SKID_MISMATCH 30
#define X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH 31
#define X509_V_ERR_KEYUSAGE_NO_CERTSIGN 32
#define X509_V_ERR_UNABLE_TO_GET_CRL_ISSUER 33
#define X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION 34
#define X509_V_ERR_KEYUSAGE_NO_CRL_SIGN 35
#define X509_V_ERR_UNHANDLED_CRITICAL_CRL_EXTENSION 36
#define X509_V_ERR_INVALID_NON_CA 37
#define X509_V_ERR_PROXY_PATH_LENGTH_EXCEEDED 38
#define X509_V_ERR_KEYUSAGE_NO_DIGITAL_SIGNATURE 39
#define X509_V_ERR_PROXY_CERTIFICATES_NOT_ALLOWED 40
#define X509_V_ERR_INVALID_EXTENSION 41
#define X509_V_ERR_INVALID_POLICY_EXTENSION 42
#define X509_V_ERR_NO_EXPLICIT_POLICY 43
#define X509_V_ERR_DIFFERENT_CRL_SCOPE 44
#define X509_V_ERR_UNSUPPORTED_EXTENSION_FEATURE 45
#define X509_V_ERR_UNNESTED_RESOURCE 46
#define X509_V_ERR_PERMITTED_VIOLATION 47
#define X509_V_ERR_EXCLUDED_VIOLATION 48
#define X509_V_ERR_SUBTREE_MINMAX 49
#define X509_V_ERR_APPLICATION_VERIFICATION 50
#define X509_V_ERR_UNSUPPORTED_CONSTRAINT_TYPE 51
#define X509_V_ERR_UNSUPPORTED_CONSTRAINT_SYNTAX 52
#define X509_V_ERR_UNSUPPORTED_NAME_SYNTAX 53
#define X509_V_ERR_CRL_PATH_VALIDATION_ERROR 54
#define X509_V_ERR_HOSTNAME_MISMATCH 62
#define X509_V_ERR_EMAIL_MISMATCH 63
#define X509_V_ERR_IP_ADDRESS_MISMATCH 64
#define X509_V_ERR_INVALID_CALL 65
#define X509_V_ERR_STORE_LOOKUP 66
#define X509_V_ERR_NAME_CONSTRAINTS_WITHOUT_SANS 67

OPENSSL_EXPORT int X509_STORE_CTX_get_error(const X509_STORE_CTX *ctx);

OPENSSL_EXPORT void X509_STORE_CTX_set_error(X509_STORE_CTX *ctx, int err);

OPENSSL_EXPORT const char *X509_verify_cert_error_string(long err);

OPENSSL_EXPORT int X509_STORE_CTX_get_error_depth(const X509_STORE_CTX *ctx);

OPENSSL_EXPORT X509 *X509_STORE_CTX_get_current_cert(const X509_STORE_CTX *ctx);

OPENSSL_EXPORT X509_CRL *X509_STORE_CTX_get0_current_crl(
    const X509_STORE_CTX *ctx);

OPENSSL_EXPORT X509_STORE *X509_STORE_CTX_get0_store(const X509_STORE_CTX *ctx);

OPENSSL_EXPORT X509 *X509_STORE_CTX_get0_cert(const X509_STORE_CTX *ctx);

OPENSSL_EXPORT STACK_OF(X509) *X509_STORE_CTX_get0_untrusted(
    const X509_STORE_CTX *ctx);

OPENSSL_EXPORT void X509_STORE_CTX_set0_trusted_stack(X509_STORE_CTX *ctx,
                                                      STACK_OF(X509) *sk);

OPENSSL_EXPORT void X509_STORE_CTX_set0_crls(X509_STORE_CTX *ctx,
                                             STACK_OF(X509_CRL) *sk);

OPENSSL_EXPORT int X509_STORE_CTX_set_default(X509_STORE_CTX *ctx,
                                              const char *name);

OPENSSL_EXPORT X509_VERIFY_PARAM *X509_STORE_CTX_get0_param(
    X509_STORE_CTX *ctx);

OPENSSL_EXPORT void X509_STORE_CTX_set0_param(X509_STORE_CTX *ctx,
                                              X509_VERIFY_PARAM *param);

OPENSSL_EXPORT void X509_STORE_CTX_set_flags(X509_STORE_CTX *ctx,
                                             unsigned long flags);

OPENSSL_EXPORT void X509_STORE_CTX_set_time(X509_STORE_CTX *ctx,
                                            unsigned long flags, time_t t);

OPENSSL_EXPORT void X509_STORE_CTX_set_time_posix(X509_STORE_CTX *ctx,
                                                  unsigned long flags,
                                                  int64_t t);

OPENSSL_EXPORT void X509_STORE_CTX_set_depth(X509_STORE_CTX *ctx, int depth);

OPENSSL_EXPORT int X509_STORE_CTX_set_purpose(X509_STORE_CTX *ctx, int purpose);

OPENSSL_EXPORT int X509_STORE_CTX_set_trust(X509_STORE_CTX *ctx, int trust);

OPENSSL_EXPORT X509_VERIFY_PARAM *X509_VERIFY_PARAM_new(void);

OPENSSL_EXPORT void X509_VERIFY_PARAM_free(X509_VERIFY_PARAM *param);

OPENSSL_EXPORT int X509_VERIFY_PARAM_inherit(X509_VERIFY_PARAM *to,
                                             const X509_VERIFY_PARAM *from);

OPENSSL_EXPORT int X509_VERIFY_PARAM_set1(X509_VERIFY_PARAM *to,
                                          const X509_VERIFY_PARAM *from);

#define X509_V_FLAG_CB_ISSUER_CHECK 0x1

#define X509_V_FLAG_USE_CHECK_TIME 0x2

#define X509_V_FLAG_CRL_CHECK 0x4

#define X509_V_FLAG_CRL_CHECK_ALL 0x8

#define X509_V_FLAG_IGNORE_CRITICAL 0x10

#define X509_V_FLAG_X509_STRICT 0x00

#define X509_V_FLAG_ALLOW_PROXY_CERTS 0x40

#define X509_V_FLAG_POLICY_CHECK 0x80

#define X509_V_FLAG_EXPLICIT_POLICY 0x100

#define X509_V_FLAG_INHIBIT_ANY 0x200

#define X509_V_FLAG_INHIBIT_MAP 0x400

#define X509_V_FLAG_NOTIFY_POLICY 0x800

#define X509_V_FLAG_EXTENDED_CRL_SUPPORT 0x1000

#define X509_V_FLAG_USE_DELTAS 0x2000

#define X509_V_FLAG_CHECK_SS_SIGNATURE 0x4000

#define X509_V_FLAG_TRUSTED_FIRST 0x8000

#define X509_V_FLAG_PARTIAL_CHAIN 0x80000

#define X509_V_FLAG_NO_ALT_CHAINS 0x100000

#define X509_V_FLAG_NO_CHECK_TIME 0x200000

OPENSSL_EXPORT int X509_VERIFY_PARAM_set_flags(X509_VERIFY_PARAM *param,
                                               unsigned long flags);

OPENSSL_EXPORT int X509_VERIFY_PARAM_clear_flags(X509_VERIFY_PARAM *param,
                                                 unsigned long flags);

OPENSSL_EXPORT unsigned long X509_VERIFY_PARAM_get_flags(
    const X509_VERIFY_PARAM *param);

OPENSSL_EXPORT void X509_VERIFY_PARAM_set_depth(X509_VERIFY_PARAM *param,
                                                int depth);

OPENSSL_EXPORT int X509_VERIFY_PARAM_get_depth(const X509_VERIFY_PARAM *param);

OPENSSL_EXPORT void X509_VERIFY_PARAM_set_time(X509_VERIFY_PARAM *param,
                                               time_t t);

OPENSSL_EXPORT void X509_VERIFY_PARAM_set_time_posix(X509_VERIFY_PARAM *param,
                                                     int64_t t);

OPENSSL_EXPORT int X509_VERIFY_PARAM_add0_policy(X509_VERIFY_PARAM *param,
                                                 ASN1_OBJECT *policy);

OPENSSL_EXPORT int X509_VERIFY_PARAM_set1_policies(
    X509_VERIFY_PARAM *param, const STACK_OF(ASN1_OBJECT) *policies);

OPENSSL_EXPORT int X509_VERIFY_PARAM_set1_host(X509_VERIFY_PARAM *param,
                                               const char *name,
                                               size_t name_len);

OPENSSL_EXPORT int X509_VERIFY_PARAM_add1_host(X509_VERIFY_PARAM *param,
                                               const char *name,
                                               size_t name_len);

#define X509_CHECK_FLAG_NO_WILDCARDS 0x2

#define X509_CHECK_FLAG_NEVER_CHECK_SUBJECT 0x20

OPENSSL_EXPORT void X509_VERIFY_PARAM_set_hostflags(X509_VERIFY_PARAM *param,
                                                    unsigned int flags);

OPENSSL_EXPORT int X509_VERIFY_PARAM_set1_email(X509_VERIFY_PARAM *param,
                                                const char *email,
                                                size_t email_len);

OPENSSL_EXPORT int X509_VERIFY_PARAM_set1_ip(X509_VERIFY_PARAM *param,
                                             const uint8_t *ip, size_t ip_len);

OPENSSL_EXPORT int X509_VERIFY_PARAM_set1_ip_asc(X509_VERIFY_PARAM *param,
                                                 const char *ipasc);

#define X509_PURPOSE_SSL_CLIENT 1

#define X509_PURPOSE_SSL_SERVER 2

#define X509_PURPOSE_NS_SSL_SERVER 3

#define X509_PURPOSE_SMIME_SIGN 4

#define X509_PURPOSE_SMIME_ENCRYPT 5

#define X509_PURPOSE_CRL_SIGN 6

#define X509_PURPOSE_ANY 7

#define X509_PURPOSE_OCSP_HELPER 8

#define X509_PURPOSE_TIMESTAMP_SIGN 9

OPENSSL_EXPORT int X509_VERIFY_PARAM_set_purpose(X509_VERIFY_PARAM *param,
                                                 int purpose);

#define X509_TRUST_COMPAT 1

#define X509_TRUST_SSL_CLIENT 2

#define X509_TRUST_SSL_SERVER 3

#define X509_TRUST_EMAIL 4

#define X509_TRUST_OBJECT_SIGN 5

#define X509_TRUST_TSA 8

OPENSSL_EXPORT int X509_VERIFY_PARAM_set_trust(X509_VERIFY_PARAM *param,
                                               int trust);

OPENSSL_EXPORT int X509_STORE_load_locations(X509_STORE *store,
                                             const char *file, const char *dir);

OPENSSL_EXPORT X509_LOOKUP *X509_STORE_add_lookup(
    X509_STORE *store, const X509_LOOKUP_METHOD *method);

OPENSSL_EXPORT const X509_LOOKUP_METHOD *X509_LOOKUP_hash_dir(void);

OPENSSL_EXPORT const X509_LOOKUP_METHOD *X509_LOOKUP_file(void);

#define X509_FILETYPE_PEM 1
#define X509_FILETYPE_ASN1 2
#define X509_FILETYPE_DEFAULT 3

OPENSSL_EXPORT int X509_LOOKUP_load_file(X509_LOOKUP *lookup, const char *file,
                                         int type);

OPENSSL_EXPORT int X509_LOOKUP_add_dir(X509_LOOKUP *lookup, const char *path,
                                       int type);

#define X509_L_FILE_LOAD 1
#define X509_L_ADD_DIR 2

OPENSSL_EXPORT int X509_LOOKUP_ctrl(X509_LOOKUP *lookup, int cmd,
                                    const char *argc, long argl, char **ret);

OPENSSL_EXPORT int X509_load_cert_file(X509_LOOKUP *lookup, const char *file,
                                       int type);

OPENSSL_EXPORT int X509_load_crl_file(X509_LOOKUP *lookup, const char *file,
                                      int type);

OPENSSL_EXPORT int X509_load_cert_crl_file(X509_LOOKUP *lookup,
                                           const char *file, int type);

OPENSSL_EXPORT uint32_t X509_NAME_hash(X509_NAME *name);

OPENSSL_EXPORT uint32_t X509_NAME_hash_old(X509_NAME *name);

OPENSSL_EXPORT int X509_STORE_set_default_paths(X509_STORE *store);

OPENSSL_EXPORT const char *X509_get_default_cert_area(void);
OPENSSL_EXPORT const char *X509_get_default_cert_dir(void);
OPENSSL_EXPORT const char *X509_get_default_cert_file(void);
OPENSSL_EXPORT const char *X509_get_default_private_dir(void);

OPENSSL_EXPORT const char *X509_get_default_cert_dir_env(void);

OPENSSL_EXPORT const char *X509_get_default_cert_file_env(void);

struct Netscape_spki_st {
  NETSCAPE_SPKAC *spkac;
  X509_ALGOR *sig_algor;
  ASN1_BIT_STRING *signature;
}                    ;

OPENSSL_EXPORT NETSCAPE_SPKI *NETSCAPE_SPKI_new(void);

OPENSSL_EXPORT void NETSCAPE_SPKI_free(NETSCAPE_SPKI *spki);

OPENSSL_EXPORT NETSCAPE_SPKI *d2i_NETSCAPE_SPKI(NETSCAPE_SPKI **out,
                                                const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_NETSCAPE_SPKI(const NETSCAPE_SPKI *spki, uint8_t **outp);

OPENSSL_EXPORT int NETSCAPE_SPKI_verify(NETSCAPE_SPKI *spki, EVP_PKEY *pkey);

OPENSSL_EXPORT NETSCAPE_SPKI *NETSCAPE_SPKI_b64_decode(const char *str,
                                                       ossl_ssize_t len);

OPENSSL_EXPORT char *NETSCAPE_SPKI_b64_encode(NETSCAPE_SPKI *spki);

OPENSSL_EXPORT EVP_PKEY *NETSCAPE_SPKI_get_pubkey(const NETSCAPE_SPKI *spki);

OPENSSL_EXPORT int NETSCAPE_SPKI_set_pubkey(NETSCAPE_SPKI *spki,
                                            EVP_PKEY *pkey);

OPENSSL_EXPORT int NETSCAPE_SPKI_sign(NETSCAPE_SPKI *spki, EVP_PKEY *pkey,
                                      const EVP_MD *md);

struct Netscape_spkac_st {
  X509_PUBKEY *pubkey;
  ASN1_IA5STRING *challenge;
}                     ;

OPENSSL_EXPORT NETSCAPE_SPKAC *NETSCAPE_SPKAC_new(void);

OPENSSL_EXPORT void NETSCAPE_SPKAC_free(NETSCAPE_SPKAC *spkac);

OPENSSL_EXPORT NETSCAPE_SPKAC *d2i_NETSCAPE_SPKAC(NETSCAPE_SPKAC **out,
                                                  const uint8_t **inp,
                                                  long len);

OPENSSL_EXPORT int i2d_NETSCAPE_SPKAC(const NETSCAPE_SPKAC *spkac,
                                      uint8_t **outp);

struct rsa_pss_params_st {
  X509_ALGOR *hashAlgorithm;
  X509_ALGOR *maskGenAlgorithm;
  ASN1_INTEGER *saltLength;
  ASN1_INTEGER *trailerField;

  X509_ALGOR *maskHash;
}                     ;

DECLARE_ASN1_ITEM(RSA_PSS_PARAMS)

OPENSSL_EXPORT RSA_PSS_PARAMS *RSA_PSS_PARAMS_new(void);

OPENSSL_EXPORT void RSA_PSS_PARAMS_free(RSA_PSS_PARAMS *params);

OPENSSL_EXPORT RSA_PSS_PARAMS *d2i_RSA_PSS_PARAMS(RSA_PSS_PARAMS **out,
                                                  const uint8_t **inp,
                                                  long len);

OPENSSL_EXPORT int i2d_RSA_PSS_PARAMS(const RSA_PSS_PARAMS *in, uint8_t **outp);

OPENSSL_EXPORT PKCS8_PRIV_KEY_INFO *PKCS8_PRIV_KEY_INFO_new(void);

OPENSSL_EXPORT void PKCS8_PRIV_KEY_INFO_free(PKCS8_PRIV_KEY_INFO *key);

OPENSSL_EXPORT PKCS8_PRIV_KEY_INFO *d2i_PKCS8_PRIV_KEY_INFO(
    PKCS8_PRIV_KEY_INFO **out, const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_PKCS8_PRIV_KEY_INFO(const PKCS8_PRIV_KEY_INFO *key,
                                           uint8_t **outp);

OPENSSL_EXPORT EVP_PKEY *EVP_PKCS82PKEY(const PKCS8_PRIV_KEY_INFO *p8);

OPENSSL_EXPORT PKCS8_PRIV_KEY_INFO *EVP_PKEY2PKCS8(const EVP_PKEY *pkey);

OPENSSL_EXPORT X509_SIG *X509_SIG_new(void);

OPENSSL_EXPORT void X509_SIG_free(X509_SIG *key);

OPENSSL_EXPORT X509_SIG *d2i_X509_SIG(X509_SIG **out, const uint8_t **inp,
                                      long len);

OPENSSL_EXPORT int i2d_X509_SIG(const X509_SIG *sig, uint8_t **outp);

OPENSSL_EXPORT void X509_SIG_get0(const X509_SIG *sig,
                                  const X509_ALGOR **out_alg,
                                  const ASN1_OCTET_STRING **out_digest);

OPENSSL_EXPORT void X509_SIG_getm(X509_SIG *sig, X509_ALGOR **out_alg,
                                  ASN1_OCTET_STRING **out_digest);

#define X509_FLAG_COMPAT 0

#define X509_FLAG_NO_HEADER 1L

#define X509_FLAG_NO_VERSION (1L << 1)

#define X509_FLAG_NO_SERIAL (1L << 2)

#define X509_FLAG_NO_SIGNAME (1L << 3)

#define X509_FLAG_NO_ISSUER (1L << 4)

#define X509_FLAG_NO_VALIDITY (1L << 5)

#define X509_FLAG_NO_SUBJECT (1L << 6)

#define X509_FLAG_NO_PUBKEY (1L << 7)

#define X509_FLAG_NO_EXTENSIONS (1L << 8)

#define X509_FLAG_NO_SIGDUMP (1L << 9)

#define X509_FLAG_NO_AUX (1L << 10)

#define X509_FLAG_NO_ATTRIBUTES (1L << 11)

#define X509_FLAG_NO_IDS (1L << 12)

#define X509V3_EXT_UNKNOWN_MASK (0xfL << 16)

#define X509V3_EXT_DEFAULT 0

#define X509V3_EXT_ERROR_UNKNOWN (1L << 16)

#define X509V3_EXT_PARSE_UNKNOWN (2L << 16)

#define X509V3_EXT_DUMP_UNKNOWN (3L << 16)

OPENSSL_EXPORT int X509_print_ex(BIO *bp, X509 *x, unsigned long nmflag,
                                 unsigned long cflag);

OPENSSL_EXPORT int X509_print_ex_fp(FILE *fp, X509 *x, unsigned long nmflag,
                                    unsigned long cflag);

OPENSSL_EXPORT int X509_print(BIO *bp, X509 *x);

OPENSSL_EXPORT int X509_print_fp(FILE *fp, X509 *x);

OPENSSL_EXPORT int X509_CRL_print(BIO *bp, X509_CRL *x);

OPENSSL_EXPORT int X509_CRL_print_fp(FILE *fp, X509_CRL *x);

OPENSSL_EXPORT int X509_REQ_print_ex(BIO *bp, X509_REQ *x, unsigned long nmflag,
                                     unsigned long cflag);

OPENSSL_EXPORT int X509_REQ_print(BIO *bp, X509_REQ *req);

OPENSSL_EXPORT int X509_REQ_print_fp(FILE *fp, X509_REQ *req);

#define XN_FLAG_COMPAT 0ul

#define XN_FLAG_SEP_MASK (0xful << 16)

#define XN_FLAG_SEP_COMMA_PLUS (1ul << 16)

#define XN_FLAG_SEP_CPLUS_SPC (2ul << 16)

#define XN_FLAG_SEP_SPLUS_SPC (3ul << 16)

#define XN_FLAG_SEP_MULTILINE (4ul << 16)

#define XN_FLAG_DN_REV (1ul << 20)

#define XN_FLAG_FN_MASK (0x3ul << 21)

#define XN_FLAG_FN_SN 0ul

#define XN_FLAG_SPC_EQ (1ul << 23)

#define XN_FLAG_DUMP_UNKNOWN_FIELDS (1ul << 24)

#define XN_FLAG_RFC2253                                             \
  (ASN1_STRFLGS_RFC2253 | XN_FLAG_SEP_COMMA_PLUS | XN_FLAG_DN_REV | \
   XN_FLAG_FN_SN | XN_FLAG_DUMP_UNKNOWN_FIELDS)

#define XN_FLAG_ONELINE                                                    \
  (ASN1_STRFLGS_RFC2253 | ASN1_STRFLGS_ESC_QUOTE | XN_FLAG_SEP_CPLUS_SPC | \
   XN_FLAG_SPC_EQ | XN_FLAG_FN_SN)

OPENSSL_EXPORT int X509_NAME_print_ex(BIO *out, const X509_NAME *nm, int indent,
                                      unsigned long flags);

OPENSSL_EXPORT int X509_NAME_print(BIO *bp, const X509_NAME *name, int obase);

OPENSSL_EXPORT char *X509_NAME_oneline(const X509_NAME *name, char *buf, int size);

OPENSSL_EXPORT int X509_NAME_print_ex_fp(FILE *fp, const X509_NAME *nm,
                                         int indent, unsigned long flags);

OPENSSL_EXPORT int X509_signature_dump(BIO *bio, const ASN1_STRING *sig,
                                       int indent);

OPENSSL_EXPORT int X509_signature_print(BIO *bio, const X509_ALGOR *alg,
                                        const ASN1_STRING *sig);

OPENSSL_EXPORT int X509V3_EXT_print(BIO *out, const X509_EXTENSION *ext,
                                    unsigned long flag, int indent);

OPENSSL_EXPORT int X509V3_EXT_print_fp(FILE *out, const X509_EXTENSION *ext,
                                       int flag, int indent);

OPENSSL_EXPORT int X509V3_extensions_print(BIO *out, const char *title,
                                           const STACK_OF(X509_EXTENSION) *exts,
                                           unsigned long flag, int indent);

OPENSSL_EXPORT int GENERAL_NAME_print(BIO *out, const GENERAL_NAME *gen);

OPENSSL_EXPORT int X509_pubkey_digest(const X509 *x509, const EVP_MD *md,
                                      uint8_t *out, unsigned *out_len);

OPENSSL_EXPORT int X509_digest(const X509 *x509, const EVP_MD *md, uint8_t *out,
                               unsigned *out_len);

OPENSSL_EXPORT int X509_CRL_digest(const X509_CRL *crl, const EVP_MD *md,
                                   uint8_t *out, unsigned *out_len);

OPENSSL_EXPORT int X509_REQ_digest(const X509_REQ *req, const EVP_MD *md,
                                   uint8_t *out, unsigned *out_len);

OPENSSL_EXPORT int X509_NAME_digest(const X509_NAME *name, const EVP_MD *md,
                                    uint8_t *out, unsigned *out_len);

OPENSSL_EXPORT X509 *d2i_X509_bio(BIO *bp, X509 **x509);
OPENSSL_EXPORT X509_CRL *d2i_X509_CRL_bio(BIO *bp, X509_CRL **crl);
OPENSSL_EXPORT X509_REQ *d2i_X509_REQ_bio(BIO *bp, X509_REQ **req);
OPENSSL_EXPORT RSA *d2i_RSAPrivateKey_bio(BIO *bp, RSA **rsa);
OPENSSL_EXPORT RSA *d2i_RSAPublicKey_bio(BIO *bp, RSA **rsa);
OPENSSL_EXPORT RSA *d2i_RSA_PUBKEY_bio(BIO *bp, RSA **rsa);
OPENSSL_EXPORT DSA *d2i_DSA_PUBKEY_bio(BIO *bp, DSA **dsa);
OPENSSL_EXPORT DSA *d2i_DSAPrivateKey_bio(BIO *bp, DSA **dsa);
OPENSSL_EXPORT EC_KEY *d2i_EC_PUBKEY_bio(BIO *bp, EC_KEY **eckey);
OPENSSL_EXPORT EC_KEY *d2i_ECPrivateKey_bio(BIO *bp, EC_KEY **eckey);
OPENSSL_EXPORT X509_SIG *d2i_PKCS8_bio(BIO *bp, X509_SIG **p8);
OPENSSL_EXPORT PKCS8_PRIV_KEY_INFO *d2i_PKCS8_PRIV_KEY_INFO_bio(
    BIO *bp, PKCS8_PRIV_KEY_INFO **p8inf);
OPENSSL_EXPORT EVP_PKEY *d2i_PUBKEY_bio(BIO *bp, EVP_PKEY **a);
OPENSSL_EXPORT DH *d2i_DHparams_bio(BIO *bp, DH **dh);

OPENSSL_EXPORT EVP_PKEY *d2i_PrivateKey_bio(BIO *bp, EVP_PKEY **a);

OPENSSL_EXPORT int i2d_X509_bio(BIO *bp, X509 *x509);
OPENSSL_EXPORT int i2d_X509_CRL_bio(BIO *bp, X509_CRL *crl);
OPENSSL_EXPORT int i2d_X509_REQ_bio(BIO *bp, X509_REQ *req);
OPENSSL_EXPORT int i2d_RSAPrivateKey_bio(BIO *bp, RSA *rsa);
OPENSSL_EXPORT int i2d_RSAPublicKey_bio(BIO *bp, RSA *rsa);
OPENSSL_EXPORT int i2d_RSA_PUBKEY_bio(BIO *bp, RSA *rsa);
OPENSSL_EXPORT int i2d_DSA_PUBKEY_bio(BIO *bp, DSA *dsa);
OPENSSL_EXPORT int i2d_DSAPrivateKey_bio(BIO *bp, DSA *dsa);
OPENSSL_EXPORT int i2d_EC_PUBKEY_bio(BIO *bp, EC_KEY *eckey);
OPENSSL_EXPORT int i2d_ECPrivateKey_bio(BIO *bp, EC_KEY *eckey);
OPENSSL_EXPORT int i2d_PKCS8_bio(BIO *bp, X509_SIG *p8);
OPENSSL_EXPORT int i2d_PKCS8_PRIV_KEY_INFO_bio(BIO *bp,
                                               PKCS8_PRIV_KEY_INFO *p8inf);
OPENSSL_EXPORT int i2d_PrivateKey_bio(BIO *bp, EVP_PKEY *pkey);
OPENSSL_EXPORT int i2d_PUBKEY_bio(BIO *bp, EVP_PKEY *pkey);
OPENSSL_EXPORT int i2d_DHparams_bio(BIO *bp, const DH *dh);

OPENSSL_EXPORT int i2d_PKCS8PrivateKeyInfo_bio(BIO *bp, EVP_PKEY *key);

OPENSSL_EXPORT X509 *d2i_X509_fp(FILE *fp, X509 **x509);
OPENSSL_EXPORT X509_CRL *d2i_X509_CRL_fp(FILE *fp, X509_CRL **crl);
OPENSSL_EXPORT X509_REQ *d2i_X509_REQ_fp(FILE *fp, X509_REQ **req);
OPENSSL_EXPORT RSA *d2i_RSAPrivateKey_fp(FILE *fp, RSA **rsa);
OPENSSL_EXPORT RSA *d2i_RSAPublicKey_fp(FILE *fp, RSA **rsa);
OPENSSL_EXPORT RSA *d2i_RSA_PUBKEY_fp(FILE *fp, RSA **rsa);
OPENSSL_EXPORT DSA *d2i_DSA_PUBKEY_fp(FILE *fp, DSA **dsa);
OPENSSL_EXPORT DSA *d2i_DSAPrivateKey_fp(FILE *fp, DSA **dsa);
OPENSSL_EXPORT EC_KEY *d2i_EC_PUBKEY_fp(FILE *fp, EC_KEY **eckey);
OPENSSL_EXPORT EC_KEY *d2i_ECPrivateKey_fp(FILE *fp, EC_KEY **eckey);
OPENSSL_EXPORT X509_SIG *d2i_PKCS8_fp(FILE *fp, X509_SIG **p8);
OPENSSL_EXPORT PKCS8_PRIV_KEY_INFO *d2i_PKCS8_PRIV_KEY_INFO_fp(
    FILE *fp, PKCS8_PRIV_KEY_INFO **p8inf);
OPENSSL_EXPORT EVP_PKEY *d2i_PrivateKey_fp(FILE *fp, EVP_PKEY **a);
OPENSSL_EXPORT EVP_PKEY *d2i_PUBKEY_fp(FILE *fp, EVP_PKEY **a);

OPENSSL_EXPORT int i2d_X509_fp(FILE *fp, X509 *x509);
OPENSSL_EXPORT int i2d_X509_CRL_fp(FILE *fp, X509_CRL *crl);
OPENSSL_EXPORT int i2d_X509_REQ_fp(FILE *fp, X509_REQ *req);
OPENSSL_EXPORT int i2d_RSAPrivateKey_fp(FILE *fp, RSA *rsa);
OPENSSL_EXPORT int i2d_RSAPublicKey_fp(FILE *fp, RSA *rsa);
OPENSSL_EXPORT int i2d_RSA_PUBKEY_fp(FILE *fp, RSA *rsa);
OPENSSL_EXPORT int i2d_DSA_PUBKEY_fp(FILE *fp, DSA *dsa);
OPENSSL_EXPORT int i2d_DSAPrivateKey_fp(FILE *fp, DSA *dsa);
OPENSSL_EXPORT int i2d_EC_PUBKEY_fp(FILE *fp, EC_KEY *eckey);
OPENSSL_EXPORT int i2d_ECPrivateKey_fp(FILE *fp, EC_KEY *eckey);
OPENSSL_EXPORT int i2d_PKCS8_fp(FILE *fp, X509_SIG *p8);
OPENSSL_EXPORT int i2d_PKCS8_PRIV_KEY_INFO_fp(FILE *fp,
                                              PKCS8_PRIV_KEY_INFO *p8inf);
OPENSSL_EXPORT int i2d_PKCS8PrivateKeyInfo_fp(FILE *fp, EVP_PKEY *key);
OPENSSL_EXPORT int i2d_PrivateKey_fp(FILE *fp, EVP_PKEY *pkey);
OPENSSL_EXPORT int i2d_PUBKEY_fp(FILE *fp, EVP_PKEY *pkey);

OPENSSL_EXPORT X509 *X509_find_by_issuer_and_serial(const STACK_OF(X509) *sk,
                                                    X509_NAME *name,
                                                    const ASN1_INTEGER *serial);

OPENSSL_EXPORT X509 *X509_find_by_subject(const STACK_OF(X509) *sk,
                                          X509_NAME *name);

OPENSSL_EXPORT int X509_cmp_time(const ASN1_TIME *s, const time_t *t);

OPENSSL_EXPORT int X509_cmp_time_posix(const ASN1_TIME *s, int64_t t);

OPENSSL_EXPORT int X509_cmp_current_time(const ASN1_TIME *s);

OPENSSL_EXPORT ASN1_TIME *X509_time_adj(ASN1_TIME *s, long offset_sec,
                                        const time_t *t);

OPENSSL_EXPORT ASN1_TIME *X509_time_adj_ex(ASN1_TIME *s, int offset_day,
                                           long offset_sec, const time_t *t);

OPENSSL_EXPORT ASN1_TIME *X509_gmtime_adj(ASN1_TIME *s, long offset_sec);

OPENSSL_EXPORT int X509_issuer_name_cmp(const X509 *a, const X509 *b);

OPENSSL_EXPORT int X509_subject_name_cmp(const X509 *a, const X509 *b);

OPENSSL_EXPORT int X509_CRL_cmp(const X509_CRL *a, const X509_CRL *b);

OPENSSL_EXPORT uint32_t X509_issuer_name_hash(X509 *x509);

OPENSSL_EXPORT uint32_t X509_subject_name_hash(X509 *x509);

OPENSSL_EXPORT uint32_t X509_issuer_name_hash_old(X509 *x509);

OPENSSL_EXPORT uint32_t X509_subject_name_hash_old(X509 *x509);

OPENSSL_EXPORT int X509_get_ex_new_index(long argl, void *argp,
                                         CRYPTO_EX_unused *unused,
                                         CRYPTO_EX_dup *dup_unused,
                                         CRYPTO_EX_free *free_func);
OPENSSL_EXPORT int X509_set_ex_data(X509 *r, int idx, void *arg);
OPENSSL_EXPORT void *X509_get_ex_data(X509 *r, int idx);

OPENSSL_EXPORT int X509_STORE_CTX_get_ex_new_index(long argl, void *argp,
                                                   CRYPTO_EX_unused *unused,
                                                   CRYPTO_EX_dup *dup_unused,
                                                   CRYPTO_EX_free *free_func);
OPENSSL_EXPORT int X509_STORE_CTX_set_ex_data(X509_STORE_CTX *ctx, int idx,
                                              void *data);
OPENSSL_EXPORT void *X509_STORE_CTX_get_ex_data(X509_STORE_CTX *ctx, int idx);

#define X509_STORE_CTX_set_app_data(ctx, data) \
  X509_STORE_CTX_set_ex_data(ctx, 0, data)
#define X509_STORE_CTX_get_app_data(ctx) X509_STORE_CTX_get_ex_data(ctx, 0)

OPENSSL_EXPORT int ASN1_digest(i2d_of_void *i2d, const EVP_MD *type, char *data,
                               unsigned char *md, unsigned int *len);

OPENSSL_EXPORT int ASN1_item_digest(const ASN1_ITEM *it, const EVP_MD *type,
                                    void *data, unsigned char *md,
                                    unsigned int *len);

OPENSSL_EXPORT int ASN1_item_verify(const ASN1_ITEM *it,
                                    const X509_ALGOR *algor1,
                                    const ASN1_BIT_STRING *signature,
                                    void *data, EVP_PKEY *pkey);

OPENSSL_EXPORT int ASN1_item_sign(const ASN1_ITEM *it, X509_ALGOR *algor1,
                                  X509_ALGOR *algor2,
                                  ASN1_BIT_STRING *signature, void *data,
                                  EVP_PKEY *pkey, const EVP_MD *type);

OPENSSL_EXPORT int ASN1_item_sign_ctx(const ASN1_ITEM *it, X509_ALGOR *algor1,
                                      X509_ALGOR *algor2,
                                      ASN1_BIT_STRING *signature, void *asn,
                                      EVP_MD_CTX *ctx);

OPENSSL_EXPORT int X509_supported_extension(const X509_EXTENSION *ex);

OPENSSL_EXPORT int X509_check_ca(X509 *x509);

OPENSSL_EXPORT int X509_check_issued(X509 *issuer, X509 *subject);

OPENSSL_EXPORT int NAME_CONSTRAINTS_check(X509 *x509, NAME_CONSTRAINTS *nc);

OPENSSL_EXPORT int X509_check_host(const X509 *x509, const char *chk,
                                   size_t chklen, unsigned int flags,
                                   char **out_peername);

OPENSSL_EXPORT int X509_check_email(const X509 *x509, const char *chk,
                                    size_t chklen, unsigned int flags);

OPENSSL_EXPORT int X509_check_ip(const X509 *x509, const uint8_t *chk,
                                 size_t chklen, unsigned int flags);

OPENSSL_EXPORT int X509_check_ip_asc(const X509 *x509, const char *ipasc,
                                     unsigned int flags);

OPENSSL_EXPORT int X509_STORE_CTX_get1_issuer(X509 **out_issuer,
                                              X509_STORE_CTX *ctx, X509 *x509);

OPENSSL_EXPORT int X509_check_purpose(X509 *x509, int purpose, int ca);

#define X509_TRUST_TRUSTED 1
#define X509_TRUST_REJECTED 2
#define X509_TRUST_UNTRUSTED 3

OPENSSL_EXPORT int X509_check_trust(X509 *x509, int id, int flags);

OPENSSL_EXPORT STACK_OF(X509) *X509_STORE_CTX_get1_certs(X509_STORE_CTX *ctx,
                                                         X509_NAME *name);

OPENSSL_EXPORT STACK_OF(X509_CRL) *X509_STORE_CTX_get1_crls(X509_STORE_CTX *ctx,
                                                            X509_NAME *name);

OPENSSL_EXPORT int X509_STORE_CTX_get_by_subject(X509_STORE_CTX *ctx, int type,
                                                 X509_NAME *name,
                                                 X509_OBJECT *ret);

struct private_key_st {
  EVP_PKEY *dec_pkey;
}                ;

struct X509_info_st {
  X509 *x509;
  X509_CRL *crl;
  X509_PKEY *x_pkey;

  EVP_CIPHER_INFO enc_cipher;
  int enc_len;
  char *enc_data;
}                ;

DEFINE_STACK_OF(X509_INFO)

OPENSSL_EXPORT void X509_INFO_free(X509_INFO *info);

typedef void *(*X509V3_EXT_NEW)(void);
typedef void (*X509V3_EXT_FREE)(void *ext);
typedef void *(*X509V3_EXT_D2I)(void *ext, const uint8_t **inp, long len);
typedef int (*X509V3_EXT_I2D)(void *ext, uint8_t **outp);
typedef STACK_OF(CONF_VALUE) *(*X509V3_EXT_I2V)(const X509V3_EXT_METHOD *method,
                                                void *ext,
                                                STACK_OF(CONF_VALUE) *extlist);
typedef void *(*X509V3_EXT_V2I)(const X509V3_EXT_METHOD *method,
                                const X509V3_CTX *ctx,
                                const STACK_OF(CONF_VALUE) *values);
typedef char *(*X509V3_EXT_I2S)(const X509V3_EXT_METHOD *method, void *ext);
typedef void *(*X509V3_EXT_S2I)(const X509V3_EXT_METHOD *method,
                                const X509V3_CTX *ctx, const char *str);
typedef int (*X509V3_EXT_I2R)(const X509V3_EXT_METHOD *method, void *ext,
                              BIO *out, int indent);
typedef void *(*X509V3_EXT_R2I)(const X509V3_EXT_METHOD *method,
                                const X509V3_CTX *ctx, const char *str);

struct v3_ext_method {

  int ext_nid;

  int ext_flags;

  ASN1_ITEM_EXP *it;

  X509V3_EXT_NEW ext_new;
  X509V3_EXT_FREE ext_free;
  X509V3_EXT_D2I d2i;
  X509V3_EXT_I2D i2d;

  X509V3_EXT_I2S i2s;
  X509V3_EXT_S2I s2i;

  X509V3_EXT_I2V i2v;
  X509V3_EXT_V2I v2i;

  X509V3_EXT_I2R i2r;
  X509V3_EXT_R2I r2i;

  void *usr_data;
}                        ;

#define X509V3_EXT_MULTILINE 0x4

OPENSSL_EXPORT const X509V3_EXT_METHOD *X509V3_EXT_get(
    const X509_EXTENSION *ext);

OPENSSL_EXPORT const X509V3_EXT_METHOD *X509V3_EXT_get_nid(int nid);

OPENSSL_EXPORT OPENSSL_DEPRECATED int X509V3_EXT_add(X509V3_EXT_METHOD *ext);

OPENSSL_EXPORT OPENSSL_DEPRECATED int X509V3_EXT_add_alias(int nid_to,
                                                           int nid_from);

struct v3_ext_ctx {
  int flags;
  const X509 *issuer_cert;
  const X509 *subject_cert;
  const X509_REQ *subject_req;
  const X509_CRL *crl;
  const CONF *db;
};

#define X509V3_CTX_TEST 0x1

OPENSSL_EXPORT void X509V3_set_ctx(X509V3_CTX *ctx, const X509 *issuer,
                                   const X509 *subject, const X509_REQ *req,
                                   const X509_CRL *crl, int flags);

#define X509V3_set_ctx_test(ctx) \
  X509V3_set_ctx(ctx, NULL, NULL, NULL, NULL, X509V3_CTX_TEST)

OPENSSL_EXPORT void X509V3_set_nconf(X509V3_CTX *ctx, const CONF *conf);

#define X509V3_set_ctx_nodb(ctx) X509V3_set_nconf(ctx, NULL)

OPENSSL_EXPORT X509_EXTENSION *X509V3_EXT_nconf(const CONF *conf,
                                                const X509V3_CTX *ctx,
                                                const char *name,
                                                const char *value);

OPENSSL_EXPORT X509_EXTENSION *X509V3_EXT_nconf_nid(const CONF *conf,
                                                    const X509V3_CTX *ctx,
                                                    int ext_nid,
                                                    const char *value);

OPENSSL_EXPORT X509_EXTENSION *X509V3_EXT_conf_nid(LHASH_OF(CONF_VALUE) *conf,
                                                   const X509V3_CTX *ctx,
                                                   int ext_nid,
                                                   const char *value);

OPENSSL_EXPORT int X509V3_EXT_add_nconf_sk(const CONF *conf,
                                           const X509V3_CTX *ctx,
                                           const char *section,
                                           STACK_OF(X509_EXTENSION) **sk);

OPENSSL_EXPORT int X509V3_EXT_add_nconf(const CONF *conf, const X509V3_CTX *ctx,
                                        const char *section, X509 *cert);

OPENSSL_EXPORT int X509V3_EXT_REQ_add_nconf(const CONF *conf,
                                            const X509V3_CTX *ctx,
                                            const char *section, X509_REQ *req);

OPENSSL_EXPORT int X509V3_EXT_CRL_add_nconf(const CONF *conf,
                                            const X509V3_CTX *ctx,
                                            const char *section, X509_CRL *crl);

OPENSSL_EXPORT char *i2s_ASN1_OCTET_STRING(const X509V3_EXT_METHOD *method,
                                           const ASN1_OCTET_STRING *oct);

OPENSSL_EXPORT ASN1_OCTET_STRING *s2i_ASN1_OCTET_STRING(
    const X509V3_EXT_METHOD *method, const X509V3_CTX *ctx, const char *str);

OPENSSL_EXPORT char *i2s_ASN1_INTEGER(const X509V3_EXT_METHOD *method,
                                      const ASN1_INTEGER *aint);

OPENSSL_EXPORT ASN1_INTEGER *s2i_ASN1_INTEGER(const X509V3_EXT_METHOD *method,
                                              const char *value);

OPENSSL_EXPORT char *i2s_ASN1_ENUMERATED(const X509V3_EXT_METHOD *method,
                                         const ASN1_ENUMERATED *aint);

OPENSSL_EXPORT void X509V3_conf_free(CONF_VALUE *val);

OPENSSL_EXPORT STACK_OF(CONF_VALUE) *i2v_GENERAL_NAME(
    const X509V3_EXT_METHOD *method, const GENERAL_NAME *gen,
    STACK_OF(CONF_VALUE) *ret);

OPENSSL_EXPORT STACK_OF(CONF_VALUE) *i2v_GENERAL_NAMES(
    const X509V3_EXT_METHOD *method, const GENERAL_NAMES *gen,
    STACK_OF(CONF_VALUE) *extlist);

OPENSSL_EXPORT ASN1_OCTET_STRING *a2i_IPADDRESS(const char *ipasc);

OPENSSL_EXPORT ASN1_OCTET_STRING *a2i_IPADDRESS_NC(const char *ipasc);

OPENSSL_EXPORT ASN1_TIME *X509_get_notBefore(const X509 *x509);

OPENSSL_EXPORT ASN1_TIME *X509_get_notAfter(const X509 *x509);

OPENSSL_EXPORT int X509_set_notBefore(X509 *x509, const ASN1_TIME *tm);

OPENSSL_EXPORT int X509_set_notAfter(X509 *x509, const ASN1_TIME *tm);

OPENSSL_EXPORT ASN1_TIME *X509_CRL_get_lastUpdate(X509_CRL *crl);

OPENSSL_EXPORT ASN1_TIME *X509_CRL_get_nextUpdate(X509_CRL *crl);

#define X509_extract_key(x) X509_get_pubkey(x)

#define X509_REQ_extract_key(a) X509_REQ_get_pubkey(a)

#define X509_name_cmp(a, b) X509_NAME_cmp((a), (b))

#define X509_CRL_set_lastUpdate X509_CRL_set1_lastUpdate
#define X509_CRL_set_nextUpdate X509_CRL_set1_nextUpdate

OPENSSL_EXPORT ASN1_INTEGER *X509_get_serialNumber(X509 *x509);

OPENSSL_EXPORT int X509_NAME_get_text_by_OBJ(const X509_NAME *name,
                                             const ASN1_OBJECT *obj, char *buf,
                                             int len);

OPENSSL_EXPORT int X509_NAME_get_text_by_NID(const X509_NAME *name, int nid,
                                             char *buf, int len);

OPENSSL_EXPORT X509_STORE_CTX *X509_STORE_CTX_get0_parent_ctx(
    const X509_STORE_CTX *ctx);

OPENSSL_EXPORT void X509_OBJECT_free_contents(X509_OBJECT *obj);

OPENSSL_EXPORT void X509_LOOKUP_free(X509_LOOKUP *ctx);

OPENSSL_EXPORT void X509_STORE_CTX_cleanup(X509_STORE_CTX *ctx);

OPENSSL_EXPORT int X509V3_add_standard_extensions(void);

#define X509_STORE_get_by_subject X509_STORE_CTX_get_by_subject
#define X509_STORE_get1_certs X509_STORE_CTX_get1_certs
#define X509_STORE_get1_crls X509_STORE_CTX_get1_crls

OPENSSL_EXPORT STACK_OF(X509) *X509_STORE_CTX_get_chain(
    const X509_STORE_CTX *ctx);

OPENSSL_EXPORT void X509_STORE_CTX_trusted_stack(X509_STORE_CTX *ctx,
                                                 STACK_OF(X509) *sk);

typedef int (*X509_STORE_CTX_verify_cb)(int, X509_STORE_CTX *);

OPENSSL_EXPORT void X509_STORE_CTX_set_verify_cb(
    X509_STORE_CTX *ctx, int (*verify_cb)(int ok, X509_STORE_CTX *ctx));

OPENSSL_EXPORT void X509_STORE_set_verify_cb(
    X509_STORE *store, X509_STORE_CTX_verify_cb verify_cb);

#define X509_STORE_set_verify_cb_func(store, func) \
  X509_STORE_set_verify_cb((store), (func))

OPENSSL_EXPORT void X509_STORE_CTX_set_chain(X509_STORE_CTX *ctx,
                                             STACK_OF(X509) *sk);

#define X509_CHECK_FLAG_ALWAYS_CHECK_SUBJECT 0
#define X509_CHECK_FLAG_MULTI_LABEL_WILDCARDS 0
#define X509_CHECK_FLAG_SINGLE_LABEL_SUBDOMAINS 0

#define X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS 0

OPENSSL_EXPORT STACK_OF(X509_OBJECT) *X509_STORE_get0_objects(
    X509_STORE *store);

OPENSSL_EXPORT int X509_PURPOSE_get_by_sname(const char *sname);

OPENSSL_EXPORT const X509_PURPOSE *X509_PURPOSE_get0(int id);

OPENSSL_EXPORT int X509_PURPOSE_get_id(const X509_PURPOSE *purpose);

#define NS_SSL_CLIENT 0x80
#define NS_SSL_SERVER 0x40
#define NS_SMIME 0x20
#define NS_OBJSIGN 0x10
#define NS_SSL_CA 0x04
#define NS_SMIME_CA 0x02
#define NS_OBJSIGN_CA 0x01
#define NS_ANY_CA (NS_SSL_CA | NS_SMIME_CA | NS_OBJSIGN_CA)

struct X509_algor_st {
  ASN1_OBJECT *algorithm;
  ASN1_TYPE *parameter;
}                 ;

#if defined(__cplusplus)
}
#endif

#if !defined(BORINGSSL_NO_CXX)
extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(ACCESS_DESCRIPTION, ACCESS_DESCRIPTION_free)
BORINGSSL_MAKE_DELETER(AUTHORITY_KEYID, AUTHORITY_KEYID_free)
BORINGSSL_MAKE_DELETER(BASIC_CONSTRAINTS, BASIC_CONSTRAINTS_free)

BORINGSSL_MAKE_DELETER(CONF_VALUE, X509V3_conf_free)
BORINGSSL_MAKE_DELETER(DIST_POINT, DIST_POINT_free)
BORINGSSL_MAKE_DELETER(GENERAL_NAME, GENERAL_NAME_free)
BORINGSSL_MAKE_DELETER(GENERAL_SUBTREE, GENERAL_SUBTREE_free)
BORINGSSL_MAKE_DELETER(NAME_CONSTRAINTS, NAME_CONSTRAINTS_free)
BORINGSSL_MAKE_DELETER(NETSCAPE_SPKI, NETSCAPE_SPKI_free)
BORINGSSL_MAKE_DELETER(POLICY_MAPPING, POLICY_MAPPING_free)
BORINGSSL_MAKE_DELETER(POLICYINFO, POLICYINFO_free)
BORINGSSL_MAKE_DELETER(RSA_PSS_PARAMS, RSA_PSS_PARAMS_free)
BORINGSSL_MAKE_DELETER(X509, X509_free)
BORINGSSL_MAKE_UP_REF(X509, X509_up_ref)
BORINGSSL_MAKE_DELETER(X509_ALGOR, X509_ALGOR_free)
BORINGSSL_MAKE_DELETER(X509_ATTRIBUTE, X509_ATTRIBUTE_free)
BORINGSSL_MAKE_DELETER(X509_CRL, X509_CRL_free)
BORINGSSL_MAKE_UP_REF(X509_CRL, X509_CRL_up_ref)
BORINGSSL_MAKE_DELETER(X509_EXTENSION, X509_EXTENSION_free)
BORINGSSL_MAKE_DELETER(X509_INFO, X509_INFO_free)
BORINGSSL_MAKE_DELETER(X509_LOOKUP, X509_LOOKUP_free)
BORINGSSL_MAKE_DELETER(X509_NAME, X509_NAME_free)
BORINGSSL_MAKE_DELETER(X509_NAME_ENTRY, X509_NAME_ENTRY_free)
BORINGSSL_MAKE_DELETER(X509_OBJECT, X509_OBJECT_free)
BORINGSSL_MAKE_DELETER(X509_PUBKEY, X509_PUBKEY_free)
BORINGSSL_MAKE_DELETER(X509_REQ, X509_REQ_free)
BORINGSSL_MAKE_DELETER(X509_REVOKED, X509_REVOKED_free)
BORINGSSL_MAKE_DELETER(X509_SIG, X509_SIG_free)
BORINGSSL_MAKE_DELETER(X509_STORE, X509_STORE_free)
BORINGSSL_MAKE_UP_REF(X509_STORE, X509_STORE_up_ref)
BORINGSSL_MAKE_DELETER(X509_STORE_CTX, X509_STORE_CTX_free)
BORINGSSL_MAKE_DELETER(X509_VERIFY_PARAM, X509_VERIFY_PARAM_free)

BSSL_NAMESPACE_END

}
#endif

#define X509_R_AKID_MISMATCH 100
#define X509_R_BAD_PKCS7_VERSION 101
#define X509_R_BAD_X509_FILETYPE 102
#define X509_R_BASE64_DECODE_ERROR 103
#define X509_R_CANT_CHECK_DH_KEY 104
#define X509_R_CERT_ALREADY_IN_HASH_TABLE 105
#define X509_R_CRL_ALREADY_DELTA 106
#define X509_R_CRL_VERIFY_FAILURE 107
#define X509_R_IDP_MISMATCH 108
#define X509_R_INVALID_BIT_STRING_BITS_LEFT 109
#define X509_R_INVALID_DIRECTORY 110
#define X509_R_INVALID_FIELD_NAME 111
#define X509_R_INVALID_PSS_PARAMETERS 112
#define X509_R_INVALID_TRUST 113
#define X509_R_ISSUER_MISMATCH 114
#define X509_R_KEY_TYPE_MISMATCH 115
#define X509_R_KEY_VALUES_MISMATCH 116
#define X509_R_LOADING_CERT_DIR 117
#define X509_R_LOADING_DEFAULTS 118
#define X509_R_NEWER_CRL_NOT_NEWER 119
#define X509_R_NOT_PKCS7_SIGNED_DATA 120
#define X509_R_NO_CERTIFICATES_INCLUDED 121
#define X509_R_NO_CERT_SET_FOR_US_TO_VERIFY 122
#define X509_R_NO_CRLS_INCLUDED 123
#define X509_R_NO_CRL_NUMBER 124
#define X509_R_PUBLIC_KEY_DECODE_ERROR 125
#define X509_R_PUBLIC_KEY_ENCODE_ERROR 126
#define X509_R_SHOULD_RETRY 127
#define X509_R_UNKNOWN_KEY_TYPE 128
#define X509_R_UNKNOWN_NID 129
#define X509_R_UNKNOWN_PURPOSE_ID 130
#define X509_R_UNKNOWN_TRUST_ID 131
#define X509_R_UNSUPPORTED_ALGORITHM 132
#define X509_R_WRONG_LOOKUP_TYPE 133
#define X509_R_WRONG_TYPE 134
#define X509_R_NAME_TOO_LONG 135
#define X509_R_INVALID_PARAMETER 136
#define X509_R_SIGNATURE_ALGORITHM_MISMATCH 137
#define X509_R_DELTA_CRL_WITHOUT_CRL_NUMBER 138
#define X509_R_INVALID_FIELD_FOR_VERSION 139
#define X509_R_INVALID_VERSION 140
#define X509_R_NO_CERTIFICATE_FOUND 141
#define X509_R_NO_CERTIFICATE_OR_CRL_FOUND 142
#define X509_R_NO_CRL_FOUND 143
#define X509_R_INVALID_POLICY_EXTENSION 144

#endif
