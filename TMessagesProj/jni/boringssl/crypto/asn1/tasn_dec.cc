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
#include <openssl/asn1t.h>
#include <openssl/bytestring.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/pool.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include "../bytestring/internal.h"
#include "../internal.h"
#include "internal.h"

#define ASN1_MAX_CONSTRUCTED_NEST 30

static int asn1_check_tlen(long *olen, int *otag, unsigned char *oclass,
                           char *cst, const unsigned char **in, long len,
                           int exptag, int expclass, char opt);

static int asn1_template_ex_d2i(ASN1_VALUE **pval, const unsigned char **in,
                                long len, const ASN1_TEMPLATE *tt, char opt,
                                CRYPTO_BUFFER *buf, int depth);
static int asn1_template_noexp_d2i(ASN1_VALUE **val, const unsigned char **in,
                                   long len, const ASN1_TEMPLATE *tt, char opt,
                                   CRYPTO_BUFFER *buf, int depth);
static int asn1_ex_c2i(ASN1_VALUE **pval, const unsigned char *cont, long len,
                       int utype, const ASN1_ITEM *it);
static int asn1_d2i_ex_primitive(ASN1_VALUE **pval, const unsigned char **in,
                                 long len, const ASN1_ITEM *it, int tag,
                                 int aclass, char opt);
static int asn1_item_ex_d2i(ASN1_VALUE **pval, const unsigned char **in,
                            long len, const ASN1_ITEM *it, int tag, int aclass,
                            char opt, CRYPTO_BUFFER *buf, int depth);

static const unsigned long tag2bit[31] = {
    0,
    0,
    0,
    B_ASN1_BIT_STRING,
    B_ASN1_OCTET_STRING,
    0,
    0,
    B_ASN1_UNKNOWN,
    B_ASN1_UNKNOWN,
    B_ASN1_UNKNOWN,
    B_ASN1_UNKNOWN,
    B_ASN1_UNKNOWN,
    B_ASN1_UTF8STRING,
    B_ASN1_UNKNOWN,
    B_ASN1_UNKNOWN,
    B_ASN1_UNKNOWN,
    B_ASN1_SEQUENCE,
    0,
    B_ASN1_NUMERICSTRING,
    B_ASN1_PRINTABLESTRING,
    B_ASN1_T61STRING,
    B_ASN1_VIDEOTEXSTRING,
    B_ASN1_IA5STRING,
    B_ASN1_UTCTIME,
    B_ASN1_GENERALIZEDTIME,
    B_ASN1_GRAPHICSTRING,
    B_ASN1_ISO64STRING,
    B_ASN1_GENERALSTRING,
    B_ASN1_UNIVERSALSTRING,
    B_ASN1_UNKNOWN,
    B_ASN1_BMPSTRING,
};

unsigned long ASN1_tag2bit(int tag) {
  if (tag < 0 || tag > 30) {
    return 0;
  }
  return tag2bit[tag];
}

static int is_supported_universal_type(int tag, int aclass) {
  if (aclass != V_ASN1_UNIVERSAL) {
    return 0;
  }
  return tag == V_ASN1_OBJECT || tag == V_ASN1_NULL || tag == V_ASN1_BOOLEAN ||
         tag == V_ASN1_BIT_STRING || tag == V_ASN1_INTEGER ||
         tag == V_ASN1_ENUMERATED || tag == V_ASN1_OCTET_STRING ||
         tag == V_ASN1_NUMERICSTRING || tag == V_ASN1_PRINTABLESTRING ||
         tag == V_ASN1_T61STRING || tag == V_ASN1_VIDEOTEXSTRING ||
         tag == V_ASN1_IA5STRING || tag == V_ASN1_UTCTIME ||
         tag == V_ASN1_GENERALIZEDTIME || tag == V_ASN1_GRAPHICSTRING ||
         tag == V_ASN1_VISIBLESTRING || tag == V_ASN1_GENERALSTRING ||
         tag == V_ASN1_UNIVERSALSTRING || tag == V_ASN1_BMPSTRING ||
         tag == V_ASN1_UTF8STRING || tag == V_ASN1_SET ||
         tag == V_ASN1_SEQUENCE;
}

ASN1_VALUE *ASN1_item_d2i(ASN1_VALUE **pval, const unsigned char **in, long len,
                          const ASN1_ITEM *it) {
  ASN1_VALUE *ret = NULL;
  if (asn1_item_ex_d2i(&ret, in, len, it,         -1,            0,         0,
                               NULL,           0) <= 0) {

    ASN1_item_ex_free(&ret, it);
  }

  if (pval != NULL) {
    ASN1_item_ex_free(pval, it);
    *pval = ret;
  }
  return ret;
}

static int asn1_item_ex_d2i(ASN1_VALUE **pval, const unsigned char **in,
                            long len, const ASN1_ITEM *it, int tag, int aclass,
                            char opt, CRYPTO_BUFFER *buf, int depth) {
  const ASN1_TEMPLATE *tt, *errtt = NULL;
  const unsigned char *p = NULL, *q;
  unsigned char oclass;
  char cst, isopt;
  int i;
  int otag;
  int ret = 0;
  ASN1_VALUE **pchptr;
  if (!pval) {
    return 0;
  }

  if (buf != NULL) {
    assert(CRYPTO_BUFFER_data(buf) <= *in &&
           *in + len <= CRYPTO_BUFFER_data(buf) + CRYPTO_BUFFER_len(buf));
  }

  if (len > INT_MAX / 2) {
    len = INT_MAX / 2;
  }

  if (++depth > ASN1_MAX_CONSTRUCTED_NEST) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_TOO_DEEP);
    goto err;
  }

  switch (it->itype) {
    case ASN1_ITYPE_PRIMITIVE:
      if (it->templates) {

        if ((tag != -1) || opt) {
          OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_OPTIONS_ON_ITEM_TEMPLATE);
          goto err;
        }
        return asn1_template_ex_d2i(pval, in, len, it->templates, opt, buf,
                                    depth);
      }
      return asn1_d2i_ex_primitive(pval, in, len, it, tag, aclass, opt);
      break;

    case ASN1_ITYPE_MSTRING:

      if (tag != -1) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
        goto err;
      }

      p = *in;

      ret = asn1_check_tlen(NULL, &otag, &oclass, NULL, &p, len, -1, 0, 1);
      if (!ret) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
        goto err;
      }

      if (oclass != V_ASN1_UNIVERSAL) {

        if (opt) {
          return -1;
        }
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_MSTRING_NOT_UNIVERSAL);
        goto err;
      }

      if (!(ASN1_tag2bit(otag) & it->utype)) {

        if (opt) {
          return -1;
        }
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_MSTRING_WRONG_TAG);
        goto err;
      }
      return asn1_d2i_ex_primitive(pval, in, len, it, otag, 0, 0);

    case ASN1_ITYPE_EXTERN: {

      if (tag != -1) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
        goto err;
      }
      const ASN1_EXTERN_FUNCS *ef =
          reinterpret_cast<const ASN1_EXTERN_FUNCS *>(it->funcs);
      return ef->asn1_ex_d2i(pval, in, len, it, opt, NULL);
    }

    case ASN1_ITYPE_CHOICE: {

      if (tag != -1) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_TEMPLATE);
        goto err;
      }

      const ASN1_AUX *aux = reinterpret_cast<const ASN1_AUX *>(it->funcs);
      ASN1_aux_cb *asn1_cb = aux != NULL ? aux->asn1_cb : NULL;
      if (asn1_cb && !asn1_cb(ASN1_OP_D2I_PRE, pval, it, NULL)) {
        goto auxerr;
      }

      if (*pval) {

        i = asn1_get_choice_selector(pval, it);
        if ((i >= 0) && (i < it->tcount)) {
          tt = it->templates + i;
          pchptr = asn1_get_field_ptr(pval, tt);
          ASN1_template_free(pchptr, tt);
          asn1_set_choice_selector(pval, -1, it);
        }
      } else if (!ASN1_item_ex_new(pval, it)) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
        goto err;
      }

      p = *in;
      for (i = 0, tt = it->templates; i < it->tcount; i++, tt++) {
        pchptr = asn1_get_field_ptr(pval, tt);

        ret = asn1_template_ex_d2i(pchptr, &p, len, tt, 1, buf, depth);

        if (ret == -1) {
          continue;
        }

        if (ret > 0) {
          break;
        }

        errtt = tt;
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
        goto err;
      }

      if (i == it->tcount) {

        if (opt) {

          ASN1_item_ex_free(pval, it);
          return -1;
        }
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_NO_MATCHING_CHOICE_TYPE);
        goto err;
      }

      asn1_set_choice_selector(pval, i, it);
      if (asn1_cb && !asn1_cb(ASN1_OP_D2I_POST, pval, it, NULL)) {
        goto auxerr;
      }
      *in = p;
      return 1;
    }

    case ASN1_ITYPE_SEQUENCE: {
      p = *in;

      if (tag == -1) {
        tag = V_ASN1_SEQUENCE;
        aclass = V_ASN1_UNIVERSAL;
      }

      ret = asn1_check_tlen(&len, NULL, NULL, &cst, &p, len, tag, aclass, opt);
      if (!ret) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
        goto err;
      } else if (ret == -1) {
        return -1;
      }
      if (!cst) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_SEQUENCE_NOT_CONSTRUCTED);
        goto err;
      }

      if (!*pval && !ASN1_item_ex_new(pval, it)) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
        goto err;
      }

      const ASN1_AUX *aux = reinterpret_cast<const ASN1_AUX *>(it->funcs);
      ASN1_aux_cb *asn1_cb = aux != NULL ? aux->asn1_cb : NULL;
      if (asn1_cb && !asn1_cb(ASN1_OP_D2I_PRE, pval, it, NULL)) {
        goto auxerr;
      }

      for (i = 0, tt = it->templates; i < it->tcount; i++, tt++) {
        if (tt->flags & ASN1_TFLG_ADB_MASK) {
          const ASN1_TEMPLATE *seqtt;
          ASN1_VALUE **pseqval;
          seqtt = asn1_do_adb(pval, tt, 0);
          if (seqtt == NULL) {
            continue;
          }
          pseqval = asn1_get_field_ptr(pval, seqtt);
          ASN1_template_free(pseqval, seqtt);
        }
      }

      for (i = 0, tt = it->templates; i < it->tcount; i++, tt++) {
        const ASN1_TEMPLATE *seqtt;
        ASN1_VALUE **pseqval;
        seqtt = asn1_do_adb(pval, tt, 1);
        if (seqtt == NULL) {
          goto err;
        }
        pseqval = asn1_get_field_ptr(pval, seqtt);

        if (!len) {
          break;
        }
        q = p;

        if (i == (it->tcount - 1)) {
          isopt = 0;
        } else {
          isopt = (seqtt->flags & ASN1_TFLG_OPTIONAL) != 0;
        }

        ret = asn1_template_ex_d2i(pseqval, &p, len, seqtt, isopt, buf, depth);
        if (!ret) {
          errtt = seqtt;
          goto err;
        } else if (ret == -1) {

          ASN1_template_free(pseqval, seqtt);
          continue;
        }

        len -= p - q;
      }

      if (len) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_SEQUENCE_LENGTH_MISMATCH);
        goto err;
      }

      for (; i < it->tcount; tt++, i++) {
        const ASN1_TEMPLATE *seqtt;
        seqtt = asn1_do_adb(pval, tt, 1);
        if (seqtt == NULL) {
          goto err;
        }
        if (seqtt->flags & ASN1_TFLG_OPTIONAL) {
          ASN1_VALUE **pseqval;
          pseqval = asn1_get_field_ptr(pval, seqtt);
          ASN1_template_free(pseqval, seqtt);
        } else {
          errtt = seqtt;
          OPENSSL_PUT_ERROR(ASN1, ASN1_R_FIELD_MISSING);
          goto err;
        }
      }

      if (!asn1_enc_save(pval, *in, p - *in, it, buf)) {
        goto auxerr;
      }
      if (asn1_cb && !asn1_cb(ASN1_OP_D2I_POST, pval, it, NULL)) {
        goto auxerr;
      }
      *in = p;
      return 1;
    }

    default:
      return 0;
  }
auxerr:
  OPENSSL_PUT_ERROR(ASN1, ASN1_R_AUX_ERROR);
err:
  ASN1_item_ex_free(pval, it);
  if (errtt) {
    ERR_add_error_data(4, "Field=", errtt->field_name, ", Type=", it->sname);
  } else {
    ERR_add_error_data(2, "Type=", it->sname);
  }
  return 0;
}

int ASN1_item_ex_d2i(ASN1_VALUE **pval, const unsigned char **in, long len,
                     const ASN1_ITEM *it, int tag, int aclass, char opt,
                     CRYPTO_BUFFER *buf) {
  return asn1_item_ex_d2i(pval, in, len, it, tag, aclass, opt, buf,
                                    0);
}

static int asn1_template_ex_d2i(ASN1_VALUE **val, const unsigned char **in,
                                long inlen, const ASN1_TEMPLATE *tt, char opt,
                                CRYPTO_BUFFER *buf, int depth) {
  int aclass;
  int ret;
  long len;
  const unsigned char *p, *q;
  if (!val) {
    return 0;
  }
  uint32_t flags = tt->flags;
  aclass = flags & ASN1_TFLG_TAG_CLASS;

  p = *in;

  if (flags & ASN1_TFLG_EXPTAG) {
    char cst;

    ret = asn1_check_tlen(&len, NULL, NULL, &cst, &p, inlen, tt->tag, aclass,
                          opt);
    q = p;
    if (!ret) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
      return 0;
    } else if (ret == -1) {
      return -1;
    }
    if (!cst) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_EXPLICIT_TAG_NOT_CONSTRUCTED);
      return 0;
    }

    ret = asn1_template_noexp_d2i(val, &p, len, tt,         0, buf, depth);
    if (!ret) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
      return 0;
    }

    len -= p - q;

    if (len) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_EXPLICIT_LENGTH_MISMATCH);
      goto err;
    }
  } else {
    return asn1_template_noexp_d2i(val, in, inlen, tt, opt, buf, depth);
  }

  *in = p;
  return 1;

err:
  ASN1_template_free(val, tt);
  return 0;
}

static int asn1_template_noexp_d2i(ASN1_VALUE **val, const unsigned char **in,
                                   long len, const ASN1_TEMPLATE *tt, char opt,
                                   CRYPTO_BUFFER *buf, int depth) {
  int aclass;
  int ret;
  const unsigned char *p;
  if (!val) {
    return 0;
  }
  uint32_t flags = tt->flags;
  aclass = flags & ASN1_TFLG_TAG_CLASS;

  p = *in;

  if (flags & ASN1_TFLG_SK_MASK) {

    int sktag, skaclass;

    if (flags & ASN1_TFLG_IMPTAG) {
      sktag = tt->tag;
      skaclass = aclass;
    } else {
      skaclass = V_ASN1_UNIVERSAL;
      if (flags & ASN1_TFLG_SET_OF) {
        sktag = V_ASN1_SET;
      } else {
        sktag = V_ASN1_SEQUENCE;
      }
    }

    ret =
        asn1_check_tlen(&len, NULL, NULL, NULL, &p, len, sktag, skaclass, opt);
    if (!ret) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
      return 0;
    } else if (ret == -1) {
      return -1;
    }
    if (!*val) {
      *val = (ASN1_VALUE *)sk_ASN1_VALUE_new_null();
    } else {

      STACK_OF(ASN1_VALUE) *sktmp = (STACK_OF(ASN1_VALUE) *)*val;
      ASN1_VALUE *vtmp;
      while (sk_ASN1_VALUE_num(sktmp) > 0) {
        vtmp = sk_ASN1_VALUE_pop(sktmp);
        ASN1_item_ex_free(&vtmp, ASN1_ITEM_ptr(tt->item));
      }
    }

    if (!*val) {
      goto err;
    }

    while (len > 0) {
      ASN1_VALUE *skfield;
      const unsigned char *q = p;
      skfield = NULL;
      if (!asn1_item_ex_d2i(&skfield, &p, len, ASN1_ITEM_ptr(tt->item),
                                    -1,            0,         0, buf, depth)) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
        goto err;
      }
      len -= p - q;
      if (!sk_ASN1_VALUE_push((STACK_OF(ASN1_VALUE) *)*val, skfield)) {
        ASN1_item_ex_free(&skfield, ASN1_ITEM_ptr(tt->item));
        goto err;
      }
    }
  } else if (flags & ASN1_TFLG_IMPTAG) {

    ret = asn1_item_ex_d2i(val, &p, len, ASN1_ITEM_ptr(tt->item), tt->tag,
                           aclass, opt, buf, depth);
    if (!ret) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
      goto err;
    } else if (ret == -1) {
      return -1;
    }
  } else {

    ret = asn1_item_ex_d2i(val, &p, len, ASN1_ITEM_ptr(tt->item),         -1,
                                      0, opt, buf, depth);
    if (!ret) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
      goto err;
    } else if (ret == -1) {
      return -1;
    }
  }

  *in = p;
  return 1;

err:
  ASN1_template_free(val, tt);
  return 0;
}

static int asn1_d2i_ex_primitive(ASN1_VALUE **pval, const unsigned char **in,
                                 long inlen, const ASN1_ITEM *it, int tag,
                                 int aclass, char opt) {
  int ret = 0, utype;
  long plen;
  char cst;
  const unsigned char *p;
  const unsigned char *cont = NULL;
  long len;
  if (!pval) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_NULL);
    return 0;
  }

  if (it->itype == ASN1_ITYPE_MSTRING) {
    utype = tag;
    tag = -1;
  } else {
    utype = it->utype;
  }

  if (utype == V_ASN1_ANY) {

    unsigned char oclass;
    if (tag >= 0) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_TAGGED_ANY);
      return 0;
    }
    if (opt) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_OPTIONAL_ANY);
      return 0;
    }
    p = *in;
    ret = asn1_check_tlen(NULL, &utype, &oclass, NULL, &p, inlen, -1, 0, 0);
    if (!ret) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
      return 0;
    }
    if (!is_supported_universal_type(utype, oclass)) {
      utype = V_ASN1_OTHER;
    }
  }
  if (tag == -1) {
    tag = utype;
    aclass = V_ASN1_UNIVERSAL;
  }
  p = *in;

  ret = asn1_check_tlen(&plen, NULL, NULL, &cst, &p, inlen, tag, aclass, opt);
  if (!ret) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_NESTED_ASN1_ERROR);
    return 0;
  } else if (ret == -1) {
    return -1;
  }
  ret = 0;

  if ((utype == V_ASN1_SEQUENCE) || (utype == V_ASN1_SET) ||
      (utype == V_ASN1_OTHER)) {

    if (utype != V_ASN1_OTHER && !cst) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_TYPE_NOT_CONSTRUCTED);
      return 0;
    }

    cont = *in;
    len = p - cont + plen;
    p += plen;
  } else if (cst) {

    OPENSSL_PUT_ERROR(ASN1, ASN1_R_TYPE_NOT_PRIMITIVE);
    return 0;
  } else {
    cont = p;
    len = plen;
    p += plen;
  }

  if (!asn1_ex_c2i(pval, cont, len, utype, it)) {
    goto err;
  }

  *in = p;
  ret = 1;
err:
  return ret;
}

static int asn1_ex_c2i(ASN1_VALUE **pval, const unsigned char *cont, long len,
                       int utype, const ASN1_ITEM *it) {
  ASN1_VALUE **opval = NULL;
  ASN1_STRING *stmp;
  ASN1_TYPE *typ = NULL;
  int ret = 0;
  ASN1_INTEGER **tint;

  assert(it->funcs == NULL);

  if (it->utype == V_ASN1_ANY) {
    if (!*pval) {
      typ = ASN1_TYPE_new();
      if (typ == NULL) {
        goto err;
      }
      *pval = (ASN1_VALUE *)typ;
    } else {
      typ = (ASN1_TYPE *)*pval;
    }

    if (utype != typ->type) {
      ASN1_TYPE_set(typ, utype, NULL);
    }
    opval = pval;
    pval = &typ->value.asn1_value;
  }
  switch (utype) {
    case V_ASN1_OBJECT:
      if (!c2i_ASN1_OBJECT((ASN1_OBJECT **)pval, &cont, len)) {
        goto err;
      }
      break;

    case V_ASN1_NULL:
      if (len) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_NULL_IS_WRONG_LENGTH);
        goto err;
      }
      *pval = (ASN1_VALUE *)1;
      break;

    case V_ASN1_BOOLEAN:
      if (len != 1) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_BOOLEAN_IS_WRONG_LENGTH);
        goto err;
      } else {
        ASN1_BOOLEAN *tbool;
        tbool = (ASN1_BOOLEAN *)pval;
        *tbool = *cont;
      }
      break;

    case V_ASN1_BIT_STRING:
      if (!c2i_ASN1_BIT_STRING((ASN1_BIT_STRING **)pval, &cont, len)) {
        goto err;
      }
      break;

    case V_ASN1_INTEGER:
    case V_ASN1_ENUMERATED:
      tint = (ASN1_INTEGER **)pval;
      if (!c2i_ASN1_INTEGER(tint, &cont, len)) {
        goto err;
      }

      (*tint)->type = utype | ((*tint)->type & V_ASN1_NEG);
      break;

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
    case V_ASN1_OTHER:
    case V_ASN1_SET:
    case V_ASN1_SEQUENCE:

    default: {
      CBS cbs;
      CBS_init(&cbs, cont, (size_t)len);
      if (utype == V_ASN1_BMPSTRING) {
        while (CBS_len(&cbs) != 0) {
          uint32_t c;
          if (!CBS_get_ucs2_be(&cbs, &c)) {
            OPENSSL_PUT_ERROR(ASN1, ASN1_R_INVALID_BMPSTRING);
            goto err;
          }
        }
      }
      if (utype == V_ASN1_UNIVERSALSTRING) {
        while (CBS_len(&cbs) != 0) {
          uint32_t c;
          if (!CBS_get_utf32_be(&cbs, &c)) {
            OPENSSL_PUT_ERROR(ASN1, ASN1_R_INVALID_UNIVERSALSTRING);
            goto err;
          }
        }
      }
      if (utype == V_ASN1_UTF8STRING) {
        while (CBS_len(&cbs) != 0) {
          uint32_t c;
          if (!CBS_get_utf8(&cbs, &c)) {
            OPENSSL_PUT_ERROR(ASN1, ASN1_R_INVALID_UTF8STRING);
            goto err;
          }
        }
      }
      if (utype == V_ASN1_UTCTIME) {
        if (!CBS_parse_utc_time(&cbs, NULL,                           1)) {
          OPENSSL_PUT_ERROR(ASN1, ASN1_R_INVALID_TIME_FORMAT);
          goto err;
        }
      }
      if (utype == V_ASN1_GENERALIZEDTIME) {
        if (!CBS_parse_generalized_time(&cbs, NULL,
                                                                  0)) {
          OPENSSL_PUT_ERROR(ASN1, ASN1_R_INVALID_TIME_FORMAT);
          goto err;
        }
      }

      if (!*pval) {
        stmp = ASN1_STRING_type_new(utype);
        if (!stmp) {
          goto err;
        }
        *pval = (ASN1_VALUE *)stmp;
      } else {
        stmp = (ASN1_STRING *)*pval;
        stmp->type = utype;
      }
      if (!ASN1_STRING_set(stmp, cont, len)) {
        ASN1_STRING_free(stmp);
        *pval = NULL;
        goto err;
      }
      break;
    }
  }

  if (typ && (utype == V_ASN1_NULL)) {
    typ->value.ptr = NULL;
  }

  ret = 1;
err:
  if (!ret) {
    ASN1_TYPE_free(typ);
    if (opval) {
      *opval = NULL;
    }
  }
  return ret;
}

static int asn1_check_tlen(long *olen, int *otag, unsigned char *oclass,
                           char *cst, const unsigned char **in, long len,
                           int exptag, int expclass, char opt) {
  int i;
  int ptag, pclass;
  long plen;
  const unsigned char *p;
  p = *in;

  i = ASN1_get_object(&p, &plen, &ptag, &pclass, len);
  if (i & 0x80) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_BAD_OBJECT_HEADER);
    return 0;
  }
  if (exptag >= 0) {
    if ((exptag != ptag) || (expclass != pclass)) {

      if (opt) {
        return -1;
      }
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_WRONG_TAG);
      return 0;
    }
  }

  if (cst) {
    *cst = i & V_ASN1_CONSTRUCTED;
  }

  if (olen) {
    *olen = plen;
  }

  if (oclass) {
    *oclass = pclass;
  }

  if (otag) {
    *otag = ptag;
  }

  *in = p;
  return 1;
}
