// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_OBJ_H
#define OPENSSL_HEADER_OBJ_H

#include <openssl/base.h>

#include <openssl/bytestring.h>
#include <openssl/nid.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT ASN1_OBJECT *OBJ_dup(const ASN1_OBJECT *obj);

OPENSSL_EXPORT int OBJ_cmp(const ASN1_OBJECT *a, const ASN1_OBJECT *b);

OPENSSL_EXPORT const uint8_t *OBJ_get0_data(const ASN1_OBJECT *obj);

OPENSSL_EXPORT size_t OBJ_length(const ASN1_OBJECT *obj);

OPENSSL_EXPORT int OBJ_obj2nid(const ASN1_OBJECT *obj);

OPENSSL_EXPORT int OBJ_cbs2nid(const CBS *cbs);

OPENSSL_EXPORT int OBJ_sn2nid(const char *short_name);

OPENSSL_EXPORT int OBJ_ln2nid(const char *long_name);

OPENSSL_EXPORT int OBJ_txt2nid(const char *s);

OPENSSL_EXPORT ASN1_OBJECT *OBJ_nid2obj(int nid);

OPENSSL_EXPORT const ASN1_OBJECT *OBJ_get_undef(void);

OPENSSL_EXPORT const char *OBJ_nid2sn(int nid);

OPENSSL_EXPORT const char *OBJ_nid2ln(int nid);

OPENSSL_EXPORT int OBJ_nid2cbb(CBB *out, int nid);

OPENSSL_EXPORT ASN1_OBJECT *OBJ_txt2obj(const char *s, int dont_search_names);

OPENSSL_EXPORT int OBJ_obj2txt(char *out, int out_len, const ASN1_OBJECT *obj,
                               int always_return_oid);

OPENSSL_EXPORT int OBJ_create(const char *oid, const char *short_name,
                              const char *long_name);

OPENSSL_EXPORT int OBJ_find_sigid_algs(int sign_nid, int *out_digest_nid,
                                       int *out_pkey_nid);

OPENSSL_EXPORT int OBJ_find_sigid_by_algs(int *out_sign_nid, int digest_nid,
                                          int pkey_nid);

typedef struct obj_name_st {
  int type;
  int alias;
  const char *name;
  const char *data;
} OBJ_NAME;

#define OBJ_NAME_TYPE_MD_METH 1
#define OBJ_NAME_TYPE_CIPHER_METH 2

OPENSSL_EXPORT void OBJ_NAME_do_all_sorted(
    int type, void (*callback)(const OBJ_NAME *, void *arg), void *arg);

OPENSSL_EXPORT void OBJ_NAME_do_all(int type, void (*callback)(const OBJ_NAME *,
                                                               void *arg),
                                    void *arg);

OPENSSL_EXPORT void OBJ_cleanup(void);

#if defined(__cplusplus)
}
#endif

#define OBJ_R_UNKNOWN_NID 100
#define OBJ_R_INVALID_OID_STRING 101

#endif
