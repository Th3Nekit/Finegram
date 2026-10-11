// Copyright 2013-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_CRYPTO_X509_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_X509_INTERNAL_H

#include <openssl/base.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

#include "../asn1/internal.h"
#include "../internal.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct X509_val_st {
  ASN1_TIME *notBefore;
  ASN1_TIME *notAfter;
} X509_VAL;

DECLARE_ASN1_FUNCTIONS_const(X509_VAL)

struct X509_pubkey_st {
  X509_ALGOR *algor;
  ASN1_BIT_STRING *public_key;
  EVP_PKEY *pkey;
}                  ;

DECLARE_ASN1_ITEM(X509_PUBKEY)

struct X509_name_entry_st {
  ASN1_OBJECT *object;
  ASN1_STRING *value;
  int set;
}                      ;

DECLARE_ASN1_ITEM(X509_NAME_ENTRY)

struct X509_name_st {
  STACK_OF(X509_NAME_ENTRY) *entries;
  int modified;
  BUF_MEM *bytes;
  unsigned char *canon_enc;
  int canon_enclen;
}                ;

struct x509_attributes_st {
  ASN1_OBJECT *object;
  STACK_OF(ASN1_TYPE) *set;
}                     ;

DECLARE_ASN1_ITEM(X509_ATTRIBUTE)

typedef struct x509_cert_aux_st {
  STACK_OF(ASN1_OBJECT) *trust;
  STACK_OF(ASN1_OBJECT) *reject;
  ASN1_UTF8STRING *alias;
  ASN1_OCTET_STRING *keyid;
} X509_CERT_AUX;

DECLARE_ASN1_FUNCTIONS_const(X509_CERT_AUX)

struct X509_extension_st {
  ASN1_OBJECT *object;
  ASN1_BOOLEAN critical;
  ASN1_OCTET_STRING *value;
}                     ;

DECLARE_ASN1_ITEM(X509_EXTENSION)

DECLARE_ASN1_ITEM(X509_EXTENSIONS)

typedef struct {
  ASN1_INTEGER *version;
  ASN1_INTEGER *serialNumber;
  X509_ALGOR *signature;
  X509_NAME *issuer;
  X509_VAL *validity;
  X509_NAME *subject;
  X509_PUBKEY *key;
  ASN1_BIT_STRING *issuerUID;
  ASN1_BIT_STRING *subjectUID;
  STACK_OF(X509_EXTENSION) *extensions;
  ASN1_ENCODING enc;
} X509_CINF;

DECLARE_ASN1_FUNCTIONS(X509_CINF)

struct x509_st {
  X509_CINF *cert_info;
  X509_ALGOR *sig_alg;
  ASN1_BIT_STRING *signature;
  CRYPTO_refcount_t references;
  CRYPTO_EX_DATA ex_data;

  long ex_pathlen;
  uint32_t ex_flags;
  uint32_t ex_kusage;
  uint32_t ex_xkusage;
  ASN1_OCTET_STRING *skid;
  AUTHORITY_KEYID *akid;
  STACK_OF(DIST_POINT) *crldp;
  STACK_OF(GENERAL_NAME) *altname;
  NAME_CONSTRAINTS *nc;
  unsigned char cert_hash[SHA256_DIGEST_LENGTH];
  X509_CERT_AUX *aux;
  CRYPTO_MUTEX lock;
}           ;

DECLARE_ASN1_ITEM(X509)

typedef struct {
  ASN1_ENCODING enc;
  ASN1_INTEGER *version;
  X509_NAME *subject;
  X509_PUBKEY *pubkey;

  STACK_OF(X509_ATTRIBUTE) *attributes;
} X509_REQ_INFO;

DECLARE_ASN1_FUNCTIONS(X509_REQ_INFO)

struct X509_req_st {
  X509_REQ_INFO *req_info;
  X509_ALGOR *sig_alg;
  ASN1_BIT_STRING *signature;
}               ;

DECLARE_ASN1_ITEM(X509_REQ)

struct x509_revoked_st {
  ASN1_INTEGER *serialNumber;
  ASN1_TIME *revocationDate;
  STACK_OF(X509_EXTENSION)                *extensions;

  int reason;
}                   ;

DECLARE_ASN1_ITEM(X509_REVOKED)

typedef struct {
  ASN1_INTEGER *version;
  X509_ALGOR *sig_alg;
  X509_NAME *issuer;
  ASN1_TIME *lastUpdate;
  ASN1_TIME *nextUpdate;
  STACK_OF(X509_REVOKED) *revoked;
  STACK_OF(X509_EXTENSION)           *extensions;
  ASN1_ENCODING enc;
} X509_CRL_INFO;

DECLARE_ASN1_FUNCTIONS(X509_CRL_INFO)

#define IDP_PRESENT 0x1

#define IDP_INVALID 0x2

#define IDP_ONLYUSER 0x4

#define IDP_ONLYCA 0x8

#define IDP_ONLYATTR 0x10

#define IDP_INDIRECT 0x20

#define IDP_REASONS 0x40

struct X509_crl_st {

  X509_CRL_INFO *crl;
  X509_ALGOR *sig_alg;
  ASN1_BIT_STRING *signature;
  CRYPTO_refcount_t references;
  int flags;

  AUTHORITY_KEYID *akid;
  ISSUING_DIST_POINT *idp;

  int idp_flags;
  unsigned char crl_hash[SHA256_DIGEST_LENGTH];
}               ;

DECLARE_ASN1_ITEM(X509_CRL)

DECLARE_ASN1_ITEM(GENERAL_NAME)

DECLARE_ASN1_ITEM(GENERAL_NAMES)

struct X509_VERIFY_PARAM_st {
  int64_t check_time;
  unsigned long flags;
  int purpose;
  int trust;
  int depth;
  STACK_OF(ASN1_OBJECT) *policies;

  STACK_OF(OPENSSL_STRING) *hosts;
  unsigned int hostflags;
  char *email;
  size_t emaillen;
  unsigned char *ip;
  size_t iplen;
  unsigned char poison;
}                        ;

struct x509_object_st {

  int type;
  union {
    char *ptr;
    X509 *x509;
    X509_CRL *crl;
    EVP_PKEY *pkey;
  } data;
}                  ;

DECLARE_ASN1_ITEM(NETSCAPE_SPKI)

DECLARE_ASN1_ITEM(NETSCAPE_SPKAC)

struct x509_lookup_method_st {
  int (*new_item)(X509_LOOKUP *ctx);
  void (*free)(X509_LOOKUP *ctx);
  int (*ctrl)(X509_LOOKUP *ctx, int cmd, const char *argc, long argl,
              char **ret);
  int (*get_by_subject)(X509_LOOKUP *ctx, int type, X509_NAME *name,
                        X509_OBJECT *ret);
}                         ;

DEFINE_STACK_OF(X509_LOOKUP)

struct x509_store_st {

  STACK_OF(X509_OBJECT) *objs;
  CRYPTO_MUTEX objs_lock;

  STACK_OF(X509_LOOKUP) *get_cert_methods;

  X509_VERIFY_PARAM *param;

  X509_STORE_CTX_verify_cb verify_cb;

  CRYPTO_refcount_t references;
}                 ;

struct x509_lookup_st {
  const X509_LOOKUP_METHOD *method;
  void *method_data;

  X509_STORE *store_ctx;
}                  ;

struct x509_store_ctx_st {
  X509_STORE *ctx;

  X509 *cert;
  STACK_OF(X509) *untrusted;
  STACK_OF(X509_CRL) *crls;

  X509_VERIFY_PARAM *param;

  STACK_OF(X509) *trusted_stack;

  X509_STORE_CTX_verify_cb verify_cb;

  int last_untrusted;
  STACK_OF(X509) *chain;

  int error_depth;
  int error;
  X509 *current_cert;
  X509_CRL *current_crl;

  X509 *current_crl_issuer;
  int current_crl_score;

  CRYPTO_EX_DATA ex_data;
}                     ;

ASN1_TYPE *ASN1_generate_v3(const char *str, const X509V3_CTX *cnf);

int X509_CERT_AUX_print(BIO *bp, X509_CERT_AUX *x, int indent);

int x509_rsa_pss_to_ctx(EVP_MD_CTX *ctx, const X509_ALGOR *sigalg,
                        EVP_PKEY *pkey);

int x509_rsa_ctx_to_pss(EVP_MD_CTX *ctx, X509_ALGOR *algor);

int x509_print_rsa_pss_params(BIO *bp, const X509_ALGOR *sigalg, int indent,
                              ASN1_PCTX *pctx);

int x509_digest_sign_algorithm(EVP_MD_CTX *ctx, X509_ALGOR *algor);

int x509_digest_verify_init(EVP_MD_CTX *ctx, const X509_ALGOR *sigalg,
                            EVP_PKEY *pkey);

int X509_policy_check(const STACK_OF(X509) *certs,
                      const STACK_OF(ASN1_OBJECT) *user_policies,
                      unsigned long flags, X509 **out_current_cert);

int x509_check_issued_with_callback(X509_STORE_CTX *ctx, X509 *x, X509 *issuer);

OPENSSL_EXPORT char *x509v3_bytes_to_hex(const uint8_t *in, size_t len);

unsigned char *x509v3_hex_to_bytes(const char *str, size_t *len);

int x509v3_conf_name_matches(const char *name, const char *cmp);

OPENSSL_EXPORT int x509v3_looks_like_dns_name(const unsigned char *in,
                                              size_t len);

OPENSSL_EXPORT int x509v3_cache_extensions(X509 *x);

int x509v3_a2i_ipadd(unsigned char ipout[16], const char *ipasc);

typedef struct {
  int bitnum;
  const char *lname;
  const char *sname;
} BIT_STRING_BITNAME;

int x509V3_add_value_asn1_string(const char *name, const ASN1_STRING *value,
                                 STACK_OF(CONF_VALUE) **extlist);

int X509V3_NAME_from_section(X509_NAME *nm, const STACK_OF(CONF_VALUE) *dn_sk,
                             int chtype);

int X509V3_bool_from_string(const char *str, ASN1_BOOLEAN *out_bool);

int X509V3_get_value_bool(const CONF_VALUE *value, ASN1_BOOLEAN *out_bool);

int X509V3_get_value_int(const CONF_VALUE *value, ASN1_INTEGER **aint);

const STACK_OF(CONF_VALUE) *X509V3_get_section(const X509V3_CTX *ctx,
                                               const char *section);

int X509V3_add_value(const char *name, const char *value,
                     STACK_OF(CONF_VALUE) **extlist);

int X509V3_add_value_bool(const char *name, int asn1_bool,
                          STACK_OF(CONF_VALUE) **extlist);

int X509V3_add_value_int(const char *name, const ASN1_INTEGER *aint,
                         STACK_OF(CONF_VALUE) **extlist);

STACK_OF(CONF_VALUE) *X509V3_parse_list(const char *line);

#define X509V3_conf_err(val)                                               \
  ERR_add_error_data(6, "section:", (val)->section, ",name:", (val)->name, \
                     ",value:", (val)->value);

OPENSSL_EXPORT int GENERAL_NAME_cmp(const GENERAL_NAME *a,
                                    const GENERAL_NAME *b);

const X509_VERIFY_PARAM *X509_VERIFY_PARAM_lookup(const char *name);

GENERAL_NAME *v2i_GENERAL_NAME(const X509V3_EXT_METHOD *method,
                               const X509V3_CTX *ctx, const CONF_VALUE *cnf);
GENERAL_NAME *v2i_GENERAL_NAME_ex(GENERAL_NAME *out,
                                  const X509V3_EXT_METHOD *method,
                                  const X509V3_CTX *ctx, const CONF_VALUE *cnf,
                                  int is_nc);
GENERAL_NAMES *v2i_GENERAL_NAMES(const X509V3_EXT_METHOD *method,
                                 const X509V3_CTX *ctx,
                                 const STACK_OF(CONF_VALUE) *nval);

int X509_check_akid(X509 *issuer, const AUTHORITY_KEYID *akid);

int X509_is_valid_trust_id(int trust);

int X509_PURPOSE_get_trust(const X509_PURPOSE *xp);

int DIST_POINT_set_dpname(DIST_POINT_NAME *dpn, X509_NAME *iname);

#if defined(__cplusplus)
}
#endif

#endif
