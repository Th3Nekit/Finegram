// Copyright 2000-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <openssl/asn1.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <openssl/asn1t.h>
#include <openssl/mem.h>

#include "../internal.h"
#include "internal.h"

static int asn1_item_ex_i2d_opt(ASN1_VALUE **pval, unsigned char **out,
                                const ASN1_ITEM *it, int tag, int aclass,
                                int optional);
static int asn1_i2d_ex_primitive(ASN1_VALUE **pval, unsigned char **out,
                                 const ASN1_ITEM *it, int tag, int aclass,
                                 int optional);
static int asn1_ex_i2c(ASN1_VALUE **pval, unsigned char *cont, int *out_omit,
                       int *putype, const ASN1_ITEM *it);
static int asn1_set_seq_out(STACK_OF(ASN1_VALUE) *sk, unsigned char **out,
                            int skcontlen, const ASN1_ITEM *item, int do_sort);
static int asn1_template_ex_i2d(ASN1_VALUE **pval, unsigned char **out,
                                const ASN1_TEMPLATE *tt, int tag, int aclass,
                                int optional);

int ASN1_item_i2d(ASN1_VALUE *val, unsigned char **out, const ASN1_ITEM *it) {
  if (out && !*out) {
    unsigned char *p, *buf;
    int len = ASN1_item_ex_i2d(&val, NULL, it,         -1,            0);
    if (len <= 0) {
      return len;
    }
    buf = reinterpret_cast<uint8_t *>(OPENSSL_malloc(len));
    if (!buf) {
      return -1;
    }
    p = buf;
    int len2 = ASN1_item_ex_i2d(&val, &p, it,         -1,            0);
    if (len2 <= 0) {
      OPENSSL_free(buf);
      return len2;
    }
    assert(len == len2);
    *out = buf;
    return len;
  }

  return ASN1_item_ex_i2d(&val, out, it,         -1,            0);
}

int ASN1_item_ex_i2d(ASN1_VALUE **pval, unsigned char **out,
                     const ASN1_ITEM *it, int tag, int aclass) {
  int ret = asn1_item_ex_i2d_opt(pval, out, it, tag, aclass,              0);
  assert(ret != 0);
  return ret;
}

int asn1_item_ex_i2d_opt(ASN1_VALUE **pval, unsigned char **out,
                         const ASN1_ITEM *it, int tag, int aclass,
                         int optional) {
  const ASN1_TEMPLATE *tt = NULL;
  int i, seqcontlen, seqlen;

  assert((aclass & ASN1_TFLG_TAG_CLASS) == aclass);

  assert(tag != -1 || aclass == 0);

  if ((it->itype != ASN1_ITYPE_PRIMITIVE) && !*pval) {
    if (optional) {
      return 0;
    }
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_MISSING_VALUE);
    return -1;
  }

  switch (it->itype) {
    case ASN1_ITYPE_PRIMITIVE:
      if (it->templates) {

        if (it->templates->flags & ASN1_TFLG_OPTIONAL) {
          OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
          return -1;
        }
        return asn1_template_ex_i2d(pval, out, it->templates, tag, aclass,
                                    optional);
      }
      return asn1_i2d_ex_primitive(pval, out, it, tag, aclass, optional);

    case ASN1_ITYPE_MSTRING:

      if (tag != -1) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
        return -1;
      }
      return asn1_i2d_ex_primitive(pval, out, it, -1, 0, optional);

    case ASN1_ITYPE_CHOICE: {

      if (tag != -1) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
        return -1;
      }
      i = asn1_get_choice_selector(pval, it);
      if (i < 0 || i >= it->tcount) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_NO_MATCHING_CHOICE_TYPE);
        return -1;
      }
      const ASN1_TEMPLATE *chtt = it->templates + i;
      if (chtt->flags & ASN1_TFLG_OPTIONAL) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
        return -1;
      }
      ASN1_VALUE **pchval = asn1_get_field_ptr(pval, chtt);
      return asn1_template_ex_i2d(pchval, out, chtt, -1, 0,              0);
    }

    case ASN1_ITYPE_EXTERN: {

      if (tag != -1) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
        return -1;
      }
      const ASN1_EXTERN_FUNCS *ef =
          reinterpret_cast<const ASN1_EXTERN_FUNCS *>(it->funcs);
      int ret = ef->asn1_ex_i2d(pval, out, it);
      if (ret == 0) {

        OPENSSL_PUT_ERROR(ASN1, ERR_R_INTERNAL_ERROR);
        return -1;
      }
      return ret;
    }

    case ASN1_ITYPE_SEQUENCE: {
      i = asn1_enc_restore(&seqcontlen, out, pval, it);

      if (i < 0) {
        return -1;
      }

      if (i > 0) {
        return seqcontlen;
      }

      seqcontlen = 0;

      if (tag == -1) {
        tag = V_ASN1_SEQUENCE;
        aclass = V_ASN1_UNIVERSAL;
      }

      for (i = 0, tt = it->templates; i < it->tcount; tt++, i++) {
        const ASN1_TEMPLATE *seqtt;
        ASN1_VALUE **pseqval;
        int tmplen;
        seqtt = asn1_do_adb(pval, tt, 1);
        if (!seqtt) {
          return -1;
        }
        pseqval = asn1_get_field_ptr(pval, seqtt);
        tmplen =
            asn1_template_ex_i2d(pseqval, NULL, seqtt, -1, 0,              0);
        if (tmplen == -1 || (tmplen > INT_MAX - seqcontlen)) {
          return -1;
        }
        seqcontlen += tmplen;
      }

      seqlen = ASN1_object_size(                1, seqcontlen, tag);
      if (!out || seqlen == -1) {
        return seqlen;
      }

      ASN1_put_object(out,                 1, seqcontlen, tag, aclass);
      for (i = 0, tt = it->templates; i < it->tcount; tt++, i++) {
        const ASN1_TEMPLATE *seqtt;
        ASN1_VALUE **pseqval;
        seqtt = asn1_do_adb(pval, tt, 1);
        if (!seqtt) {
          return -1;
        }
        pseqval = asn1_get_field_ptr(pval, seqtt);
        if (asn1_template_ex_i2d(pseqval, out, seqtt, -1, 0,              0) <
            0) {
          return -1;
        }
      }
      return seqlen;
    }

    default:
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
      return -1;
  }
}

static int asn1_template_ex_i2d(ASN1_VALUE **pval, unsigned char **out,
                                const ASN1_TEMPLATE *tt, int tag, int iclass,
                                int optional) {
  int i, ret, ttag, tclass;
  size_t j;
  uint32_t flags = tt->flags;

  assert((iclass & ASN1_TFLG_TAG_CLASS) == iclass);

  assert(tag != -1 || iclass == 0);

  if (flags & ASN1_TFLG_TAG_MASK) {

    if (tag != -1) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
      return -1;
    }

    ttag = tt->tag;
    tclass = flags & ASN1_TFLG_TAG_CLASS;
  } else if (tag != -1) {

    ttag = tag;
    tclass = iclass & ASN1_TFLG_TAG_CLASS;
  } else {
    ttag = -1;
    tclass = 0;
  }

  assert(!optional || (flags & ASN1_TFLG_OPTIONAL) == 0);
  optional = optional || (flags & ASN1_TFLG_OPTIONAL) != 0;

  if (flags & ASN1_TFLG_SK_MASK) {

    STACK_OF(ASN1_VALUE) *sk = (STACK_OF(ASN1_VALUE) *)*pval;
    int isset, sktag, skaclass;
    int skcontlen, sklen;
    ASN1_VALUE *skitem;

    if (!*pval) {
      if (optional) {
        return 0;
      }
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_MISSING_VALUE);
      return -1;
    }

    if (flags & ASN1_TFLG_SET_OF) {
      isset = 1;

      assert((flags & ASN1_TFLG_SEQUENCE_OF) == 0);
    } else {
      isset = 0;
    }

    if ((ttag != -1) && !(flags & ASN1_TFLG_EXPTAG)) {
      sktag = ttag;
      skaclass = tclass;
    } else {
      skaclass = V_ASN1_UNIVERSAL;
      if (isset) {
        sktag = V_ASN1_SET;
      } else {
        sktag = V_ASN1_SEQUENCE;
      }
    }

    skcontlen = 0;
    for (j = 0; j < sk_ASN1_VALUE_num(sk); j++) {
      int tmplen;
      skitem = sk_ASN1_VALUE_value(sk, j);
      tmplen = ASN1_item_ex_i2d(&skitem, NULL, ASN1_ITEM_ptr(tt->item), -1, 0);
      if (tmplen == -1 || (skcontlen > INT_MAX - tmplen)) {
        return -1;
      }
      skcontlen += tmplen;
    }
    sklen = ASN1_object_size(                1, skcontlen, sktag);
    if (sklen == -1) {
      return -1;
    }

    if (flags & ASN1_TFLG_EXPTAG) {
      ret = ASN1_object_size(                1, sklen, ttag);
    } else {
      ret = sklen;
    }

    if (!out || ret == -1) {
      return ret;
    }

    if (flags & ASN1_TFLG_EXPTAG) {
      ASN1_put_object(out,                 1, sklen, ttag, tclass);
    }

    ASN1_put_object(out,                 1, skcontlen, sktag, skaclass);

    if (!asn1_set_seq_out(sk, out, skcontlen, ASN1_ITEM_ptr(tt->item), isset)) {
      return -1;
    }
    return ret;
  }

  if (flags & ASN1_TFLG_EXPTAG) {

    i = asn1_item_ex_i2d_opt(pval, NULL, ASN1_ITEM_ptr(tt->item), -1, 0,
                             optional);
    if (i <= 0) {
      return i;
    }

    ret = ASN1_object_size(                1, i, ttag);
    if (out && ret != -1) {

      ASN1_put_object(out,                 1, i, ttag, tclass);
      if (ASN1_item_ex_i2d(pval, out, ASN1_ITEM_ptr(tt->item), -1, 0) < 0) {
        return -1;
      }
    }
    return ret;
  }

  return asn1_item_ex_i2d_opt(pval, out, ASN1_ITEM_ptr(tt->item), ttag, tclass,
                              optional);
}

typedef struct {
  unsigned char *data;
  int length;
} DER_ENC;

static int der_cmp(const void *a, const void *b) {
  const DER_ENC *d1 = reinterpret_cast<const DER_ENC *>(a),
                *d2 = reinterpret_cast<const DER_ENC *>(b);
  int cmplen, i;
  cmplen = (d1->length < d2->length) ? d1->length : d2->length;
  i = OPENSSL_memcmp(d1->data, d2->data, cmplen);
  if (i) {
    return i;
  }
  return d1->length - d2->length;
}

static int asn1_set_seq_out(STACK_OF(ASN1_VALUE) *sk, unsigned char **out,
                            int skcontlen, const ASN1_ITEM *item, int do_sort) {

  if (!do_sort || sk_ASN1_VALUE_num(sk) < 2) {
    for (size_t i = 0; i < sk_ASN1_VALUE_num(sk); i++) {
      ASN1_VALUE *skitem = sk_ASN1_VALUE_value(sk, i);
      if (ASN1_item_ex_i2d(&skitem, out, item, -1, 0) < 0) {
        return 0;
      }
    }
    return 1;
  }

  int ret = 0;
  uint8_t *const buf = reinterpret_cast<uint8_t *>(OPENSSL_malloc(skcontlen));
  DER_ENC *encoded = reinterpret_cast<DER_ENC *>(
      OPENSSL_calloc(sk_ASN1_VALUE_num(sk), sizeof(*encoded)));
  uint8_t *p = buf;
  if (encoded == NULL || buf == NULL) {
    goto err;
  }

  for (size_t i = 0; i < sk_ASN1_VALUE_num(sk); i++) {
    ASN1_VALUE *skitem = sk_ASN1_VALUE_value(sk, i);
    encoded[i].data = p;
    encoded[i].length = ASN1_item_ex_i2d(&skitem, &p, item, -1, 0);
    if (encoded[i].length < 0) {
      goto err;
    }
    assert(p - buf <= skcontlen);
  }

  qsort(encoded, sk_ASN1_VALUE_num(sk), sizeof(*encoded), der_cmp);

  p = *out;
  for (size_t i = 0; i < sk_ASN1_VALUE_num(sk); i++) {
    OPENSSL_memcpy(p, encoded[i].data, encoded[i].length);
    p += encoded[i].length;
  }
  *out = p;

  ret = 1;

err:
  OPENSSL_free(encoded);
  OPENSSL_free(buf);
  return ret;
}

static int asn1_i2d_ex_primitive(ASN1_VALUE **pval, unsigned char **out,
                                 const ASN1_ITEM *it, int tag, int aclass,
                                 int optional) {

  int omit;
  int utype = it->utype;
  int len = asn1_ex_i2c(pval, NULL, &omit, &utype, it);
  if (len < 0) {
    return -1;
  }
  if (omit) {
    if (optional) {
      return 0;
    }
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_MISSING_VALUE);
    return -1;
  }

  int usetag =
      utype != V_ASN1_SEQUENCE && utype != V_ASN1_SET && utype != V_ASN1_OTHER;

  if (tag == -1) {
    tag = utype;
  }

  if (out) {
    if (usetag) {
      ASN1_put_object(out,                 0, len, tag, aclass);
    }
    int len2 = asn1_ex_i2c(pval, *out, &omit, &utype, it);
    if (len2 < 0) {
      return -1;
    }
    assert(len == len2);
    assert(!omit);
    *out += len;
  }

  if (usetag) {
    return ASN1_object_size(                0, len, tag);
  }
  return len;
}

static int asn1_ex_i2c(ASN1_VALUE **pval, unsigned char *cout, int *out_omit,
                       int *putype, const ASN1_ITEM *it) {
  ASN1_BOOLEAN *tbool = NULL;
  ASN1_STRING *strtmp;
  ASN1_OBJECT *otmp;
  int utype;
  const unsigned char *cont;
  unsigned char c;
  int len;

  assert(it->funcs == NULL);

  *out_omit = 0;

  if ((it->itype != ASN1_ITYPE_PRIMITIVE) || (it->utype != V_ASN1_BOOLEAN)) {
    if (!*pval) {
      *out_omit = 1;
      return 0;
    }
  }

  if (it->itype == ASN1_ITYPE_MSTRING) {

    strtmp = (ASN1_STRING *)*pval;
    utype = strtmp->type;
    if (utype < 0 && utype != V_ASN1_OTHER) {

      OPENSSL_PUT_ERROR(ASN1, ASN1_R_WRONG_TYPE);
      return -1;
    }

    if (utype == V_ASN1_NEG_INTEGER) {
      utype = V_ASN1_INTEGER;
    } else if (utype == V_ASN1_NEG_ENUMERATED) {
      utype = V_ASN1_ENUMERATED;
    }
    *putype = utype;
  } else if (it->utype == V_ASN1_ANY) {

    ASN1_TYPE *typ;
    typ = (ASN1_TYPE *)*pval;
    utype = typ->type;
    if (utype < 0 && utype != V_ASN1_OTHER) {

      OPENSSL_PUT_ERROR(ASN1, ASN1_R_WRONG_TYPE);
      return -1;
    }
    *putype = utype;
    pval = &typ->value.asn1_value;
  } else {
    utype = *putype;
  }

  switch (utype) {
    case V_ASN1_OBJECT:
      otmp = (ASN1_OBJECT *)*pval;
      cont = otmp->data;
      len = otmp->length;
      if (len == 0) {

        OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_OBJECT);
        return -1;
      }
      break;

    case V_ASN1_NULL:
      cont = NULL;
      len = 0;
      break;

    case V_ASN1_BOOLEAN:
      tbool = (ASN1_BOOLEAN *)pval;
      if (*tbool == ASN1_BOOLEAN_NONE) {
        *out_omit = 1;
        return 0;
      }
      if (it->utype != V_ASN1_ANY) {

        if ((*tbool && (it->size > 0)) || (!*tbool && !it->size)) {
          *out_omit = 1;
          return 0;
        }
      }
      c = *tbool ? 0xff : 0x00;
      cont = &c;
      len = 1;
      break;

    case V_ASN1_BIT_STRING: {
      int ret =
          i2c_ASN1_BIT_STRING((ASN1_BIT_STRING *)*pval, cout ? &cout : NULL);

      return ret <= 0 ? -1 : ret;
    }

    case V_ASN1_INTEGER:
    case V_ASN1_ENUMERATED: {

      int ret = i2c_ASN1_INTEGER((ASN1_INTEGER *)*pval, cout ? &cout : NULL);

      return ret <= 0 ? -1 : ret;
    }

    case V_ASN1_OCTET_STRING:
    case V_ASN1_NUMERICSTRING:
    case V_ASN1_PRINTABLESTRING:
    case V_ASN1_T61STRING:
    case V_ASN1_VIDEOTEXSTRING:
    case V_ASN1_IA5STRING:
    case V_ASN1_UTCTIME:
    case V_ASN1_GENERALIZEDTIME:
    case V_ASN1_GRAPHICSTRING:
    case V_ASN1_VISIBLESTRING:
    case V_ASN1_GENERALSTRING:
    case V_ASN1_UNIVERSALSTRING:
    case V_ASN1_BMPSTRING:
    case V_ASN1_UTF8STRING:
    case V_ASN1_SEQUENCE:
    case V_ASN1_SET:

    case V_ASN1_OTHER:

    default:

      strtmp = (ASN1_STRING *)*pval;
      cont = strtmp->data;
      len = strtmp->length;
      break;
  }
  if (cout && len) {
    OPENSSL_memcpy(cout, cont, len);
  }
  return len;
}
