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

#ifndef OPENSSL_HEADER_ASN1_H
#define OPENSSL_HEADER_ASN1_H

#include <openssl/base.h>

#include <time.h>

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/stack.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define V_ASN1_UNIVERSAL 0x00
#define V_ASN1_APPLICATION 0x40
#define V_ASN1_CONTEXT_SPECIFIC 0x80
#define V_ASN1_PRIVATE 0xc0

#define V_ASN1_CONSTRUCTED 0x20

#define V_ASN1_PRIMITIVE_TAG 0x1f

#define V_ASN1_MAX_UNIVERSAL 0xff

#define V_ASN1_UNDEF (-1)

#define V_ASN1_OTHER (-3)

#define V_ASN1_ANY (-4)

#define V_ASN1_EOC 0
#define V_ASN1_BOOLEAN 1
#define V_ASN1_INTEGER 2
#define V_ASN1_BIT_STRING 3
#define V_ASN1_OCTET_STRING 4
#define V_ASN1_NULL 5
#define V_ASN1_OBJECT 6
#define V_ASN1_OBJECT_DESCRIPTOR 7
#define V_ASN1_EXTERNAL 8
#define V_ASN1_REAL 9
#define V_ASN1_ENUMERATED 10
#define V_ASN1_UTF8STRING 12
#define V_ASN1_SEQUENCE 16
#define V_ASN1_SET 17
#define V_ASN1_NUMERICSTRING 18
#define V_ASN1_PRINTABLESTRING 19
#define V_ASN1_T61STRING 20
#define V_ASN1_TELETEXSTRING 20
#define V_ASN1_VIDEOTEXSTRING 21
#define V_ASN1_IA5STRING 22
#define V_ASN1_UTCTIME 23
#define V_ASN1_GENERALIZEDTIME 24
#define V_ASN1_GRAPHICSTRING 25
#define V_ASN1_ISO64STRING 26
#define V_ASN1_VISIBLESTRING 26
#define V_ASN1_GENERALSTRING 27
#define V_ASN1_UNIVERSALSTRING 28
#define V_ASN1_BMPSTRING 30

#define V_ASN1_NEG 0x100
#define V_ASN1_NEG_INTEGER (V_ASN1_INTEGER | V_ASN1_NEG)
#define V_ASN1_NEG_ENUMERATED (V_ASN1_ENUMERATED | V_ASN1_NEG)

#define B_ASN1_NUMERICSTRING 0x0001
#define B_ASN1_PRINTABLESTRING 0x0002
#define B_ASN1_T61STRING 0x0004
#define B_ASN1_TELETEXSTRING 0x0004
#define B_ASN1_VIDEOTEXSTRING 0x0008
#define B_ASN1_IA5STRING 0x0010
#define B_ASN1_GRAPHICSTRING 0x0020
#define B_ASN1_ISO64STRING 0x0040
#define B_ASN1_VISIBLESTRING 0x0040
#define B_ASN1_GENERALSTRING 0x0080
#define B_ASN1_UNIVERSALSTRING 0x0100
#define B_ASN1_OCTET_STRING 0x0200
#define B_ASN1_BIT_STRING 0x0400
#define B_ASN1_BMPSTRING 0x0800
#define B_ASN1_UNKNOWN 0x1000
#define B_ASN1_UTF8STRING 0x2000
#define B_ASN1_UTCTIME 0x4000
#define B_ASN1_GENERALIZEDTIME 0x8000
#define B_ASN1_SEQUENCE 0x10000

OPENSSL_EXPORT unsigned long ASN1_tag2bit(int tag);

OPENSSL_EXPORT const char *ASN1_tag2str(int tag);

#if 0

SAMPLE *d2i_SAMPLE(SAMPLE **out, const uint8_t **inp, long len);

int i2d_SAMPLE(const SAMPLE *in, uint8_t **outp);

#endif

typedef void *d2i_of_void(void **, const unsigned char **, long);
typedef int i2d_of_void(const void *, unsigned char **);

#define DECLARE_ASN1_ITEM(name) extern OPENSSL_EXPORT const ASN1_ITEM name##_it;

#define ASN1_ITEM_rptr(name) (&(name##_it))

typedef const ASN1_ITEM ASN1_ITEM_EXP;

#define ASN1_ITEM_ref(name) (&(name##_it))

#define ASN1_ITEM_ptr(iptr) (iptr)

typedef struct ASN1_VALUE_st ASN1_VALUE;

OPENSSL_EXPORT ASN1_VALUE *ASN1_item_new(const ASN1_ITEM *it);

OPENSSL_EXPORT void ASN1_item_free(ASN1_VALUE *val, const ASN1_ITEM *it);

OPENSSL_EXPORT ASN1_VALUE *ASN1_item_d2i(ASN1_VALUE **out,
                                         const unsigned char **inp, long len,
                                         const ASN1_ITEM *it);

OPENSSL_EXPORT int ASN1_item_i2d(ASN1_VALUE *val, unsigned char **outp,
                                 const ASN1_ITEM *it);

OPENSSL_EXPORT void *ASN1_item_dup(const ASN1_ITEM *it, void *x);

OPENSSL_EXPORT void *ASN1_item_d2i_fp(const ASN1_ITEM *it, FILE *in, void *out);
OPENSSL_EXPORT void *ASN1_item_d2i_bio(const ASN1_ITEM *it, BIO *in, void *out);

OPENSSL_EXPORT int ASN1_item_i2d_fp(const ASN1_ITEM *it, FILE *out, void *in);
OPENSSL_EXPORT int ASN1_item_i2d_bio(const ASN1_ITEM *it, BIO *out, void *in);

OPENSSL_EXPORT void *ASN1_item_unpack(const ASN1_STRING *oct,
                                      const ASN1_ITEM *it);

OPENSSL_EXPORT ASN1_STRING *ASN1_item_pack(void *obj, const ASN1_ITEM *it,
                                           ASN1_STRING **out);

#define ASN1_BOOLEAN_FALSE 0

#define ASN1_BOOLEAN_TRUE 0xff

#define ASN1_BOOLEAN_NONE (-1)

OPENSSL_EXPORT ASN1_BOOLEAN d2i_ASN1_BOOLEAN(ASN1_BOOLEAN *out,
                                             const unsigned char **inp,
                                             long len);

OPENSSL_EXPORT int i2d_ASN1_BOOLEAN(ASN1_BOOLEAN a, unsigned char **outp);

DECLARE_ASN1_ITEM(ASN1_BOOLEAN)
DECLARE_ASN1_ITEM(ASN1_TBOOLEAN)
DECLARE_ASN1_ITEM(ASN1_FBOOLEAN)

struct asn1_string_st {
  int length;
  int type;
  unsigned char *data;
  long flags;
};

#define ASN1_STRING_FLAG_BITS_LEFT 0x08

OPENSSL_EXPORT ASN1_STRING *ASN1_STRING_type_new(int type);

OPENSSL_EXPORT ASN1_STRING *ASN1_STRING_new(void);

OPENSSL_EXPORT void ASN1_STRING_free(ASN1_STRING *str);

OPENSSL_EXPORT int ASN1_STRING_copy(ASN1_STRING *dst, const ASN1_STRING *str);

OPENSSL_EXPORT ASN1_STRING *ASN1_STRING_dup(const ASN1_STRING *str);

OPENSSL_EXPORT int ASN1_STRING_type(const ASN1_STRING *str);

OPENSSL_EXPORT const unsigned char *ASN1_STRING_get0_data(
    const ASN1_STRING *str);

OPENSSL_EXPORT unsigned char *ASN1_STRING_data(ASN1_STRING *str);

OPENSSL_EXPORT int ASN1_STRING_length(const ASN1_STRING *str);

OPENSSL_EXPORT int ASN1_STRING_cmp(const ASN1_STRING *a, const ASN1_STRING *b);

OPENSSL_EXPORT int ASN1_STRING_set(ASN1_STRING *str, const void *data,
                                   ossl_ssize_t len);

OPENSSL_EXPORT void ASN1_STRING_set0(ASN1_STRING *str, void *data, int len);

OPENSSL_EXPORT ASN1_BMPSTRING *ASN1_BMPSTRING_new(void);
OPENSSL_EXPORT ASN1_GENERALSTRING *ASN1_GENERALSTRING_new(void);
OPENSSL_EXPORT ASN1_IA5STRING *ASN1_IA5STRING_new(void);
OPENSSL_EXPORT ASN1_OCTET_STRING *ASN1_OCTET_STRING_new(void);
OPENSSL_EXPORT ASN1_PRINTABLESTRING *ASN1_PRINTABLESTRING_new(void);
OPENSSL_EXPORT ASN1_T61STRING *ASN1_T61STRING_new(void);
OPENSSL_EXPORT ASN1_UNIVERSALSTRING *ASN1_UNIVERSALSTRING_new(void);
OPENSSL_EXPORT ASN1_UTF8STRING *ASN1_UTF8STRING_new(void);
OPENSSL_EXPORT ASN1_VISIBLESTRING *ASN1_VISIBLESTRING_new(void);

OPENSSL_EXPORT void ASN1_BMPSTRING_free(ASN1_BMPSTRING *str);
OPENSSL_EXPORT void ASN1_GENERALSTRING_free(ASN1_GENERALSTRING *str);
OPENSSL_EXPORT void ASN1_IA5STRING_free(ASN1_IA5STRING *str);
OPENSSL_EXPORT void ASN1_OCTET_STRING_free(ASN1_OCTET_STRING *str);
OPENSSL_EXPORT void ASN1_PRINTABLESTRING_free(ASN1_PRINTABLESTRING *str);
OPENSSL_EXPORT void ASN1_T61STRING_free(ASN1_T61STRING *str);
OPENSSL_EXPORT void ASN1_UNIVERSALSTRING_free(ASN1_UNIVERSALSTRING *str);
OPENSSL_EXPORT void ASN1_UTF8STRING_free(ASN1_UTF8STRING *str);
OPENSSL_EXPORT void ASN1_VISIBLESTRING_free(ASN1_VISIBLESTRING *str);

OPENSSL_EXPORT ASN1_BMPSTRING *d2i_ASN1_BMPSTRING(ASN1_BMPSTRING **out,
                                                  const uint8_t **inp,
                                                  long len);
OPENSSL_EXPORT ASN1_GENERALSTRING *d2i_ASN1_GENERALSTRING(
    ASN1_GENERALSTRING **out, const uint8_t **inp, long len);
OPENSSL_EXPORT ASN1_IA5STRING *d2i_ASN1_IA5STRING(ASN1_IA5STRING **out,
                                                  const uint8_t **inp,
                                                  long len);
OPENSSL_EXPORT ASN1_OCTET_STRING *d2i_ASN1_OCTET_STRING(ASN1_OCTET_STRING **out,
                                                        const uint8_t **inp,
                                                        long len);
OPENSSL_EXPORT ASN1_PRINTABLESTRING *d2i_ASN1_PRINTABLESTRING(
    ASN1_PRINTABLESTRING **out, const uint8_t **inp, long len);
OPENSSL_EXPORT ASN1_T61STRING *d2i_ASN1_T61STRING(ASN1_T61STRING **out,
                                                  const uint8_t **inp,
                                                  long len);
OPENSSL_EXPORT ASN1_UNIVERSALSTRING *d2i_ASN1_UNIVERSALSTRING(
    ASN1_UNIVERSALSTRING **out, const uint8_t **inp, long len);
OPENSSL_EXPORT ASN1_UTF8STRING *d2i_ASN1_UTF8STRING(ASN1_UTF8STRING **out,
                                                    const uint8_t **inp,
                                                    long len);
OPENSSL_EXPORT ASN1_VISIBLESTRING *d2i_ASN1_VISIBLESTRING(
    ASN1_VISIBLESTRING **out, const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_ASN1_BMPSTRING(const ASN1_BMPSTRING *in, uint8_t **outp);
OPENSSL_EXPORT int i2d_ASN1_GENERALSTRING(const ASN1_GENERALSTRING *in,
                                          uint8_t **outp);
OPENSSL_EXPORT int i2d_ASN1_IA5STRING(const ASN1_IA5STRING *in, uint8_t **outp);
OPENSSL_EXPORT int i2d_ASN1_OCTET_STRING(const ASN1_OCTET_STRING *in,
                                         uint8_t **outp);
OPENSSL_EXPORT int i2d_ASN1_PRINTABLESTRING(const ASN1_PRINTABLESTRING *in,
                                            uint8_t **outp);
OPENSSL_EXPORT int i2d_ASN1_T61STRING(const ASN1_T61STRING *in, uint8_t **outp);
OPENSSL_EXPORT int i2d_ASN1_UNIVERSALSTRING(const ASN1_UNIVERSALSTRING *in,
                                            uint8_t **outp);
OPENSSL_EXPORT int i2d_ASN1_UTF8STRING(const ASN1_UTF8STRING *in,
                                       uint8_t **outp);
OPENSSL_EXPORT int i2d_ASN1_VISIBLESTRING(const ASN1_VISIBLESTRING *in,
                                          uint8_t **outp);

DECLARE_ASN1_ITEM(ASN1_BMPSTRING)
DECLARE_ASN1_ITEM(ASN1_GENERALSTRING)
DECLARE_ASN1_ITEM(ASN1_IA5STRING)
DECLARE_ASN1_ITEM(ASN1_OCTET_STRING)
DECLARE_ASN1_ITEM(ASN1_PRINTABLESTRING)
DECLARE_ASN1_ITEM(ASN1_T61STRING)
DECLARE_ASN1_ITEM(ASN1_UNIVERSALSTRING)
DECLARE_ASN1_ITEM(ASN1_UTF8STRING)
DECLARE_ASN1_ITEM(ASN1_VISIBLESTRING)

OPENSSL_EXPORT ASN1_OCTET_STRING *ASN1_OCTET_STRING_dup(
    const ASN1_OCTET_STRING *a);

OPENSSL_EXPORT int ASN1_OCTET_STRING_cmp(const ASN1_OCTET_STRING *a,
                                         const ASN1_OCTET_STRING *b);

OPENSSL_EXPORT int ASN1_OCTET_STRING_set(ASN1_OCTET_STRING *str,
                                         const unsigned char *data, int len);

OPENSSL_EXPORT int ASN1_STRING_to_UTF8(unsigned char **out,
                                       const ASN1_STRING *in);

#define MBSTRING_FLAG 0x1000
#define MBSTRING_UTF8 (MBSTRING_FLAG)
#define MBSTRING_ASC (MBSTRING_FLAG | 1)
#define MBSTRING_BMP (MBSTRING_FLAG | 2)
#define MBSTRING_UNIV (MBSTRING_FLAG | 4)

#define DIRSTRING_TYPE                                            \
  (B_ASN1_PRINTABLESTRING | B_ASN1_T61STRING | B_ASN1_BMPSTRING | \
   B_ASN1_UTF8STRING)

#define PKCS9STRING_TYPE (DIRSTRING_TYPE | B_ASN1_IA5STRING)

OPENSSL_EXPORT int ASN1_mbstring_copy(ASN1_STRING **out, const uint8_t *in,
                                      ossl_ssize_t len, int inform,
                                      unsigned long mask);

OPENSSL_EXPORT int ASN1_mbstring_ncopy(ASN1_STRING **out, const uint8_t *in,
                                       ossl_ssize_t len, int inform,
                                       unsigned long mask, ossl_ssize_t minsize,
                                       ossl_ssize_t maxsize);

OPENSSL_EXPORT ASN1_STRING *ASN1_STRING_set_by_NID(ASN1_STRING **out,
                                                   const unsigned char *in,
                                                   ossl_ssize_t len, int inform,
                                                   int nid);

#define STABLE_NO_MASK 0x02

OPENSSL_EXPORT int ASN1_STRING_TABLE_add(int nid, long minsize, long maxsize,
                                         unsigned long mask,
                                         unsigned long flags);

#define B_ASN1_DIRECTORYSTRING                                        \
  (B_ASN1_PRINTABLESTRING | B_ASN1_TELETEXSTRING | B_ASN1_BMPSTRING | \
   B_ASN1_UNIVERSALSTRING | B_ASN1_UTF8STRING)

OPENSSL_EXPORT ASN1_STRING *DIRECTORYSTRING_new(void);

OPENSSL_EXPORT void DIRECTORYSTRING_free(ASN1_STRING *str);

OPENSSL_EXPORT ASN1_STRING *d2i_DIRECTORYSTRING(ASN1_STRING **out,
                                                const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_DIRECTORYSTRING(const ASN1_STRING *in, uint8_t **outp);

DECLARE_ASN1_ITEM(DIRECTORYSTRING)

#define B_ASN1_DISPLAYTEXT                                      \
  (B_ASN1_IA5STRING | B_ASN1_VISIBLESTRING | B_ASN1_BMPSTRING | \
   B_ASN1_UTF8STRING)

OPENSSL_EXPORT ASN1_STRING *DISPLAYTEXT_new(void);

OPENSSL_EXPORT void DISPLAYTEXT_free(ASN1_STRING *str);

OPENSSL_EXPORT ASN1_STRING *d2i_DISPLAYTEXT(ASN1_STRING **out,
                                            const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_DISPLAYTEXT(const ASN1_STRING *in, uint8_t **outp);

DECLARE_ASN1_ITEM(DISPLAYTEXT)

OPENSSL_EXPORT ASN1_BIT_STRING *ASN1_BIT_STRING_new(void);

OPENSSL_EXPORT void ASN1_BIT_STRING_free(ASN1_BIT_STRING *str);

OPENSSL_EXPORT ASN1_BIT_STRING *d2i_ASN1_BIT_STRING(ASN1_BIT_STRING **out,
                                                    const uint8_t **inp,
                                                    long len);

OPENSSL_EXPORT int i2d_ASN1_BIT_STRING(const ASN1_BIT_STRING *in,
                                       uint8_t **outp);

OPENSSL_EXPORT ASN1_BIT_STRING *c2i_ASN1_BIT_STRING(ASN1_BIT_STRING **out,
                                                    const uint8_t **inp,
                                                    long len);

OPENSSL_EXPORT int i2c_ASN1_BIT_STRING(const ASN1_BIT_STRING *in,
                                       uint8_t **outp);

DECLARE_ASN1_ITEM(ASN1_BIT_STRING)

OPENSSL_EXPORT int ASN1_BIT_STRING_num_bytes(const ASN1_BIT_STRING *str,
                                             size_t *out);

OPENSSL_EXPORT int ASN1_BIT_STRING_set(ASN1_BIT_STRING *str,
                                       const unsigned char *d,
                                       ossl_ssize_t length);

OPENSSL_EXPORT int ASN1_BIT_STRING_set_bit(ASN1_BIT_STRING *str, int n,
                                           int value);

OPENSSL_EXPORT int ASN1_BIT_STRING_get_bit(const ASN1_BIT_STRING *str, int n);

OPENSSL_EXPORT int ASN1_BIT_STRING_check(const ASN1_BIT_STRING *str,
                                         const unsigned char *flags,
                                         int flags_len);

DEFINE_STACK_OF(ASN1_INTEGER)

OPENSSL_EXPORT ASN1_INTEGER *ASN1_INTEGER_new(void);

OPENSSL_EXPORT void ASN1_INTEGER_free(ASN1_INTEGER *str);

OPENSSL_EXPORT ASN1_INTEGER *ASN1_INTEGER_dup(const ASN1_INTEGER *x);

OPENSSL_EXPORT ASN1_INTEGER *d2i_ASN1_INTEGER(ASN1_INTEGER **out,
                                              const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_ASN1_INTEGER(const ASN1_INTEGER *in, uint8_t **outp);

OPENSSL_EXPORT ASN1_INTEGER *c2i_ASN1_INTEGER(ASN1_INTEGER **in,
                                              const uint8_t **outp, long len);

OPENSSL_EXPORT int i2c_ASN1_INTEGER(const ASN1_INTEGER *in, uint8_t **outp);

DECLARE_ASN1_ITEM(ASN1_INTEGER)

OPENSSL_EXPORT int ASN1_INTEGER_set_uint64(ASN1_INTEGER *out, uint64_t v);

OPENSSL_EXPORT int ASN1_INTEGER_set_int64(ASN1_INTEGER *out, int64_t v);

OPENSSL_EXPORT int ASN1_INTEGER_get_uint64(uint64_t *out,
                                           const ASN1_INTEGER *a);

OPENSSL_EXPORT int ASN1_INTEGER_get_int64(int64_t *out, const ASN1_INTEGER *a);

OPENSSL_EXPORT ASN1_INTEGER *BN_to_ASN1_INTEGER(const BIGNUM *bn,
                                                ASN1_INTEGER *ai);

OPENSSL_EXPORT BIGNUM *ASN1_INTEGER_to_BN(const ASN1_INTEGER *ai, BIGNUM *bn);

OPENSSL_EXPORT int ASN1_INTEGER_cmp(const ASN1_INTEGER *x,
                                    const ASN1_INTEGER *y);

OPENSSL_EXPORT ASN1_ENUMERATED *ASN1_ENUMERATED_new(void);

OPENSSL_EXPORT void ASN1_ENUMERATED_free(ASN1_ENUMERATED *str);

OPENSSL_EXPORT ASN1_ENUMERATED *d2i_ASN1_ENUMERATED(ASN1_ENUMERATED **out,
                                                    const uint8_t **inp,
                                                    long len);

OPENSSL_EXPORT int i2d_ASN1_ENUMERATED(const ASN1_ENUMERATED *in,
                                       uint8_t **outp);

DECLARE_ASN1_ITEM(ASN1_ENUMERATED)

OPENSSL_EXPORT int ASN1_ENUMERATED_set_uint64(ASN1_ENUMERATED *out, uint64_t v);

OPENSSL_EXPORT int ASN1_ENUMERATED_set_int64(ASN1_ENUMERATED *out, int64_t v);

OPENSSL_EXPORT int ASN1_ENUMERATED_get_uint64(uint64_t *out,
                                              const ASN1_ENUMERATED *a);

OPENSSL_EXPORT int ASN1_ENUMERATED_get_int64(int64_t *out,
                                             const ASN1_ENUMERATED *a);

OPENSSL_EXPORT ASN1_ENUMERATED *BN_to_ASN1_ENUMERATED(const BIGNUM *bn,
                                                      ASN1_ENUMERATED *ai);

OPENSSL_EXPORT BIGNUM *ASN1_ENUMERATED_to_BN(const ASN1_ENUMERATED *ai,
                                             BIGNUM *bn);

OPENSSL_EXPORT ASN1_UTCTIME *ASN1_UTCTIME_new(void);

OPENSSL_EXPORT void ASN1_UTCTIME_free(ASN1_UTCTIME *str);

OPENSSL_EXPORT ASN1_UTCTIME *d2i_ASN1_UTCTIME(ASN1_UTCTIME **out,
                                              const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_ASN1_UTCTIME(const ASN1_UTCTIME *in, uint8_t **outp);

DECLARE_ASN1_ITEM(ASN1_UTCTIME)

OPENSSL_EXPORT int ASN1_UTCTIME_check(const ASN1_UTCTIME *a);

OPENSSL_EXPORT ASN1_UTCTIME *ASN1_UTCTIME_set(ASN1_UTCTIME *s,
                                              int64_t posix_time);

OPENSSL_EXPORT ASN1_UTCTIME *ASN1_UTCTIME_adj(ASN1_UTCTIME *s,
                                              int64_t posix_time,
                                              int offset_day, long offset_sec);

OPENSSL_EXPORT int ASN1_UTCTIME_set_string(ASN1_UTCTIME *s, const char *str);

OPENSSL_EXPORT ASN1_GENERALIZEDTIME *ASN1_GENERALIZEDTIME_new(void);

OPENSSL_EXPORT void ASN1_GENERALIZEDTIME_free(ASN1_GENERALIZEDTIME *str);

OPENSSL_EXPORT ASN1_GENERALIZEDTIME *d2i_ASN1_GENERALIZEDTIME(
    ASN1_GENERALIZEDTIME **out, const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_ASN1_GENERALIZEDTIME(const ASN1_GENERALIZEDTIME *in,
                                            uint8_t **outp);

DECLARE_ASN1_ITEM(ASN1_GENERALIZEDTIME)

OPENSSL_EXPORT int ASN1_GENERALIZEDTIME_check(const ASN1_GENERALIZEDTIME *a);

OPENSSL_EXPORT ASN1_GENERALIZEDTIME *ASN1_GENERALIZEDTIME_set(
    ASN1_GENERALIZEDTIME *s, int64_t posix_time);

OPENSSL_EXPORT ASN1_GENERALIZEDTIME *ASN1_GENERALIZEDTIME_adj(
    ASN1_GENERALIZEDTIME *s, int64_t posix_time, int offset_day,
    long offset_sec);

OPENSSL_EXPORT int ASN1_GENERALIZEDTIME_set_string(ASN1_GENERALIZEDTIME *s,
                                                   const char *str);

#define B_ASN1_TIME (B_ASN1_UTCTIME | B_ASN1_GENERALIZEDTIME)

OPENSSL_EXPORT ASN1_TIME *ASN1_TIME_new(void);

OPENSSL_EXPORT void ASN1_TIME_free(ASN1_TIME *str);

OPENSSL_EXPORT ASN1_TIME *d2i_ASN1_TIME(ASN1_TIME **out, const uint8_t **inp,
                                        long len);

OPENSSL_EXPORT int i2d_ASN1_TIME(const ASN1_TIME *in, uint8_t **outp);

DECLARE_ASN1_ITEM(ASN1_TIME)

OPENSSL_EXPORT int ASN1_TIME_diff(int *out_days, int *out_seconds,
                                  const ASN1_TIME *from, const ASN1_TIME *to);

OPENSSL_EXPORT ASN1_TIME *ASN1_TIME_set_posix(ASN1_TIME *s, int64_t posix_time);

OPENSSL_EXPORT ASN1_TIME *ASN1_TIME_set(ASN1_TIME *s, time_t time);

OPENSSL_EXPORT ASN1_TIME *ASN1_TIME_adj(ASN1_TIME *s, int64_t posix_time,
                                        int offset_day, long offset_sec);

OPENSSL_EXPORT int ASN1_TIME_check(const ASN1_TIME *t);

OPENSSL_EXPORT ASN1_GENERALIZEDTIME *ASN1_TIME_to_generalizedtime(
    const ASN1_TIME *t, ASN1_GENERALIZEDTIME **out);

OPENSSL_EXPORT int ASN1_TIME_set_string(ASN1_TIME *s, const char *str);

OPENSSL_EXPORT int ASN1_TIME_set_string_X509(ASN1_TIME *s, const char *str);

OPENSSL_EXPORT int ASN1_TIME_to_time_t(const ASN1_TIME *t, time_t *out);

OPENSSL_EXPORT int ASN1_TIME_to_posix(const ASN1_TIME *t, int64_t *out);

OPENSSL_EXPORT int ASN1_TIME_to_posix_nonstandard(
    const ASN1_TIME *t, int64_t *out);

OPENSSL_EXPORT ASN1_NULL *ASN1_NULL_new(void);

OPENSSL_EXPORT void ASN1_NULL_free(ASN1_NULL *null);

OPENSSL_EXPORT ASN1_NULL *d2i_ASN1_NULL(ASN1_NULL **out, const uint8_t **inp,
                                        long len);

OPENSSL_EXPORT int i2d_ASN1_NULL(const ASN1_NULL *in, uint8_t **outp);

DECLARE_ASN1_ITEM(ASN1_NULL)

DEFINE_STACK_OF(ASN1_OBJECT)

OPENSSL_EXPORT ASN1_OBJECT *ASN1_OBJECT_create(int nid, const uint8_t *data,
                                               size_t len, const char *sn,
                                               const char *ln);

OPENSSL_EXPORT void ASN1_OBJECT_free(ASN1_OBJECT *a);

OPENSSL_EXPORT ASN1_OBJECT *d2i_ASN1_OBJECT(ASN1_OBJECT **out,
                                            const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_ASN1_OBJECT(const ASN1_OBJECT *in, uint8_t **outp);

OPENSSL_EXPORT ASN1_OBJECT *c2i_ASN1_OBJECT(ASN1_OBJECT **out,
                                            const uint8_t **inp, long len);

DECLARE_ASN1_ITEM(ASN1_OBJECT)

struct asn1_type_st {
  int type;
  union {
    char *ptr;
    ASN1_BOOLEAN boolean;
    ASN1_STRING *asn1_string;
    ASN1_OBJECT *object;
    ASN1_INTEGER *integer;
    ASN1_ENUMERATED *enumerated;
    ASN1_BIT_STRING *bit_string;
    ASN1_OCTET_STRING *octet_string;
    ASN1_PRINTABLESTRING *printablestring;
    ASN1_T61STRING *t61string;
    ASN1_IA5STRING *ia5string;
    ASN1_GENERALSTRING *generalstring;
    ASN1_BMPSTRING *bmpstring;
    ASN1_UNIVERSALSTRING *universalstring;
    ASN1_UTCTIME *utctime;
    ASN1_GENERALIZEDTIME *generalizedtime;
    ASN1_VISIBLESTRING *visiblestring;
    ASN1_UTF8STRING *utf8string;

    ASN1_STRING *set;
    ASN1_STRING *sequence;
    ASN1_VALUE *asn1_value;
  } value;
};

DEFINE_STACK_OF(ASN1_TYPE)

OPENSSL_EXPORT ASN1_TYPE *ASN1_TYPE_new(void);

OPENSSL_EXPORT void ASN1_TYPE_free(ASN1_TYPE *a);

OPENSSL_EXPORT ASN1_TYPE *d2i_ASN1_TYPE(ASN1_TYPE **out, const uint8_t **inp,
                                        long len);

OPENSSL_EXPORT int i2d_ASN1_TYPE(const ASN1_TYPE *in, uint8_t **outp);

DECLARE_ASN1_ITEM(ASN1_ANY)

OPENSSL_EXPORT int ASN1_TYPE_get(const ASN1_TYPE *a);

OPENSSL_EXPORT void ASN1_TYPE_set(ASN1_TYPE *a, int type, void *value);

OPENSSL_EXPORT int ASN1_TYPE_set1(ASN1_TYPE *a, int type, const void *value);

OPENSSL_EXPORT int ASN1_TYPE_cmp(const ASN1_TYPE *a, const ASN1_TYPE *b);

typedef STACK_OF(ASN1_TYPE) ASN1_SEQUENCE_ANY;

OPENSSL_EXPORT ASN1_SEQUENCE_ANY *d2i_ASN1_SEQUENCE_ANY(ASN1_SEQUENCE_ANY **out,
                                                        const uint8_t **inp,
                                                        long len);

OPENSSL_EXPORT int i2d_ASN1_SEQUENCE_ANY(const ASN1_SEQUENCE_ANY *in,
                                         uint8_t **outp);

OPENSSL_EXPORT ASN1_SEQUENCE_ANY *d2i_ASN1_SET_ANY(ASN1_SEQUENCE_ANY **out,
                                                   const uint8_t **inp,
                                                   long len);

OPENSSL_EXPORT int i2d_ASN1_SET_ANY(const ASN1_SEQUENCE_ANY *in,
                                    uint8_t **outp);

OPENSSL_EXPORT int ASN1_UTCTIME_print(BIO *out, const ASN1_UTCTIME *a);

OPENSSL_EXPORT int ASN1_GENERALIZEDTIME_print(BIO *out,
                                              const ASN1_GENERALIZEDTIME *a);

OPENSSL_EXPORT int ASN1_TIME_print(BIO *out, const ASN1_TIME *a);

OPENSSL_EXPORT int ASN1_STRING_print(BIO *out, const ASN1_STRING *str);

#define ASN1_STRFLGS_ESC_2253 1ul

#define ASN1_STRFLGS_ESC_CTRL 2ul

#define ASN1_STRFLGS_ESC_MSB 4ul

#define ASN1_STRFLGS_ESC_QUOTE 8ul

#define ASN1_STRFLGS_UTF8_CONVERT 0x10ul

#define ASN1_STRFLGS_IGNORE_TYPE 0x20ul

#define ASN1_STRFLGS_SHOW_TYPE 0x40ul

#define ASN1_STRFLGS_DUMP_ALL 0x80ul

#define ASN1_STRFLGS_DUMP_UNKNOWN 0x100ul

#define ASN1_STRFLGS_DUMP_DER 0x200ul

#define ASN1_STRFLGS_RFC2253                                              \
  (ASN1_STRFLGS_ESC_2253 | ASN1_STRFLGS_ESC_CTRL | ASN1_STRFLGS_ESC_MSB | \
   ASN1_STRFLGS_UTF8_CONVERT | ASN1_STRFLGS_DUMP_UNKNOWN |                \
   ASN1_STRFLGS_DUMP_DER)

OPENSSL_EXPORT int ASN1_STRING_print_ex(BIO *out, const ASN1_STRING *str,
                                        unsigned long flags);

OPENSSL_EXPORT int ASN1_STRING_print_ex_fp(FILE *fp, const ASN1_STRING *str,
                                           unsigned long flags);

OPENSSL_EXPORT int i2a_ASN1_INTEGER(BIO *bp, const ASN1_INTEGER *a);

OPENSSL_EXPORT int i2a_ASN1_ENUMERATED(BIO *bp, const ASN1_ENUMERATED *a);

OPENSSL_EXPORT int i2a_ASN1_OBJECT(BIO *bp, const ASN1_OBJECT *a);

OPENSSL_EXPORT int i2a_ASN1_STRING(BIO *bp, const ASN1_STRING *a, int type);

OPENSSL_EXPORT int i2t_ASN1_OBJECT(char *buf, int buf_len,
                                   const ASN1_OBJECT *a);

OPENSSL_EXPORT int ASN1_get_object(const unsigned char **inp, long *out_length,
                                   int *out_tag, int *out_class, long max_len);

OPENSSL_EXPORT void ASN1_put_object(unsigned char **outp, int constructed,
                                    int length, int tag, int xclass);

OPENSSL_EXPORT int ASN1_put_eoc(unsigned char **outp);

OPENSSL_EXPORT int ASN1_object_size(int constructed, int length, int tag);

#define DECLARE_ASN1_FUNCTIONS(type) DECLARE_ASN1_FUNCTIONS_name(type, type)

#define DECLARE_ASN1_ALLOC_FUNCTIONS(type) \
  DECLARE_ASN1_ALLOC_FUNCTIONS_name(type, type)

#define DECLARE_ASN1_FUNCTIONS_name(type, name) \
  DECLARE_ASN1_ALLOC_FUNCTIONS_name(type, name) \
  DECLARE_ASN1_ENCODE_FUNCTIONS(type, name, name)

#define DECLARE_ASN1_FUNCTIONS_fname(type, itname, name) \
  DECLARE_ASN1_ALLOC_FUNCTIONS_name(type, name)          \
  DECLARE_ASN1_ENCODE_FUNCTIONS(type, itname, name)

#define DECLARE_ASN1_ENCODE_FUNCTIONS(type, itname, name)             \
  OPENSSL_EXPORT type *d2i_##name(type **a, const unsigned char **in, \
                                  long len);                          \
  OPENSSL_EXPORT int i2d_##name(type *a, unsigned char **out);        \
  DECLARE_ASN1_ITEM(itname)

#define DECLARE_ASN1_ENCODE_FUNCTIONS_const(type, name)               \
  OPENSSL_EXPORT type *d2i_##name(type **a, const unsigned char **in, \
                                  long len);                          \
  OPENSSL_EXPORT int i2d_##name(const type *a, unsigned char **out);  \
  DECLARE_ASN1_ITEM(name)

#define DECLARE_ASN1_FUNCTIONS_const(name) \
  DECLARE_ASN1_ALLOC_FUNCTIONS(name)       \
  DECLARE_ASN1_ENCODE_FUNCTIONS_const(name, name)

#define DECLARE_ASN1_ALLOC_FUNCTIONS_name(type, name) \
  OPENSSL_EXPORT type *name##_new(void);              \
  OPENSSL_EXPORT void name##_free(type *a);

OPENSSL_EXPORT void ASN1_STRING_set_default_mask(unsigned long mask);

OPENSSL_EXPORT int ASN1_STRING_set_default_mask_asc(const char *p);

OPENSSL_EXPORT unsigned long ASN1_STRING_get_default_mask(void);

OPENSSL_EXPORT void ASN1_STRING_TABLE_cleanup(void);

#define M_ASN1_STRING_length(x) ASN1_STRING_length(x)
#define M_ASN1_STRING_type(x) ASN1_STRING_type(x)
#define M_ASN1_STRING_data(x) ASN1_STRING_data(x)
#define M_ASN1_BIT_STRING_new() ASN1_BIT_STRING_new()
#define M_ASN1_BIT_STRING_free(a) ASN1_BIT_STRING_free(a)
#define M_ASN1_BIT_STRING_dup(a) ASN1_STRING_dup(a)
#define M_ASN1_BIT_STRING_cmp(a, b) ASN1_STRING_cmp(a, b)
#define M_ASN1_BIT_STRING_set(a, b, c) ASN1_BIT_STRING_set(a, b, c)
#define M_ASN1_INTEGER_new() ASN1_INTEGER_new()
#define M_ASN1_INTEGER_free(a) ASN1_INTEGER_free(a)
#define M_ASN1_INTEGER_dup(a) ASN1_INTEGER_dup(a)
#define M_ASN1_INTEGER_cmp(a, b) ASN1_INTEGER_cmp(a, b)
#define M_ASN1_ENUMERATED_new() ASN1_ENUMERATED_new()
#define M_ASN1_ENUMERATED_free(a) ASN1_ENUMERATED_free(a)
#define M_ASN1_ENUMERATED_dup(a) ASN1_STRING_dup(a)
#define M_ASN1_ENUMERATED_cmp(a, b) ASN1_STRING_cmp(a, b)
#define M_ASN1_OCTET_STRING_new() ASN1_OCTET_STRING_new()
#define M_ASN1_OCTET_STRING_free(a) ASN1_OCTET_STRING_free()
#define M_ASN1_OCTET_STRING_dup(a) ASN1_OCTET_STRING_dup(a)
#define M_ASN1_OCTET_STRING_cmp(a, b) ASN1_OCTET_STRING_cmp(a, b)
#define M_ASN1_OCTET_STRING_set(a, b, c) ASN1_OCTET_STRING_set(a, b, c)
#define M_ASN1_OCTET_STRING_print(a, b) ASN1_STRING_print(a, b)
#define M_ASN1_PRINTABLESTRING_new() ASN1_PRINTABLESTRING_new()
#define M_ASN1_PRINTABLESTRING_free(a) ASN1_PRINTABLESTRING_free(a)
#define M_ASN1_IA5STRING_new() ASN1_IA5STRING_new()
#define M_ASN1_IA5STRING_free(a) ASN1_IA5STRING_free(a)
#define M_ASN1_IA5STRING_dup(a) ASN1_STRING_dup(a)
#define M_ASN1_UTCTIME_new() ASN1_UTCTIME_new()
#define M_ASN1_UTCTIME_free(a) ASN1_UTCTIME_free(a)
#define M_ASN1_UTCTIME_dup(a) ASN1_STRING_dup(a)
#define M_ASN1_T61STRING_new() ASN1_T61STRING_new()
#define M_ASN1_T61STRING_free(a) ASN1_T61STRING_free(a)
#define M_ASN1_GENERALIZEDTIME_new() ASN1_GENERALIZEDTIME_new()
#define M_ASN1_GENERALIZEDTIME_free(a) ASN1_GENERALIZEDTIME_free(a)
#define M_ASN1_GENERALIZEDTIME_dup(a) ASN1_STRING_dup(a)
#define M_ASN1_GENERALSTRING_new() ASN1_GENERALSTRING_new()
#define M_ASN1_GENERALSTRING_free(a) ASN1_GENERALSTRING_free(a)
#define M_ASN1_UNIVERSALSTRING_new() ASN1_UNIVERSALSTRING_new()
#define M_ASN1_UNIVERSALSTRING_free(a) ASN1_UNIVERSALSTRING_free(a)
#define M_ASN1_BMPSTRING_new() ASN1_BMPSTRING_new()
#define M_ASN1_BMPSTRING_free(a) ASN1_BMPSTRING_free(a)
#define M_ASN1_VISIBLESTRING_new() ASN1_VISIBLESTRING_new()
#define M_ASN1_VISIBLESTRING_free(a) ASN1_VISIBLESTRING_free(a)
#define M_ASN1_UTF8STRING_new() ASN1_UTF8STRING_new()
#define M_ASN1_UTF8STRING_free(a) ASN1_UTF8STRING_free(a)

#define B_ASN1_PRINTABLE                                              \
  (B_ASN1_NUMERICSTRING | B_ASN1_PRINTABLESTRING | B_ASN1_T61STRING | \
   B_ASN1_IA5STRING | B_ASN1_BIT_STRING | B_ASN1_UNIVERSALSTRING |    \
   B_ASN1_BMPSTRING | B_ASN1_UTF8STRING | B_ASN1_SEQUENCE | B_ASN1_UNKNOWN)

OPENSSL_EXPORT ASN1_STRING *ASN1_PRINTABLE_new(void);

OPENSSL_EXPORT void ASN1_PRINTABLE_free(ASN1_STRING *str);

OPENSSL_EXPORT ASN1_STRING *d2i_ASN1_PRINTABLE(ASN1_STRING **out,
                                               const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_ASN1_PRINTABLE(const ASN1_STRING *in, uint8_t **outp);

DECLARE_ASN1_ITEM(ASN1_PRINTABLE)

OPENSSL_EXPORT int ASN1_INTEGER_set(ASN1_INTEGER *a, long v);

OPENSSL_EXPORT int ASN1_ENUMERATED_set(ASN1_ENUMERATED *a, long v);

OPENSSL_EXPORT long ASN1_INTEGER_get(const ASN1_INTEGER *a);

OPENSSL_EXPORT long ASN1_ENUMERATED_get(const ASN1_ENUMERATED *a);

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(ASN1_OBJECT, ASN1_OBJECT_free)
BORINGSSL_MAKE_DELETER(ASN1_STRING, ASN1_STRING_free)
BORINGSSL_MAKE_DELETER(ASN1_TYPE, ASN1_TYPE_free)

BSSL_NAMESPACE_END

}

#endif

#define ASN1_R_ASN1_LENGTH_MISMATCH 100
#define ASN1_R_AUX_ERROR 101
#define ASN1_R_BAD_GET_ASN1_OBJECT_CALL 102
#define ASN1_R_BAD_OBJECT_HEADER 103
#define ASN1_R_BMPSTRING_IS_WRONG_LENGTH 104
#define ASN1_R_BN_LIB 105
#define ASN1_R_BOOLEAN_IS_WRONG_LENGTH 106
#define ASN1_R_BUFFER_TOO_SMALL 107
#define ASN1_R_CONTEXT_NOT_INITIALISED 108
#define ASN1_R_DECODE_ERROR 109
#define ASN1_R_DEPTH_EXCEEDED 110
#define ASN1_R_DIGEST_AND_KEY_TYPE_NOT_SUPPORTED 111
#define ASN1_R_ENCODE_ERROR 112
#define ASN1_R_ERROR_GETTING_TIME 113
#define ASN1_R_EXPECTING_AN_ASN1_SEQUENCE 114
#define ASN1_R_EXPECTING_AN_INTEGER 115
#define ASN1_R_EXPECTING_AN_OBJECT 116
#define ASN1_R_EXPECTING_A_BOOLEAN 117
#define ASN1_R_EXPECTING_A_TIME 118
#define ASN1_R_EXPLICIT_LENGTH_MISMATCH 119
#define ASN1_R_EXPLICIT_TAG_NOT_CONSTRUCTED 120
#define ASN1_R_FIELD_MISSING 121
#define ASN1_R_FIRST_NUM_TOO_LARGE 122
#define ASN1_R_HEADER_TOO_LONG 123
#define ASN1_R_ILLEGAL_BITSTRING_FORMAT 124
#define ASN1_R_ILLEGAL_BOOLEAN 125
#define ASN1_R_ILLEGAL_CHARACTERS 126
#define ASN1_R_ILLEGAL_FORMAT 127
#define ASN1_R_ILLEGAL_HEX 128
#define ASN1_R_ILLEGAL_IMPLICIT_TAG 129
#define ASN1_R_ILLEGAL_INTEGER 130
#define ASN1_R_ILLEGAL_NESTED_TAGGING 131
#define ASN1_R_ILLEGAL_NULL 132
#define ASN1_R_ILLEGAL_NULL_VALUE 133
#define ASN1_R_ILLEGAL_OBJECT 134
#define ASN1_R_ILLEGAL_OPTIONAL_ANY 135
#define ASN1_R_ILLEGAL_OPTIONS_ON_ITEM_TEMPLATE 136
#define ASN1_R_ILLEGAL_TAGGED_ANY 137
#define ASN1_R_ILLEGAL_TIME_VALUE 138
#define ASN1_R_INTEGER_NOT_ASCII_FORMAT 139
#define ASN1_R_INTEGER_TOO_LARGE_FOR_LONG 140
#define ASN1_R_INVALID_BIT_STRING_BITS_LEFT 141
#define ASN1_R_INVALID_BMPSTRING 142
#define ASN1_R_INVALID_DIGIT 143
#define ASN1_R_INVALID_MODIFIER 144
#define ASN1_R_INVALID_NUMBER 145
#define ASN1_R_INVALID_OBJECT_ENCODING 146
#define ASN1_R_INVALID_SEPARATOR 147
#define ASN1_R_INVALID_TIME_FORMAT 148
#define ASN1_R_INVALID_UNIVERSALSTRING 149
#define ASN1_R_INVALID_UTF8STRING 150
#define ASN1_R_LIST_ERROR 151
#define ASN1_R_MISSING_ASN1_EOS 152
#define ASN1_R_MISSING_EOC 153
#define ASN1_R_MISSING_SECOND_NUMBER 154
#define ASN1_R_MISSING_VALUE 155
#define ASN1_R_MSTRING_NOT_UNIVERSAL 156
#define ASN1_R_MSTRING_WRONG_TAG 157
#define ASN1_R_NESTED_ASN1_ERROR 158
#define ASN1_R_NESTED_ASN1_STRING 159
#define ASN1_R_NON_HEX_CHARACTERS 160
#define ASN1_R_NOT_ASCII_FORMAT 161
#define ASN1_R_NOT_ENOUGH_DATA 162
#define ASN1_R_NO_MATCHING_CHOICE_TYPE 163
#define ASN1_R_NULL_IS_WRONG_LENGTH 164
#define ASN1_R_OBJECT_NOT_ASCII_FORMAT 165
#define ASN1_R_ODD_NUMBER_OF_CHARS 166
#define ASN1_R_SECOND_NUMBER_TOO_LARGE 167
#define ASN1_R_SEQUENCE_LENGTH_MISMATCH 168
#define ASN1_R_SEQUENCE_NOT_CONSTRUCTED 169
#define ASN1_R_SEQUENCE_OR_SET_NEEDS_CONFIG 170
#define ASN1_R_SHORT_LINE 171
#define ASN1_R_STREAMING_NOT_SUPPORTED 172
#define ASN1_R_STRING_TOO_LONG 173
#define ASN1_R_STRING_TOO_SHORT 174
#define ASN1_R_TAG_VALUE_TOO_HIGH 175
#define ASN1_R_TIME_NOT_ASCII_FORMAT 176
#define ASN1_R_TOO_LONG 177
#define ASN1_R_TYPE_NOT_CONSTRUCTED 178
#define ASN1_R_TYPE_NOT_PRIMITIVE 179
#define ASN1_R_UNEXPECTED_EOC 180
#define ASN1_R_UNIVERSALSTRING_IS_WRONG_LENGTH 181
#define ASN1_R_UNKNOWN_FORMAT 182
#define ASN1_R_UNKNOWN_MESSAGE_DIGEST_ALGORITHM 183
#define ASN1_R_UNKNOWN_SIGNATURE_ALGORITHM 184
#define ASN1_R_UNKNOWN_TAG 185
#define ASN1_R_UNSUPPORTED_ANY_DEFINED_BY_TYPE 186
#define ASN1_R_UNSUPPORTED_PUBLIC_KEY_TYPE 187
#define ASN1_R_UNSUPPORTED_TYPE 188
#define ASN1_R_WRONG_PUBLIC_KEY_TYPE 189
#define ASN1_R_WRONG_TAG 190
#define ASN1_R_WRONG_TYPE 191
#define ASN1_R_NESTED_TOO_DEEP 192
#define ASN1_R_BAD_TEMPLATE 193
#define ASN1_R_INVALID_BIT_STRING_PADDING 194
#define ASN1_R_WRONG_INTEGER_TYPE 195
#define ASN1_R_INVALID_INTEGER 196

#endif
