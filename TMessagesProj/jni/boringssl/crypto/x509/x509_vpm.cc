// Copyright 2004-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <string.h>

#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/stack.h>
#include <openssl/x509.h>

#include "../internal.h"
#include "internal.h"

#define SET_HOST 0
#define ADD_HOST 1

static void str_free(char *s) { OPENSSL_free(s); }

static int int_x509_param_set_hosts(X509_VERIFY_PARAM *param, int mode,
                                    const char *name, size_t namelen) {
  char *copy;

  if (name == NULL || namelen == 0) {

    return 0;
  }

  if (name && OPENSSL_memchr(name, '\0', namelen)) {
    return 0;
  }

  if (mode == SET_HOST && param->hosts) {
    sk_OPENSSL_STRING_pop_free(param->hosts, str_free);
    param->hosts = NULL;
  }

  copy = OPENSSL_strndup(name, namelen);
  if (copy == NULL) {
    return 0;
  }

  if (param->hosts == NULL &&
      (param->hosts = sk_OPENSSL_STRING_new_null()) == NULL) {
    OPENSSL_free(copy);
    return 0;
  }

  if (!sk_OPENSSL_STRING_push(param->hosts, copy)) {
    OPENSSL_free(copy);
    if (sk_OPENSSL_STRING_num(param->hosts) == 0) {
      sk_OPENSSL_STRING_free(param->hosts);
      param->hosts = NULL;
    }
    return 0;
  }

  return 1;
}

X509_VERIFY_PARAM *X509_VERIFY_PARAM_new(void) {
  X509_VERIFY_PARAM *param = reinterpret_cast<X509_VERIFY_PARAM *>(
      OPENSSL_zalloc(sizeof(X509_VERIFY_PARAM)));
  if (!param) {
    return NULL;
  }
  param->depth = -1;
  return param;
}

void X509_VERIFY_PARAM_free(X509_VERIFY_PARAM *param) {
  if (param == NULL) {
    return;
  }
  sk_ASN1_OBJECT_pop_free(param->policies, ASN1_OBJECT_free);
  sk_OPENSSL_STRING_pop_free(param->hosts, str_free);
  OPENSSL_free(param->email);
  OPENSSL_free(param->ip);
  OPENSSL_free(param);
}

static int should_copy(int dest_is_set, int src_is_set, int prefer_src) {
  if (prefer_src) {

    return src_is_set;
  }

  return src_is_set && !dest_is_set;
}

static void copy_int_param(int *dest, const int *src, int default_val,
                           int prefer_src) {
  if (should_copy(*dest != default_val, *src != default_val, prefer_src)) {
    *dest = *src;
  }
}

static int x509_verify_param_copy(X509_VERIFY_PARAM *dest,
                                  const X509_VERIFY_PARAM *src,
                                  int prefer_src) {
  if (src == NULL) {
    return 1;
  }

  copy_int_param(&dest->purpose, &src->purpose,                 0, prefer_src);
  copy_int_param(&dest->trust, &src->trust,                 0, prefer_src);
  copy_int_param(&dest->depth, &src->depth,                 -1, prefer_src);

  if (!(dest->flags & X509_V_FLAG_USE_CHECK_TIME)) {
    dest->check_time = src->check_time;

  }

  dest->flags |= src->flags;

  if (should_copy(dest->policies != NULL, src->policies != NULL, prefer_src)) {
    if (!X509_VERIFY_PARAM_set1_policies(dest, src->policies)) {
      return 0;
    }
  }

  if (should_copy(dest->hosts != NULL, src->hosts != NULL, prefer_src)) {
    sk_OPENSSL_STRING_pop_free(dest->hosts, str_free);
    dest->hosts = NULL;
    if (src->hosts) {
      dest->hosts =
          sk_OPENSSL_STRING_deep_copy(src->hosts, OPENSSL_strdup, str_free);
      if (dest->hosts == NULL) {
        return 0;
      }

      dest->hostflags = src->hostflags;
    }
  }

  if (should_copy(dest->email != NULL, src->email != NULL, prefer_src)) {
    if (!X509_VERIFY_PARAM_set1_email(dest, src->email, src->emaillen)) {
      return 0;
    }
  }

  if (should_copy(dest->ip != NULL, src->ip != NULL, prefer_src)) {
    if (!X509_VERIFY_PARAM_set1_ip(dest, src->ip, src->iplen)) {
      return 0;
    }
  }

  dest->poison = src->poison;
  return 1;
}

int X509_VERIFY_PARAM_inherit(X509_VERIFY_PARAM *dest,
                              const X509_VERIFY_PARAM *src) {

  return x509_verify_param_copy(dest, src,                0);
}

int X509_VERIFY_PARAM_set1(X509_VERIFY_PARAM *to,
                           const X509_VERIFY_PARAM *from) {

  return x509_verify_param_copy(to, from,                1);
}

static int int_x509_param_set1(char **pdest, size_t *pdestlen, const char *src,
                               size_t srclen) {
  void *tmp;
  if (src == NULL || srclen == 0) {

    return 0;
  }

  tmp = OPENSSL_memdup(src, srclen);
  if (!tmp) {
    return 0;
  }

  if (*pdest) {
    OPENSSL_free(*pdest);
  }
  *pdest = reinterpret_cast<char *>(tmp);
  if (pdestlen) {
    *pdestlen = srclen;
  }
  return 1;
}

int X509_VERIFY_PARAM_set_flags(X509_VERIFY_PARAM *param, unsigned long flags) {
  param->flags |= flags;
  return 1;
}

int X509_VERIFY_PARAM_clear_flags(X509_VERIFY_PARAM *param,
                                  unsigned long flags) {
  param->flags &= ~flags;
  return 1;
}

unsigned long X509_VERIFY_PARAM_get_flags(const X509_VERIFY_PARAM *param) {
  return param->flags;
}

int X509_VERIFY_PARAM_set_purpose(X509_VERIFY_PARAM *param, int purpose) {
  if (X509_PURPOSE_get0(purpose) == NULL) {
    OPENSSL_PUT_ERROR(X509V3, X509V3_R_INVALID_PURPOSE);
    return 0;
  }
  param->purpose = purpose;
  return 1;
}

int X509_VERIFY_PARAM_set_trust(X509_VERIFY_PARAM *param, int trust) {
  if (!X509_is_valid_trust_id(trust)) {
    OPENSSL_PUT_ERROR(X509, X509_R_UNKNOWN_TRUST_ID);
    return 0;
  }

  param->trust = trust;
  return 1;
}

void X509_VERIFY_PARAM_set_depth(X509_VERIFY_PARAM *param, int depth) {
  param->depth = depth;
}

void X509_VERIFY_PARAM_set_time_posix(X509_VERIFY_PARAM *param, int64_t t) {
  param->check_time = t;
  param->flags |= X509_V_FLAG_USE_CHECK_TIME;
}

void X509_VERIFY_PARAM_set_time(X509_VERIFY_PARAM *param, time_t t) {
  X509_VERIFY_PARAM_set_time_posix(param, t);
}

int X509_VERIFY_PARAM_add0_policy(X509_VERIFY_PARAM *param,
                                  ASN1_OBJECT *policy) {
  if (!param->policies) {
    param->policies = sk_ASN1_OBJECT_new_null();
    if (!param->policies) {
      return 0;
    }
  }
  if (!sk_ASN1_OBJECT_push(param->policies, policy)) {
    return 0;
  }
  return 1;
}

int X509_VERIFY_PARAM_set1_policies(X509_VERIFY_PARAM *param,
                                    const STACK_OF(ASN1_OBJECT) *policies) {
  if (!param) {
    return 0;
  }

  sk_ASN1_OBJECT_pop_free(param->policies, ASN1_OBJECT_free);
  if (!policies) {
    param->policies = NULL;
    return 1;
  }

  param->policies =
      sk_ASN1_OBJECT_deep_copy(policies, OBJ_dup, ASN1_OBJECT_free);
  if (!param->policies) {
    return 0;
  }

  return 1;
}

int X509_VERIFY_PARAM_set1_host(X509_VERIFY_PARAM *param, const char *name,
                                size_t namelen) {
  if (!int_x509_param_set_hosts(param, SET_HOST, name, namelen)) {
    param->poison = 1;
    return 0;
  }
  return 1;
}

int X509_VERIFY_PARAM_add1_host(X509_VERIFY_PARAM *param, const char *name,
                                size_t namelen) {
  if (!int_x509_param_set_hosts(param, ADD_HOST, name, namelen)) {
    param->poison = 1;
    return 0;
  }
  return 1;
}

void X509_VERIFY_PARAM_set_hostflags(X509_VERIFY_PARAM *param,
                                     unsigned int flags) {
  param->hostflags = flags;
}

int X509_VERIFY_PARAM_set1_email(X509_VERIFY_PARAM *param, const char *email,
                                 size_t emaillen) {
  if (OPENSSL_memchr(email, '\0', emaillen) != NULL ||
      !int_x509_param_set1(&param->email, &param->emaillen, email, emaillen)) {
    param->poison = 1;
    return 0;
  }

  return 1;
}

int X509_VERIFY_PARAM_set1_ip(X509_VERIFY_PARAM *param, const unsigned char *ip,
                              size_t iplen) {
  if ((iplen != 4 && iplen != 16) ||
      !int_x509_param_set1((char **)&param->ip, &param->iplen, (char *)ip,
                           iplen)) {
    param->poison = 1;
    return 0;
  }

  return 1;
}

int X509_VERIFY_PARAM_set1_ip_asc(X509_VERIFY_PARAM *param, const char *ipasc) {
  unsigned char ipout[16];
  size_t iplen;

  iplen = (size_t)x509v3_a2i_ipadd(ipout, ipasc);
  if (iplen == 0) {
    return 0;
  }
  return X509_VERIFY_PARAM_set1_ip(param, ipout, iplen);
}

int X509_VERIFY_PARAM_get_depth(const X509_VERIFY_PARAM *param) {
  return param->depth;
}

static const X509_VERIFY_PARAM kDefaultParam = {
                   0,
              X509_V_FLAG_TRUSTED_FIRST,
                0,
              0,
              100,
                 nullptr,
              nullptr,
                  0,
              nullptr,
                 0,
           nullptr,
              0,
               0,
};

static const X509_VERIFY_PARAM kSMIMESignParam = {
                   0,
              0,
                X509_PURPOSE_SMIME_SIGN,
              X509_TRUST_EMAIL,
              -1,
                 nullptr,
              nullptr,
                  0,
              nullptr,
                 0,
           nullptr,
              0,
               0,
};

static const X509_VERIFY_PARAM kSSLClientParam = {
                   0,
              0,
                X509_PURPOSE_SSL_CLIENT,
              X509_TRUST_SSL_CLIENT,
              -1,
                 nullptr,
              nullptr,
                  0,
              nullptr,
                 0,
           nullptr,
              0,
               0,
};

static const X509_VERIFY_PARAM kSSLServerParam = {
                   0,
              0,
                X509_PURPOSE_SSL_SERVER,
              X509_TRUST_SSL_SERVER,
              -1,
                 nullptr,
              nullptr,
                  0,
              nullptr,
                 0,
           nullptr,
              0,
               0,
};

const X509_VERIFY_PARAM *X509_VERIFY_PARAM_lookup(const char *name) {
  if (strcmp(name, "default") == 0) {
    return &kDefaultParam;
  }
  if (strcmp(name, "pkcs7") == 0) {

    return &kSMIMESignParam;
  }
  if (strcmp(name, "smime_sign") == 0) {
    return &kSMIMESignParam;
  }
  if (strcmp(name, "ssl_client") == 0) {
    return &kSSLClientParam;
  }
  if (strcmp(name, "ssl_server") == 0) {
    return &kSSLServerParam;
  }
  return NULL;
}
