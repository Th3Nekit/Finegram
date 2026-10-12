// Copyright 2005-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_CRYPTO_ASN1_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_ASN1_INTERNAL_H

#include <time.h>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT struct tm *OPENSSL_gmtime(const time_t *time, struct tm *result);

OPENSSL_EXPORT int OPENSSL_gmtime_adj(struct tm *tm, int offset_day,
                                      int64_t offset_sec);

OPENSSL_EXPORT int OPENSSL_gmtime_diff(int *out_days, int *out_secs,
                                       const struct tm *from,
                                       const struct tm *to);

#define ASN1_OBJECT_FLAG_DYNAMIC 0x01          // internal use
#define ASN1_OBJECT_FLAG_DYNAMIC_STRINGS 0x04  // internal use
#define ASN1_OBJECT_FLAG_DYNAMIC_DATA 0x08     // internal use

struct asn1_object_st {
  const char *sn, *ln;
  int nid;
  int length;
  const unsigned char *data;
  int flags;
};

ASN1_OBJECT *ASN1_OBJECT_new(void);

typedef struct ASN1_ENCODING_st {

  uint8_t *enc;

  size_t len;

  CRYPTO_BUFFER *buf;
} ASN1_ENCODING;

OPENSSL_EXPORT int asn1_utctime_to_tm(struct tm *tm, const ASN1_UTCTIME *d,
                                      int allow_timezone_offset);
OPENSSL_EXPORT int asn1_generalizedtime_to_tm(struct tm *tm,
                                              const ASN1_GENERALIZEDTIME *d);

int ASN1_item_ex_new(ASN1_VALUE **pval, const ASN1_ITEM *it);
void ASN1_item_ex_free(ASN1_VALUE **pval, const ASN1_ITEM *it);

void ASN1_template_free(ASN1_VALUE **pval, const ASN1_TEMPLATE *tt);

int ASN1_item_ex_d2i(ASN1_VALUE **pval, const unsigned char **in, long len,
                     const ASN1_ITEM *it, int tag, int aclass, char opt,
                     CRYPTO_BUFFER *buf);

int ASN1_item_ex_i2d(ASN1_VALUE **pval, unsigned char **out,
                     const ASN1_ITEM *it, int tag, int aclass);

void ASN1_primitive_free(ASN1_VALUE **pval, const ASN1_ITEM *it);

int asn1_get_choice_selector(ASN1_VALUE **pval, const ASN1_ITEM *it);

int asn1_set_choice_selector(ASN1_VALUE **pval, int value, const ASN1_ITEM *it);

ASN1_VALUE **asn1_get_field_ptr(ASN1_VALUE **pval, const ASN1_TEMPLATE *tt);

const ASN1_TEMPLATE *asn1_do_adb(ASN1_VALUE **pval, const ASN1_TEMPLATE *tt,
                                 int nullerr);

void asn1_refcount_set_one(ASN1_VALUE **pval, const ASN1_ITEM *it);
int asn1_refcount_dec_and_test_zero(ASN1_VALUE **pval, const ASN1_ITEM *it);

void asn1_enc_init(ASN1_VALUE **pval, const ASN1_ITEM *it);
void asn1_enc_free(ASN1_VALUE **pval, const ASN1_ITEM *it);

int asn1_enc_restore(int *len, unsigned char **out, ASN1_VALUE **pval,
                     const ASN1_ITEM *it);

int asn1_enc_save(ASN1_VALUE **pval, const uint8_t *in, size_t inlen,
                  const ASN1_ITEM *it, CRYPTO_BUFFER *buf);

void asn1_encoding_clear(ASN1_ENCODING *enc);

const void *asn1_type_value_as_pointer(const ASN1_TYPE *a);

void asn1_type_set0_string(ASN1_TYPE *a, ASN1_STRING *str);

void asn1_type_cleanup(ASN1_TYPE *a);

int asn1_is_printable(uint32_t value);

int asn1_bit_string_length(const ASN1_BIT_STRING *str,
                           uint8_t *out_padding_bits);

typedef struct {
  int nid;
  long minsize;
  long maxsize;
  unsigned long mask;
  unsigned long flags;
} ASN1_STRING_TABLE;

OPENSSL_EXPORT void asn1_get_string_table_for_testing(
    const ASN1_STRING_TABLE **out_ptr, size_t *out_len);

typedef ASN1_VALUE *ASN1_new_func(void);
typedef void ASN1_free_func(ASN1_VALUE *a);
typedef ASN1_VALUE *ASN1_d2i_func(ASN1_VALUE **a, const unsigned char **in,
                                  long length);
typedef int ASN1_i2d_func(ASN1_VALUE *a, unsigned char **in);

typedef int ASN1_ex_d2i(ASN1_VALUE **pval, const unsigned char **in, long len,
                        const ASN1_ITEM *it, int opt, ASN1_TLC *ctx);

typedef int ASN1_ex_i2d(ASN1_VALUE **pval, unsigned char **out,
                        const ASN1_ITEM *it);
typedef int ASN1_ex_new_func(ASN1_VALUE **pval, const ASN1_ITEM *it);
typedef void ASN1_ex_free_func(ASN1_VALUE **pval, const ASN1_ITEM *it);

typedef struct ASN1_EXTERN_FUNCS_st {
  ASN1_ex_new_func *asn1_ex_new;
  ASN1_ex_free_func *asn1_ex_free;
  ASN1_ex_d2i *asn1_ex_d2i;
  ASN1_ex_i2d *asn1_ex_i2d;
} ASN1_EXTERN_FUNCS;

#if defined(__cplusplus)
}
#endif

#endif
