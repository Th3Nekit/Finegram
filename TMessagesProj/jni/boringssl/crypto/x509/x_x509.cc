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

#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include <openssl/asn1t.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/pool.h>
#include <openssl/thread.h>
#include <openssl/x509.h>

#include "../asn1/internal.h"
#include "../bytestring/internal.h"
#include "../internal.h"
#include "internal.h"

static CRYPTO_EX_DATA_CLASS g_ex_data_class = CRYPTO_EX_DATA_CLASS_INIT;

ASN1_SEQUENCE_enc(X509_CINF, enc, 0) = {
    ASN1_EXP_OPT(X509_CINF, version, ASN1_INTEGER, 0),
    ASN1_SIMPLE(X509_CINF, serialNumber, ASN1_INTEGER),
    ASN1_SIMPLE(X509_CINF, signature, X509_ALGOR),
    ASN1_SIMPLE(X509_CINF, issuer, X509_NAME),
    ASN1_SIMPLE(X509_CINF, validity, X509_VAL),
    ASN1_SIMPLE(X509_CINF, subject, X509_NAME),
    ASN1_SIMPLE(X509_CINF, key, X509_PUBKEY),
    ASN1_IMP_OPT(X509_CINF, issuerUID, ASN1_BIT_STRING, 1),
    ASN1_IMP_OPT(X509_CINF, subjectUID, ASN1_BIT_STRING, 2),
    ASN1_EXP_SEQUENCE_OF_OPT(X509_CINF, extensions, X509_EXTENSION, 3),
} ASN1_SEQUENCE_END_enc(X509_CINF, X509_CINF)

IMPLEMENT_ASN1_FUNCTIONS(X509_CINF)

static X509 *x509_new_null(void) {
  X509 *ret = reinterpret_cast<X509 *>(OPENSSL_zalloc(sizeof(X509)));
  if (ret == NULL) {
    return NULL;
  }

  ret->references = 1;
  ret->ex_pathlen = -1;
  CRYPTO_new_ex_data(&ret->ex_data);
  CRYPTO_MUTEX_init(&ret->lock);
  return ret;
}

X509 *X509_new(void) {
  X509 *ret = x509_new_null();
  if (ret == NULL) {
    return NULL;
  }

  ret->cert_info = X509_CINF_new();
  ret->sig_alg = X509_ALGOR_new();
  ret->signature = ASN1_BIT_STRING_new();
  if (ret->cert_info == NULL || ret->sig_alg == NULL ||
      ret->signature == NULL) {
    X509_free(ret);
    return NULL;
  }

  return ret;
}

void X509_free(X509 *x509) {
  if (x509 == NULL || !CRYPTO_refcount_dec_and_test_zero(&x509->references)) {
    return;
  }

  CRYPTO_free_ex_data(&g_ex_data_class, x509, &x509->ex_data);

  X509_CINF_free(x509->cert_info);
  X509_ALGOR_free(x509->sig_alg);
  ASN1_BIT_STRING_free(x509->signature);
  ASN1_OCTET_STRING_free(x509->skid);
  AUTHORITY_KEYID_free(x509->akid);
  CRL_DIST_POINTS_free(x509->crldp);
  GENERAL_NAMES_free(x509->altname);
  NAME_CONSTRAINTS_free(x509->nc);
  X509_CERT_AUX_free(x509->aux);
  CRYPTO_MUTEX_cleanup(&x509->lock);

  OPENSSL_free(x509);
}

static X509 *x509_parse(CBS *cbs, CRYPTO_BUFFER *buf) {
  CBS cert, tbs, sigalg, sig;
  if (!CBS_get_asn1(cbs, &cert, CBS_ASN1_SEQUENCE) ||

      CBS_len(&cert) > INT_MAX / 2 ||
      !CBS_get_asn1_element(&cert, &tbs, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1_element(&cert, &sigalg, CBS_ASN1_SEQUENCE)) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_DECODE_ERROR);
    return NULL;
  }

  CBS_ASN1_TAG tag;
  size_t header_len;
  int indefinite;
  if (!CBS_get_any_ber_asn1_element(&cert, &sig, &tag, &header_len,
                                                      NULL,
                                    &indefinite) ||
      tag != CBS_ASN1_BITSTRING || indefinite ||
      !CBS_skip(&sig, header_len) ||
      CBS_len(&cert) != 0) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_DECODE_ERROR);
    return NULL;
  }

  X509 *ret = x509_new_null();
  if (ret == NULL) {
    return NULL;
  }

  {

    const uint8_t *inp = CBS_data(&tbs);
    if (ASN1_item_ex_d2i((ASN1_VALUE **)&ret->cert_info, &inp, CBS_len(&tbs),
                         ASN1_ITEM_rptr(X509_CINF),         -1,
                                    0,         0, buf) <= 0 ||
        inp != CBS_data(&tbs) + CBS_len(&tbs)) {
      goto err;
    }

    inp = CBS_data(&sigalg);
    ret->sig_alg = d2i_X509_ALGOR(NULL, &inp, CBS_len(&sigalg));
    if (ret->sig_alg == NULL || inp != CBS_data(&sigalg) + CBS_len(&sigalg)) {
      goto err;
    }

    inp = CBS_data(&sig);
    ret->signature = c2i_ASN1_BIT_STRING(NULL, &inp, CBS_len(&sig));
    if (ret->signature == NULL || inp != CBS_data(&sig) + CBS_len(&sig)) {
      goto err;
    }

    long version = X509_VERSION_1;
    if (ret->cert_info->version != NULL) {
      version = ASN1_INTEGER_get(ret->cert_info->version);

      if (version < X509_VERSION_1 || version > X509_VERSION_3) {
        OPENSSL_PUT_ERROR(X509, X509_R_INVALID_VERSION);
        goto err;
      }
    }

    if (version == X509_VERSION_1 && (ret->cert_info->issuerUID != NULL ||
                                      ret->cert_info->subjectUID != NULL)) {
      OPENSSL_PUT_ERROR(X509, X509_R_INVALID_FIELD_FOR_VERSION);
      goto err;
    }

    if (version != X509_VERSION_3 && ret->cert_info->extensions != NULL) {
      OPENSSL_PUT_ERROR(X509, X509_R_INVALID_FIELD_FOR_VERSION);
      goto err;
    }

    return ret;
  }

err:
  X509_free(ret);
  return NULL;
}

X509 *d2i_X509(X509 **out, const uint8_t **inp, long len) {
  X509 *ret = NULL;
  if (len < 0) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_BUFFER_TOO_SMALL);
    goto err;
  }

  CBS cbs;
  CBS_init(&cbs, *inp, (size_t)len);
  ret = x509_parse(&cbs, NULL);
  if (ret == NULL) {
    goto err;
  }

  *inp = CBS_data(&cbs);

err:
  if (out != NULL) {
    X509_free(*out);
    *out = ret;
  }
  return ret;
}

int i2d_X509(X509 *x509, uint8_t **outp) {
  if (x509 == NULL) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_MISSING_VALUE);
    return -1;
  }

  CBB cbb, cert;
  int len;
  if (!CBB_init(&cbb, 64) ||
      !CBB_add_asn1(&cbb, &cert, CBS_ASN1_SEQUENCE)) {
    goto err;
  }

  uint8_t *out;
  len = i2d_X509_CINF(x509->cert_info, NULL);
  if (len < 0 ||
      !CBB_add_space(&cert, &out, (size_t)len) ||
      i2d_X509_CINF(x509->cert_info, &out) != len) {
    goto err;
  }

  len = i2d_X509_ALGOR(x509->sig_alg, NULL);
  if (len < 0 ||
      !CBB_add_space(&cert, &out, (size_t)len) ||
      i2d_X509_ALGOR(x509->sig_alg, &out) != len) {
    goto err;
  }

  len = i2d_ASN1_BIT_STRING(x509->signature, NULL);
  if (len < 0 ||
      !CBB_add_space(&cert, &out, (size_t)len) ||
      i2d_ASN1_BIT_STRING(x509->signature, &out) != len) {
    goto err;
  }

  return CBB_finish_i2d(&cbb, outp);

err:
  CBB_cleanup(&cbb);
  return -1;
}

static int x509_new_cb(ASN1_VALUE **pval, const ASN1_ITEM *it) {
  *pval = (ASN1_VALUE *)X509_new();
  return *pval != NULL;
}

static void x509_free_cb(ASN1_VALUE **pval, const ASN1_ITEM *it) {
  X509_free((X509 *)*pval);
  *pval = NULL;
}

static int x509_d2i_cb(ASN1_VALUE **pval, const unsigned char **in, long len,
                       const ASN1_ITEM *it, int opt, ASN1_TLC *ctx) {
  if (len < 0) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_BUFFER_TOO_SMALL);
    return 0;
  }

  CBS cbs;
  CBS_init(&cbs, *in, len);
  if (opt && !CBS_peek_asn1_tag(&cbs, CBS_ASN1_SEQUENCE)) {
    return -1;
  }

  X509 *ret = x509_parse(&cbs, NULL);
  if (ret == NULL) {
    return 0;
  }

  *in = CBS_data(&cbs);
  X509_free((X509 *)*pval);
  *pval = (ASN1_VALUE *)ret;
  return 1;
}

static int x509_i2d_cb(ASN1_VALUE **pval, unsigned char **out,
                       const ASN1_ITEM *it) {
  return i2d_X509((X509 *)*pval, out);
}

static const ASN1_EXTERN_FUNCS x509_extern_funcs = {
    x509_new_cb,
    x509_free_cb,
    x509_d2i_cb,
    x509_i2d_cb,
};

IMPLEMENT_EXTERN_ASN1(X509, V_ASN1_SEQUENCE, x509_extern_funcs)

X509 *X509_dup(X509 *x509) {
  uint8_t *der = NULL;
  int len = i2d_X509(x509, &der);
  if (len < 0) {
    return NULL;
  }

  const uint8_t *inp = der;
  X509 *ret = d2i_X509(NULL, &inp, len);
  OPENSSL_free(der);
  return ret;
}

X509 *X509_parse_from_buffer(CRYPTO_BUFFER *buf) {
  CBS cbs;
  CBS_init(&cbs, CRYPTO_BUFFER_data(buf), CRYPTO_BUFFER_len(buf));
  X509 *ret = x509_parse(&cbs, buf);
  if (ret == NULL || CBS_len(&cbs) != 0) {
    X509_free(ret);
    return NULL;
  }

  return ret;
}

int X509_up_ref(X509 *x) {
  CRYPTO_refcount_inc(&x->references);
  return 1;
}

int X509_get_ex_new_index(long argl, void *argp, CRYPTO_EX_unused *unused,
                          CRYPTO_EX_dup *dup_unused,
                          CRYPTO_EX_free *free_func) {
  return CRYPTO_get_ex_new_index_ex(&g_ex_data_class, argl, argp, free_func);
}

int X509_set_ex_data(X509 *r, int idx, void *arg) {
  return (CRYPTO_set_ex_data(&r->ex_data, idx, arg));
}

void *X509_get_ex_data(X509 *r, int idx) {
  return (CRYPTO_get_ex_data(&r->ex_data, idx));
}

X509 *d2i_X509_AUX(X509 **a, const unsigned char **pp, long length) {
  const unsigned char *q = *pp;
  X509 *ret;
  int freeret = 0;

  if (!a || *a == NULL) {
    freeret = 1;
  }
  ret = d2i_X509(a, &q, length);

  if (!ret) {
    return NULL;
  }

  length -= q - *pp;

  if (length > 0 && !d2i_X509_CERT_AUX(&ret->aux, &q, length)) {
    goto err;
  }
  *pp = q;
  return ret;
err:
  if (freeret) {
    X509_free(ret);
    if (a) {
      *a = NULL;
    }
  }
  return NULL;
}

static int i2d_x509_aux_internal(X509 *a, unsigned char **pp) {
  int length, tmplen;
  unsigned char *start = pp != NULL ? *pp : NULL;

  assert(pp == NULL || *pp != NULL);

  length = i2d_X509(a, pp);
  if (length <= 0 || a == NULL) {
    return length;
  }

  if (a->aux != NULL) {
    tmplen = i2d_X509_CERT_AUX(a->aux, pp);
    if (tmplen < 0) {
      if (start != NULL) {
        *pp = start;
      }
      return tmplen;
    }
    length += tmplen;
  }

  return length;
}

int i2d_X509_AUX(X509 *a, unsigned char **pp) {
  int length;
  unsigned char *tmp;

  if (pp == NULL || *pp != NULL) {
    return i2d_x509_aux_internal(a, pp);
  }

  if ((length = i2d_x509_aux_internal(a, NULL)) <= 0) {
    return length;
  }

  *pp = tmp = reinterpret_cast<uint8_t *>(OPENSSL_malloc(length));
  if (tmp == NULL) {
    return -1;
  }

  length = i2d_x509_aux_internal(a, &tmp);
  if (length <= 0) {
    OPENSSL_free(*pp);
    *pp = NULL;
  }
  return length;
}

int i2d_re_X509_tbs(X509 *x509, unsigned char **outp) {
  asn1_encoding_clear(&x509->cert_info->enc);
  return i2d_X509_CINF(x509->cert_info, outp);
}

int i2d_X509_tbs(X509 *x509, unsigned char **outp) {
  return i2d_X509_CINF(x509->cert_info, outp);
}

int X509_set1_signature_algo(X509 *x509, const X509_ALGOR *algo) {
  X509_ALGOR *copy1 = X509_ALGOR_dup(algo);
  X509_ALGOR *copy2 = X509_ALGOR_dup(algo);
  if (copy1 == NULL || copy2 == NULL) {
    X509_ALGOR_free(copy1);
    X509_ALGOR_free(copy2);
    return 0;
  }

  X509_ALGOR_free(x509->sig_alg);
  x509->sig_alg = copy1;
  X509_ALGOR_free(x509->cert_info->signature);
  x509->cert_info->signature = copy2;
  return 1;
}

int X509_set1_signature_value(X509 *x509, const uint8_t *sig, size_t sig_len) {
  if (!ASN1_STRING_set(x509->signature, sig, sig_len)) {
    return 0;
  }
  x509->signature->flags &= ~(ASN1_STRING_FLAG_BITS_LEFT | 0x07);
  x509->signature->flags |= ASN1_STRING_FLAG_BITS_LEFT;
  return 1;
}

void X509_get0_signature(const ASN1_BIT_STRING **psig, const X509_ALGOR **palg,
                         const X509 *x) {
  if (psig) {
    *psig = x->signature;
  }
  if (palg) {
    *palg = x->sig_alg;
  }
}

int X509_get_signature_nid(const X509 *x) {
  return OBJ_obj2nid(x->sig_alg->algorithm);
}
