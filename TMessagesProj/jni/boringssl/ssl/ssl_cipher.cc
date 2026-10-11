// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
// Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved.
// Copyright 2005 Nokia. All rights reserved.
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

#include <openssl/ssl.h>

#include <assert.h>
#include <string.h>

#include <openssl/err.h>
#include <openssl/md5.h>
#include <openssl/mem.h>
#include <openssl/sha.h>
#include <openssl/stack.h>

#include "../crypto/internal.h"
#include "internal.h"

BSSL_NAMESPACE_BEGIN

static constexpr SSL_CIPHER kCiphers[] = {

    {
        SSL3_TXT_RSA_DES_192_CBC3_SHA,
        "TLS_RSA_WITH_3DES_EDE_CBC_SHA",
        SSL3_CK_RSA_DES_192_CBC3_SHA,
        SSL_kRSA,
        SSL_aRSA_DECRYPT,
        SSL_3DES,
        SSL_SHA1,
        SSL_HANDSHAKE_MAC_DEFAULT,
    },

    {
        TLS1_TXT_RSA_WITH_AES_128_SHA,
        "TLS_RSA_WITH_AES_128_CBC_SHA",
        TLS1_CK_RSA_WITH_AES_128_SHA,
        SSL_kRSA,
        SSL_aRSA_DECRYPT,
        SSL_AES128,
        SSL_SHA1,
        SSL_HANDSHAKE_MAC_DEFAULT,
    },

    {
        TLS1_TXT_RSA_WITH_AES_256_SHA,
        "TLS_RSA_WITH_AES_256_CBC_SHA",
        TLS1_CK_RSA_WITH_AES_256_SHA,
        SSL_kRSA,
        SSL_aRSA_DECRYPT,
        SSL_AES256,
        SSL_SHA1,
        SSL_HANDSHAKE_MAC_DEFAULT,
    },

    {
        TLS1_TXT_PSK_WITH_AES_128_CBC_SHA,
        "TLS_PSK_WITH_AES_128_CBC_SHA",
        TLS1_CK_PSK_WITH_AES_128_CBC_SHA,
        SSL_kPSK,
        SSL_aPSK,
        SSL_AES128,
        SSL_SHA1,
        SSL_HANDSHAKE_MAC_DEFAULT,
    },

    {
        TLS1_TXT_PSK_WITH_AES_256_CBC_SHA,
        "TLS_PSK_WITH_AES_256_CBC_SHA",
        TLS1_CK_PSK_WITH_AES_256_CBC_SHA,
        SSL_kPSK,
        SSL_aPSK,
        SSL_AES256,
        SSL_SHA1,
        SSL_HANDSHAKE_MAC_DEFAULT,
    },

    {
        TLS1_TXT_RSA_WITH_AES_128_GCM_SHA256,
        "TLS_RSA_WITH_AES_128_GCM_SHA256",
        TLS1_CK_RSA_WITH_AES_128_GCM_SHA256,
        SSL_kRSA,
        SSL_aRSA_DECRYPT,
        SSL_AES128GCM,
        SSL_AEAD,
        SSL_HANDSHAKE_MAC_SHA256,
    },

    {
        TLS1_TXT_RSA_WITH_AES_256_GCM_SHA384,
        "TLS_RSA_WITH_AES_256_GCM_SHA384",
        TLS1_CK_RSA_WITH_AES_256_GCM_SHA384,
        SSL_kRSA,
        SSL_aRSA_DECRYPT,
        SSL_AES256GCM,
        SSL_AEAD,
        SSL_HANDSHAKE_MAC_SHA384,
    },

    {
        TLS1_3_RFC_AES_128_GCM_SHA256,
        "TLS_AES_128_GCM_SHA256",
        TLS1_3_CK_AES_128_GCM_SHA256,
        SSL_kGENERIC,
        SSL_aGENERIC,
        SSL_AES128GCM,
        SSL_AEAD,
        SSL_HANDSHAKE_MAC_SHA256,
    },

    {
        TLS1_3_RFC_AES_256_GCM_SHA384,
        "TLS_AES_256_GCM_SHA384",
        TLS1_3_CK_AES_256_GCM_SHA384,
        SSL_kGENERIC,
        SSL_aGENERIC,
        SSL_AES256GCM,
        SSL_AEAD,
        SSL_HANDSHAKE_MAC_SHA384,
    },

    {
        TLS1_3_RFC_CHACHA20_POLY1305_SHA256,
        "TLS_CHACHA20_POLY1305_SHA256",
        TLS1_3_CK_CHACHA20_POLY1305_SHA256,
        SSL_kGENERIC,
        SSL_aGENERIC,
        SSL_CHACHA20POLY1305,
        SSL_AEAD,
        SSL_HANDSHAKE_MAC_SHA256,
    },

    {
        TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
        "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA",
        TLS1_CK_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_AES128,
        SSL_SHA1,
        SSL_HANDSHAKE_MAC_DEFAULT,
    },

    {
        TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
        "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA",
        TLS1_CK_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_AES256,
        SSL_SHA1,
        SSL_HANDSHAKE_MAC_DEFAULT,
    },

    {
        TLS1_TXT_ECDHE_RSA_WITH_AES_128_CBC_SHA,
        "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA",
        TLS1_CK_ECDHE_RSA_WITH_AES_128_CBC_SHA,
        SSL_kECDHE,
        SSL_aRSA_SIGN,
        SSL_AES128,
        SSL_SHA1,
        SSL_HANDSHAKE_MAC_DEFAULT,
    },

    {
        TLS1_TXT_ECDHE_RSA_WITH_AES_256_CBC_SHA,
        "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA",
        TLS1_CK_ECDHE_RSA_WITH_AES_256_CBC_SHA,
        SSL_kECDHE,
        SSL_aRSA_SIGN,
        SSL_AES256,
        SSL_SHA1,
        SSL_HANDSHAKE_MAC_DEFAULT,
    },

    {
        TLS1_TXT_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
        "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256",
        TLS1_CK_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
        SSL_kECDHE,
        SSL_aRSA_SIGN,
        SSL_AES128,
        SSL_SHA256,
        SSL_HANDSHAKE_MAC_SHA256,
    },

    {
        TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
        "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256",
        TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_AES128GCM,
        SSL_AEAD,
        SSL_HANDSHAKE_MAC_SHA256,
    },

    {
        TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
        "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384",
        TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_AES256GCM,
        SSL_AEAD,
        SSL_HANDSHAKE_MAC_SHA384,
    },

    {
        TLS1_TXT_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",
        TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        SSL_kECDHE,
        SSL_aRSA_SIGN,
        SSL_AES128GCM,
        SSL_AEAD,
        SSL_HANDSHAKE_MAC_SHA256,
    },

    {
        TLS1_TXT_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
        "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",
        TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
        SSL_kECDHE,
        SSL_aRSA_SIGN,
        SSL_AES256GCM,
        SSL_AEAD,
        SSL_HANDSHAKE_MAC_SHA384,
    },

    {
        TLS1_TXT_ECDHE_PSK_WITH_AES_128_CBC_SHA,
        "TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA",
        TLS1_CK_ECDHE_PSK_WITH_AES_128_CBC_SHA,
        SSL_kECDHE,
        SSL_aPSK,
        SSL_AES128,
        SSL_SHA1,
        SSL_HANDSHAKE_MAC_DEFAULT,
    },

    {
        TLS1_TXT_ECDHE_PSK_WITH_AES_256_CBC_SHA,
        "TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA",
        TLS1_CK_ECDHE_PSK_WITH_AES_256_CBC_SHA,
        SSL_kECDHE,
        SSL_aPSK,
        SSL_AES256,
        SSL_SHA1,
        SSL_HANDSHAKE_MAC_DEFAULT,
    },

    {
        TLS1_TXT_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
        "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256",
        TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
        SSL_kECDHE,
        SSL_aRSA_SIGN,
        SSL_CHACHA20POLY1305,
        SSL_AEAD,
        SSL_HANDSHAKE_MAC_SHA256,
    },

    {
        TLS1_TXT_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
        "TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256",
        TLS1_CK_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_CHACHA20POLY1305,
        SSL_AEAD,
        SSL_HANDSHAKE_MAC_SHA256,
    },

    {
        TLS1_TXT_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256,
        "TLS_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256",
        TLS1_CK_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256,
        SSL_kECDHE,
        SSL_aPSK,
        SSL_CHACHA20POLY1305,
        SSL_AEAD,
        SSL_HANDSHAKE_MAC_SHA256,
    },

};

Span<const SSL_CIPHER> AllCiphers() { return kCiphers; }

static constexpr size_t NumTLS13Ciphers() {
  size_t num = 0;
  for (const auto &cipher : kCiphers) {
    if (cipher.algorithm_mkey == SSL_kGENERIC) {
      num++;
    }
  }
  return num;
}

#define CIPHER_ADD 1
#define CIPHER_KILL 2
#define CIPHER_DEL 3
#define CIPHER_ORD 4
#define CIPHER_SPECIAL 5

typedef struct cipher_order_st {
  const SSL_CIPHER *cipher;
  bool active;
  bool in_group;
  struct cipher_order_st *next, *prev;
} CIPHER_ORDER;

typedef struct cipher_alias_st {

  const char *name = nullptr;

  uint32_t algorithm_mkey = ~0u;
  uint32_t algorithm_auth = ~0u;
  uint32_t algorithm_enc = ~0u;
  uint32_t algorithm_mac = ~0u;

  uint16_t min_version = 0;

  bool include_deprecated = false;
} CIPHER_ALIAS;

static const CIPHER_ALIAS kCipherAliases[] = {
    {"ALL", ~0u, ~0u, ~0u, ~0u, 0},

    {"kRSA", SSL_kRSA, ~0u, ~0u, ~0u, 0},

    {"kECDHE", SSL_kECDHE, ~0u, ~0u, ~0u, 0},
    {"kEECDH", SSL_kECDHE, ~0u, ~0u, ~0u, 0},
    {"ECDH", SSL_kECDHE, ~0u, ~0u, ~0u, 0},

    {"kPSK", SSL_kPSK, ~0u, ~0u, ~0u, 0},

    {"aRSA", ~0u, SSL_aRSA_SIGN | SSL_aRSA_DECRYPT, ~0u, ~0u, 0},
    {"aECDSA", ~0u, SSL_aECDSA, ~0u, ~0u, 0},
    {"ECDSA", ~0u, SSL_aECDSA, ~0u, ~0u, 0},
    {"aPSK", ~0u, SSL_aPSK, ~0u, ~0u, 0},

    {"ECDHE", SSL_kECDHE, ~0u, ~0u, ~0u, 0},
    {"EECDH", SSL_kECDHE, ~0u, ~0u, ~0u, 0},
    {"RSA", SSL_kRSA, SSL_aRSA_SIGN | SSL_aRSA_DECRYPT, ~0u, ~0u, 0},
    {"PSK", SSL_kPSK, SSL_aPSK, ~0u, ~0u, 0},

    {"3DES", ~0u, ~0u, SSL_3DES, ~0u, 0,                        true},
    {"AES128", ~0u, ~0u, SSL_AES128 | SSL_AES128GCM, ~0u, 0,
                            false},
    {"AES256", ~0u, ~0u, SSL_AES256 | SSL_AES256GCM, ~0u, 0,
                            false},
    {"AES", ~0u, ~0u, SSL_AES, ~0u, 0},
    {"AESGCM", ~0u, ~0u, SSL_AES128GCM | SSL_AES256GCM, ~0u, 0,
                            false},
    {"CHACHA20", ~0u, ~0u, SSL_CHACHA20POLY1305, ~0u, 0,
                            false},

    {"SHA1", ~0u, ~0u, ~0u, SSL_SHA1, 0},
    {"SHA", ~0u, ~0u, ~0u, SSL_SHA1, 0},

    {"SSLv3", ~0u, ~0u, ~0u, ~0u, SSL3_VERSION},
    {"TLSv1", ~0u, ~0u, ~0u, ~0u, SSL3_VERSION},
    {"TLSv1.2", ~0u, ~0u, ~0u, ~0u, TLS1_2_VERSION},

    {"HIGH", ~0u, ~0u, ~0u, ~0u, 0},
    {"FIPS", ~0u, ~0u, ~0u, ~0u, 0},

    {"SHA256", 0, 0, 0, 0, 0},
    {"SHA384", 0, 0, 0, 0, 0},
};

static const size_t kCipherAliasesLen = OPENSSL_ARRAY_SIZE(kCipherAliases);

bool ssl_cipher_get_evp_aead(const EVP_AEAD **out_aead,
                             size_t *out_mac_secret_len,
                             size_t *out_fixed_iv_len, const SSL_CIPHER *cipher,
                             uint16_t version) {
  *out_aead = NULL;
  *out_mac_secret_len = 0;
  *out_fixed_iv_len = 0;

  if (cipher->algorithm_mac == SSL_AEAD) {
    if (cipher->algorithm_enc == SSL_AES128GCM) {
      if (version < TLS1_3_VERSION) {
        *out_aead = EVP_aead_aes_128_gcm_tls12();
      } else {
        *out_aead = EVP_aead_aes_128_gcm_tls13();
      }
      *out_fixed_iv_len = 4;
    } else if (cipher->algorithm_enc == SSL_AES256GCM) {
      if (version < TLS1_3_VERSION) {
        *out_aead = EVP_aead_aes_256_gcm_tls12();
      } else {
        *out_aead = EVP_aead_aes_256_gcm_tls13();
      }
      *out_fixed_iv_len = 4;
    } else if (cipher->algorithm_enc == SSL_CHACHA20POLY1305) {
      *out_aead = EVP_aead_chacha20_poly1305();
      *out_fixed_iv_len = 12;
    } else {
      return false;
    }

    if (version >= TLS1_3_VERSION) {
      *out_fixed_iv_len = EVP_AEAD_nonce_length(*out_aead);
    }
  } else if (cipher->algorithm_mac == SSL_SHA1) {
    if (cipher->algorithm_enc == SSL_3DES) {
      if (version == TLS1_VERSION) {
        *out_aead = EVP_aead_des_ede3_cbc_sha1_tls_implicit_iv();
        *out_fixed_iv_len = 8;
      } else {
        *out_aead = EVP_aead_des_ede3_cbc_sha1_tls();
      }
    } else if (cipher->algorithm_enc == SSL_AES128) {
      if (version == TLS1_VERSION) {
        *out_aead = EVP_aead_aes_128_cbc_sha1_tls_implicit_iv();
        *out_fixed_iv_len = 16;
      } else {
        *out_aead = EVP_aead_aes_128_cbc_sha1_tls();
      }
    } else if (cipher->algorithm_enc == SSL_AES256) {
      if (version == TLS1_VERSION) {
        *out_aead = EVP_aead_aes_256_cbc_sha1_tls_implicit_iv();
        *out_fixed_iv_len = 16;
      } else {
        *out_aead = EVP_aead_aes_256_cbc_sha1_tls();
      }
    } else {
      return false;
    }

    *out_mac_secret_len = SHA_DIGEST_LENGTH;
  } else if (cipher->algorithm_mac == SSL_SHA256) {
    if (cipher->algorithm_enc == SSL_AES128) {
      *out_aead = EVP_aead_aes_128_cbc_sha256_tls();
    } else {
      return false;
    }

    *out_mac_secret_len = SHA256_DIGEST_LENGTH;
  } else {
    return false;
  }

  return true;
}

const EVP_MD *ssl_get_handshake_digest(uint16_t version,
                                       const SSL_CIPHER *cipher) {
  switch (cipher->algorithm_prf) {
    case SSL_HANDSHAKE_MAC_DEFAULT:
      return version >= TLS1_2_VERSION ? EVP_sha256() : EVP_md5_sha1();
    case SSL_HANDSHAKE_MAC_SHA256:
      return EVP_sha256();
    case SSL_HANDSHAKE_MAC_SHA384:
      return EVP_sha384();
    default:
      assert(0);
      return NULL;
  }
}

static bool is_cipher_list_separator(char c, bool is_strict) {
  if (c == ':') {
    return true;
  }
  return !is_strict && (c == ' ' || c == ';' || c == ',');
}

static bool rule_equals(const char *rule, const char *buf, size_t buf_len) {

  return strncmp(rule, buf, buf_len) == 0 && rule[buf_len] == '\0';
}

static void ll_append_tail(CIPHER_ORDER **head, CIPHER_ORDER *curr,
                           CIPHER_ORDER **tail) {
  if (curr == *tail) {
    return;
  }
  if (curr == *head) {
    *head = curr->next;
  }
  if (curr->prev != NULL) {
    curr->prev->next = curr->next;
  }
  if (curr->next != NULL) {
    curr->next->prev = curr->prev;
  }
  (*tail)->next = curr;
  curr->prev = *tail;
  curr->next = NULL;
  *tail = curr;
}

static void ll_append_head(CIPHER_ORDER **head, CIPHER_ORDER *curr,
                           CIPHER_ORDER **tail) {
  if (curr == *head) {
    return;
  }
  if (curr == *tail) {
    *tail = curr->prev;
  }
  if (curr->next != NULL) {
    curr->next->prev = curr->prev;
  }
  if (curr->prev != NULL) {
    curr->prev->next = curr->next;
  }
  (*head)->prev = curr;
  curr->next = *head;
  curr->prev = NULL;
  *head = curr;
}

SSLCipherPreferenceList::~SSLCipherPreferenceList() {
  OPENSSL_free(in_group_flags);
}

bool SSLCipherPreferenceList::Init(UniquePtr<STACK_OF(SSL_CIPHER)> ciphers_arg,
                                   Span<const bool> in_group_flags_arg) {
  if (sk_SSL_CIPHER_num(ciphers_arg.get()) != in_group_flags_arg.size()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  Array<bool> copy;
  if (!copy.CopyFrom(in_group_flags_arg)) {
    return false;
  }
  ciphers = std::move(ciphers_arg);
  size_t unused_len;
  copy.Release(&in_group_flags, &unused_len);
  return true;
}

bool SSLCipherPreferenceList::Init(const SSLCipherPreferenceList &other) {
  size_t size = sk_SSL_CIPHER_num(other.ciphers.get());
  Span<const bool> other_flags(other.in_group_flags, size);
  UniquePtr<STACK_OF(SSL_CIPHER)> other_ciphers(
      sk_SSL_CIPHER_dup(other.ciphers.get()));
  if (!other_ciphers) {
    return false;
  }
  return Init(std::move(other_ciphers), other_flags);
}

void SSLCipherPreferenceList::Remove(const SSL_CIPHER *cipher) {
  size_t index;
  if (!sk_SSL_CIPHER_find(ciphers.get(), &index, cipher)) {
    return;
  }
  if (!in_group_flags[index]                             && index > 0) {
    in_group_flags[index - 1] = false;
  }
  for (size_t i = index; i < sk_SSL_CIPHER_num(ciphers.get()) - 1; ++i) {
    in_group_flags[i] = in_group_flags[i + 1];
  }
  sk_SSL_CIPHER_delete(ciphers.get(), index);
}

bool ssl_cipher_is_deprecated(const SSL_CIPHER *cipher) {
  return cipher->id == TLS1_CK_ECDHE_RSA_WITH_AES_128_CBC_SHA256 ||
         cipher->algorithm_enc == SSL_3DES;
}

static void ssl_cipher_apply_rule(uint32_t cipher_id, const CIPHER_ALIAS *alias,
                                  int rule, int strength_bits, bool in_group,
                                  CIPHER_ORDER **head_p,
                                  CIPHER_ORDER **tail_p) {
  CIPHER_ORDER *head, *tail, *curr, *next, *last;
  const SSL_CIPHER *cp;
  bool reverse = false;

  if (cipher_id == 0 && strength_bits == -1 && alias->min_version == 0 &&
      (alias->algorithm_mkey == 0 || alias->algorithm_auth == 0 ||
       alias->algorithm_enc == 0 || alias->algorithm_mac == 0)) {

    return;
  }

  if (rule == CIPHER_DEL) {

    reverse = true;
  }

  head = *head_p;
  tail = *tail_p;

  if (reverse) {
    next = tail;
    last = head;
  } else {
    next = head;
    last = tail;
  }

  curr = NULL;
  for (;;) {
    if (curr == last) {
      break;
    }

    curr = next;
    if (curr == NULL) {
      break;
    }

    next = reverse ? curr->prev : curr->next;
    cp = curr->cipher;

    if (cipher_id != 0) {
      if (cipher_id != cp->id) {
        continue;
      }
    } else if (strength_bits >= 0) {
      if (strength_bits != SSL_CIPHER_get_bits(cp, NULL)) {
        continue;
      }
    } else {
      if (!(alias->algorithm_mkey & cp->algorithm_mkey) ||
          !(alias->algorithm_auth & cp->algorithm_auth) ||
          !(alias->algorithm_enc & cp->algorithm_enc) ||
          !(alias->algorithm_mac & cp->algorithm_mac) ||
          (alias->min_version != 0 &&
           SSL_CIPHER_get_min_version(cp) != alias->min_version) ||
          (!alias->include_deprecated && ssl_cipher_is_deprecated(cp))) {
        continue;
      }
    }

    if (rule == CIPHER_ADD) {

      if (!curr->active) {
        ll_append_tail(&head, curr, &tail);
        curr->active = true;
        curr->in_group = in_group;
      }
    }

    else if (rule == CIPHER_ORD) {

      if (curr->active) {
        ll_append_tail(&head, curr, &tail);
        curr->in_group = false;
      }
    } else if (rule == CIPHER_DEL) {

      if (curr->active) {

        ll_append_head(&head, curr, &tail);
        curr->active = false;
        curr->in_group = false;
      }
    } else if (rule == CIPHER_KILL) {

      if (head == curr) {
        head = curr->next;
      } else {
        curr->prev->next = curr->next;
      }

      if (tail == curr) {
        tail = curr->prev;
      }
      curr->active = false;
      if (curr->next != NULL) {
        curr->next->prev = curr->prev;
      }
      if (curr->prev != NULL) {
        curr->prev->next = curr->next;
      }
      curr->next = NULL;
      curr->prev = NULL;
    }
  }

  *head_p = head;
  *tail_p = tail;
}

static bool ssl_cipher_strength_sort(CIPHER_ORDER **head_p,
                                     CIPHER_ORDER **tail_p) {

  int max_strength_bits = 0;
  CIPHER_ORDER *curr = *head_p;
  while (curr != NULL) {
    if (curr->active &&
        SSL_CIPHER_get_bits(curr->cipher, NULL) > max_strength_bits) {
      max_strength_bits = SSL_CIPHER_get_bits(curr->cipher, NULL);
    }
    curr = curr->next;
  }

  Array<int> number_uses;
  if (!number_uses.Init(max_strength_bits + 1)) {
    return false;
  }

  curr = *head_p;
  while (curr != NULL) {
    if (curr->active) {
      number_uses[SSL_CIPHER_get_bits(curr->cipher, NULL)]++;
    }
    curr = curr->next;
  }

  for (int i = max_strength_bits; i >= 0; i--) {
    if (number_uses[i] > 0) {
      ssl_cipher_apply_rule(              0,           nullptr, CIPHER_ORD, i,
                            false, head_p, tail_p);
    }
  }

  return true;
}

static bool ssl_cipher_process_rulestr(const char *rule_str,
                                       CIPHER_ORDER **head_p,
                                       CIPHER_ORDER **tail_p, bool strict) {
  const char *l, *buf;
  bool in_group = false, has_group = false;
  size_t j, buf_len;
  char ch;

  l = rule_str;
  for (;;) {
    ch = *l;

    if (ch == '\0') {
      break;
    }

    int rule;
    if (in_group) {
      if (ch == ']') {
        if (*tail_p) {
          (*tail_p)->in_group = false;
        }
        in_group = false;
        l++;
        continue;
      }

      if (ch == '|') {
        rule = CIPHER_ADD;
        l++;
        continue;
      } else if (!OPENSSL_isalnum(ch)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_OPERATOR_IN_GROUP);
        return false;
      } else {
        rule = CIPHER_ADD;
      }
    } else if (ch == '-') {
      rule = CIPHER_DEL;
      l++;
    } else if (ch == '+') {
      rule = CIPHER_ORD;
      l++;
    } else if (ch == '!') {
      rule = CIPHER_KILL;
      l++;
    } else if (ch == '@') {
      rule = CIPHER_SPECIAL;
      l++;
    } else if (ch == '[') {
      assert(!in_group);
      in_group = true;
      has_group = true;
      l++;
      continue;
    } else {
      rule = CIPHER_ADD;
    }

    if (has_group && rule != CIPHER_ADD) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_MIXED_SPECIAL_OPERATOR_WITH_GROUPS);
      return false;
    }

    if (is_cipher_list_separator(ch, strict)) {
      l++;
      continue;
    }

    bool multi = false;
    uint32_t cipher_id = 0;
    CIPHER_ALIAS alias;
    bool skip_rule = false;

    alias.include_deprecated = rule != CIPHER_ADD;

    for (;;) {
      ch = *l;
      buf = l;
      buf_len = 0;
      while (OPENSSL_isalnum(ch) || ch == '-' || ch == '.' || ch == '_') {
        ch = *(++l);
        buf_len++;
      }

      if (buf_len == 0) {

        OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_COMMAND);
        return false;
      }

      if (rule == CIPHER_SPECIAL) {
        break;
      }

      if (!multi && ch != '+') {
        for (j = 0; j < OPENSSL_ARRAY_SIZE(kCiphers); j++) {
          const SSL_CIPHER *cipher = &kCiphers[j];
          if (rule_equals(cipher->name, buf, buf_len) ||
              rule_equals(cipher->standard_name, buf, buf_len)) {
            cipher_id = cipher->id;
            break;
          }
        }
      }
      if (cipher_id == 0) {

        for (j = 0; j < kCipherAliasesLen; j++) {
          if (rule_equals(kCipherAliases[j].name, buf, buf_len)) {
            alias.algorithm_mkey &= kCipherAliases[j].algorithm_mkey;
            alias.algorithm_auth &= kCipherAliases[j].algorithm_auth;
            alias.algorithm_enc &= kCipherAliases[j].algorithm_enc;
            alias.algorithm_mac &= kCipherAliases[j].algorithm_mac;

            alias.include_deprecated |= kCipherAliases[j].include_deprecated;

            if (alias.min_version != 0 &&
                alias.min_version != kCipherAliases[j].min_version) {
              skip_rule = true;
            } else {
              alias.min_version = kCipherAliases[j].min_version;
            }
            break;
          }
        }
        if (j == kCipherAliasesLen) {
          skip_rule = true;
          if (strict) {
            OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_COMMAND);
            return false;
          }
        }
      }

      if (ch != '+') {
        break;
      }
      l++;
      multi = true;
    }

    if (rule == CIPHER_SPECIAL) {
      if (buf_len != 8 || strncmp(buf, "STRENGTH", 8) != 0) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_COMMAND);
        return false;
      }
      if (!ssl_cipher_strength_sort(head_p, tail_p)) {
        return false;
      }

      while (*l != '\0' && !is_cipher_list_separator(*l, strict)) {
        l++;
      }
    } else if (!skip_rule) {
      ssl_cipher_apply_rule(cipher_id, &alias, rule, -1, in_group, head_p,
                            tail_p);
    }
  }

  if (in_group) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_COMMAND);
    return false;
  }

  return true;
}

bool ssl_create_cipher_list(UniquePtr<SSLCipherPreferenceList> *out_cipher_list,
                            const bool has_aes_hw, const char *rule_str,
                            bool strict) {

  if (rule_str == NULL || out_cipher_list == NULL) {
    return false;
  }

  static const uint16_t kAESCiphers[] = {
      TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 & 0xffff,
      TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256 & 0xffff,
      TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 & 0xffff,
      TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384 & 0xffff,
  };
  static const uint16_t kChaChaCiphers[] = {
      TLS1_CK_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256 & 0xffff,
      TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 & 0xffff,
      TLS1_CK_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256 & 0xffff,
  };
  static const uint16_t kLegacyCiphers[] = {
      TLS1_CK_ECDHE_ECDSA_WITH_AES_128_CBC_SHA & 0xffff,
      TLS1_CK_ECDHE_RSA_WITH_AES_128_CBC_SHA & 0xffff,
      TLS1_CK_ECDHE_PSK_WITH_AES_128_CBC_SHA & 0xffff,
      TLS1_CK_ECDHE_ECDSA_WITH_AES_256_CBC_SHA & 0xffff,
      TLS1_CK_ECDHE_RSA_WITH_AES_256_CBC_SHA & 0xffff,
      TLS1_CK_ECDHE_PSK_WITH_AES_256_CBC_SHA & 0xffff,
      TLS1_CK_ECDHE_RSA_WITH_AES_128_CBC_SHA256 & 0xffff,
      TLS1_CK_RSA_WITH_AES_128_GCM_SHA256 & 0xffff,
      TLS1_CK_RSA_WITH_AES_256_GCM_SHA384 & 0xffff,
      TLS1_CK_RSA_WITH_AES_128_SHA & 0xffff,
      TLS1_CK_PSK_WITH_AES_128_CBC_SHA & 0xffff,
      TLS1_CK_RSA_WITH_AES_256_SHA & 0xffff,
      TLS1_CK_PSK_WITH_AES_256_CBC_SHA & 0xffff,
      SSL3_CK_RSA_DES_192_CBC3_SHA & 0xffff,
  };

  CIPHER_ORDER co_list[OPENSSL_ARRAY_SIZE(kAESCiphers) +
                       OPENSSL_ARRAY_SIZE(kChaChaCiphers) +
                       OPENSSL_ARRAY_SIZE(kLegacyCiphers)];
  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(co_list); i++) {
    co_list[i].next =
        i + 1 < OPENSSL_ARRAY_SIZE(co_list) ? &co_list[i + 1] : nullptr;
    co_list[i].prev = i == 0 ? nullptr : &co_list[i - 1];
    co_list[i].active = false;
    co_list[i].in_group = false;
  }
  CIPHER_ORDER *head = &co_list[0];
  CIPHER_ORDER *tail = &co_list[OPENSSL_ARRAY_SIZE(co_list) - 1];

  size_t num = 0;
  if (has_aes_hw) {
    for (uint16_t id : kAESCiphers) {
      co_list[num++].cipher = SSL_get_cipher_by_value(id);
      assert(co_list[num - 1].cipher != nullptr);
    }
  }
  for (uint16_t id : kChaChaCiphers) {
    co_list[num++].cipher = SSL_get_cipher_by_value(id);
    assert(co_list[num - 1].cipher != nullptr);
  }
  if (!has_aes_hw) {
    for (uint16_t id : kAESCiphers) {
      co_list[num++].cipher = SSL_get_cipher_by_value(id);
      assert(co_list[num - 1].cipher != nullptr);
    }
  }
  for (uint16_t id : kLegacyCiphers) {
    co_list[num++].cipher = SSL_get_cipher_by_value(id);
    assert(co_list[num - 1].cipher != nullptr);
  }
  assert(num == OPENSSL_ARRAY_SIZE(co_list));
  static_assert(OPENSSL_ARRAY_SIZE(co_list) + NumTLS13Ciphers() ==
                    OPENSSL_ARRAY_SIZE(kCiphers),
                "Not all ciphers are included in the cipher order");

  const char *rule_p = rule_str;
  if (strncmp(rule_str, "DEFAULT", 7) == 0) {
    if (!ssl_cipher_process_rulestr(SSL_DEFAULT_CIPHER_LIST, &head, &tail,
                                    strict)) {
      return false;
    }
    rule_p += 7;
    if (*rule_p == ':') {
      rule_p++;
    }
  }

  if (*rule_p != '\0' &&
      !ssl_cipher_process_rulestr(rule_p, &head, &tail, strict)) {
    return false;
  }

  UniquePtr<STACK_OF(SSL_CIPHER)> cipherstack(sk_SSL_CIPHER_new_null());
  Array<bool> in_group_flags;
  if (cipherstack == nullptr ||
      !in_group_flags.InitForOverwrite(OPENSSL_ARRAY_SIZE(kCiphers))) {
    return false;
  }

  size_t num_in_group_flags = 0;
  for (CIPHER_ORDER *curr = head; curr != NULL; curr = curr->next) {
    if (curr->active) {
      if (!sk_SSL_CIPHER_push(cipherstack.get(), curr->cipher)) {
        return false;
      }
      in_group_flags[num_in_group_flags++] = curr->in_group;
    }
  }
  in_group_flags.Shrink(num_in_group_flags);

  UniquePtr<SSLCipherPreferenceList> pref_list =
      MakeUnique<SSLCipherPreferenceList>();
  if (!pref_list || !pref_list->Init(std::move(cipherstack), in_group_flags)) {
    return false;
  }

  *out_cipher_list = std::move(pref_list);

  if (sk_SSL_CIPHER_num((*out_cipher_list)->ciphers.get()) == 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_NO_CIPHER_MATCH);
    return false;
  }

  return true;
}

uint32_t ssl_cipher_auth_mask_for_key(const EVP_PKEY *key, bool sign_ok) {
  switch (EVP_PKEY_id(key)) {
    case EVP_PKEY_RSA:
      return sign_ok ? (SSL_aRSA_SIGN | SSL_aRSA_DECRYPT) : SSL_aRSA_DECRYPT;
    case EVP_PKEY_EC:
    case EVP_PKEY_ED25519:

      return sign_ok ? SSL_aECDSA : 0;
    default:
      return 0;
  }
}

bool ssl_cipher_uses_certificate_auth(const SSL_CIPHER *cipher) {
  return (cipher->algorithm_auth & SSL_aCERT) != 0;
}

bool ssl_cipher_requires_server_key_exchange(const SSL_CIPHER *cipher) {

  return (cipher->algorithm_mkey & SSL_kECDHE) != 0;
}

size_t ssl_cipher_get_record_split_len(const SSL_CIPHER *cipher) {
  size_t block_size;
  switch (cipher->algorithm_enc) {
    case SSL_3DES:
      block_size = 8;
      break;
    case SSL_AES128:
    case SSL_AES256:
      block_size = 16;
      break;
    default:
      return 0;
  }

  assert(cipher->algorithm_mac == SSL_SHA1);
  size_t ret = 1 + SHA_DIGEST_LENGTH;
  ret += block_size - (ret % block_size);
  return ret;
}

BSSL_NAMESPACE_END

using namespace bssl;

static constexpr int ssl_cipher_id_cmp(const SSL_CIPHER *a,
                                       const SSL_CIPHER *b) {
  if (a->id > b->id) {
    return 1;
  }
  if (a->id < b->id) {
    return -1;
  }
  return 0;
}

static int ssl_cipher_id_cmp_void(const void *in_a, const void *in_b) {
  return ssl_cipher_id_cmp(reinterpret_cast<const SSL_CIPHER *>(in_a),
                           reinterpret_cast<const SSL_CIPHER *>(in_b));
}

template <size_t N>
static constexpr bool ssl_ciphers_sorted(const SSL_CIPHER (&ciphers)[N]) {
  for (size_t i = 1; i < N; i++) {
    if (ssl_cipher_id_cmp(&ciphers[i - 1], &ciphers[i]) >= 0) {
      return false;
    }
  }
  return true;
}

static_assert(ssl_ciphers_sorted(kCiphers),
              "Ciphers are not sorted, bsearch won't work");

const SSL_CIPHER *SSL_get_cipher_by_value(uint16_t value) {
  SSL_CIPHER c;

  c.id = 0x03000000L | value;
  return reinterpret_cast<const SSL_CIPHER *>(
      bsearch(&c, kCiphers, OPENSSL_ARRAY_SIZE(kCiphers), sizeof(SSL_CIPHER),
              ssl_cipher_id_cmp_void));
}

uint32_t SSL_CIPHER_get_id(const SSL_CIPHER *cipher) { return cipher->id; }

uint16_t SSL_CIPHER_get_protocol_id(const SSL_CIPHER *cipher) {

  assert((cipher->id & 0xff000000) == 0x03000000);
  return static_cast<uint16_t>(cipher->id);
}

int SSL_CIPHER_is_aead(const SSL_CIPHER *cipher) {
  return (cipher->algorithm_mac & SSL_AEAD) != 0;
}

int SSL_CIPHER_get_cipher_nid(const SSL_CIPHER *cipher) {
  switch (cipher->algorithm_enc) {
    case SSL_3DES:
      return NID_des_ede3_cbc;
    case SSL_AES128:
      return NID_aes_128_cbc;
    case SSL_AES256:
      return NID_aes_256_cbc;
    case SSL_AES128GCM:
      return NID_aes_128_gcm;
    case SSL_AES256GCM:
      return NID_aes_256_gcm;
    case SSL_CHACHA20POLY1305:
      return NID_chacha20_poly1305;
  }
  assert(0);
  return NID_undef;
}

int SSL_CIPHER_get_digest_nid(const SSL_CIPHER *cipher) {
  switch (cipher->algorithm_mac) {
    case SSL_AEAD:
      return NID_undef;
    case SSL_SHA1:
      return NID_sha1;
    case SSL_SHA256:
      return NID_sha256;
  }
  assert(0);
  return NID_undef;
}

int SSL_CIPHER_get_kx_nid(const SSL_CIPHER *cipher) {
  switch (cipher->algorithm_mkey) {
    case SSL_kRSA:
      return NID_kx_rsa;
    case SSL_kECDHE:
      return NID_kx_ecdhe;
    case SSL_kPSK:
      return NID_kx_psk;
    case SSL_kGENERIC:
      return NID_kx_any;
  }
  assert(0);
  return NID_undef;
}

int SSL_CIPHER_get_auth_nid(const SSL_CIPHER *cipher) {
  switch (cipher->algorithm_auth) {
    case SSL_aRSA_DECRYPT:
    case SSL_aRSA_SIGN:
      return NID_auth_rsa;
    case SSL_aECDSA:
      return NID_auth_ecdsa;
    case SSL_aPSK:
      return NID_auth_psk;
    case SSL_aGENERIC:
      return NID_auth_any;
  }
  assert(0);
  return NID_undef;
}

const EVP_MD *SSL_CIPHER_get_handshake_digest(const SSL_CIPHER *cipher) {
  switch (cipher->algorithm_prf) {
    case SSL_HANDSHAKE_MAC_DEFAULT:
      return EVP_md5_sha1();
    case SSL_HANDSHAKE_MAC_SHA256:
      return EVP_sha256();
    case SSL_HANDSHAKE_MAC_SHA384:
      return EVP_sha384();
  }
  assert(0);
  return NULL;
}

int SSL_CIPHER_get_prf_nid(const SSL_CIPHER *cipher) {
  const EVP_MD *md = SSL_CIPHER_get_handshake_digest(cipher);
  if (md == NULL) {
    return NID_undef;
  }
  return EVP_MD_nid(md);
}

int SSL_CIPHER_is_block_cipher(const SSL_CIPHER *cipher) {
  return cipher->algorithm_mac != SSL_AEAD;
}

uint16_t SSL_CIPHER_get_min_version(const SSL_CIPHER *cipher) {
  if (cipher->algorithm_mkey == SSL_kGENERIC ||
      cipher->algorithm_auth == SSL_aGENERIC) {
    return TLS1_3_VERSION;
  }

  if (cipher->algorithm_prf != SSL_HANDSHAKE_MAC_DEFAULT) {

    return TLS1_2_VERSION;
  }
  return SSL3_VERSION;
}

uint16_t SSL_CIPHER_get_max_version(const SSL_CIPHER *cipher) {
  if (cipher->algorithm_mkey == SSL_kGENERIC ||
      cipher->algorithm_auth == SSL_aGENERIC) {
    return TLS1_3_VERSION;
  }
  return TLS1_2_VERSION;
}

static const char *kUnknownCipher = "(NONE)";

const char *SSL_CIPHER_get_name(const SSL_CIPHER *cipher) {
  if (cipher != NULL) {
    return cipher->name;
  }

  return kUnknownCipher;
}

const char *SSL_CIPHER_standard_name(const SSL_CIPHER *cipher) {
  return cipher->standard_name;
}

const char *SSL_CIPHER_get_kx_name(const SSL_CIPHER *cipher) {
  if (cipher == NULL) {
    return "";
  }

  switch (cipher->algorithm_mkey) {
    case SSL_kRSA:
      return "RSA";

    case SSL_kECDHE:
      switch (cipher->algorithm_auth) {
        case SSL_aECDSA:
          return "ECDHE_ECDSA";
        case SSL_aRSA_SIGN:
          return "ECDHE_RSA";
        case SSL_aPSK:
          return "ECDHE_PSK";
        default:
          assert(0);
          return "UNKNOWN";
      }

    case SSL_kPSK:
      assert(cipher->algorithm_auth == SSL_aPSK);
      return "PSK";

    case SSL_kGENERIC:
      assert(cipher->algorithm_auth == SSL_aGENERIC);
      return "GENERIC";

    default:
      assert(0);
      return "UNKNOWN";
  }
}

int SSL_CIPHER_get_bits(const SSL_CIPHER *cipher, int *out_alg_bits) {
  if (cipher == NULL) {
    return 0;
  }

  int alg_bits, strength_bits;
  switch (cipher->algorithm_enc) {
    case SSL_AES128:
    case SSL_AES128GCM:
      alg_bits = 128;
      strength_bits = 128;
      break;

    case SSL_AES256:
    case SSL_AES256GCM:
    case SSL_CHACHA20POLY1305:
      alg_bits = 256;
      strength_bits = 256;
      break;

    case SSL_3DES:
      alg_bits = 168;
      strength_bits = 112;
      break;

    default:
      assert(0);
      alg_bits = 0;
      strength_bits = 0;
  }

  if (out_alg_bits != NULL) {
    *out_alg_bits = alg_bits;
  }
  return strength_bits;
}

const char *SSL_CIPHER_description(const SSL_CIPHER *cipher, char *buf,
                                   int len) {
  const char *kx, *au, *enc, *mac;
  uint32_t alg_mkey, alg_auth, alg_enc, alg_mac;

  alg_mkey = cipher->algorithm_mkey;
  alg_auth = cipher->algorithm_auth;
  alg_enc = cipher->algorithm_enc;
  alg_mac = cipher->algorithm_mac;

  switch (alg_mkey) {
    case SSL_kRSA:
      kx = "RSA";
      break;

    case SSL_kECDHE:
      kx = "ECDH";
      break;

    case SSL_kPSK:
      kx = "PSK";
      break;

    case SSL_kGENERIC:
      kx = "GENERIC";
      break;

    default:
      kx = "unknown";
  }

  switch (alg_auth) {
    case SSL_aRSA_DECRYPT:
    case SSL_aRSA_SIGN:
      au = "RSA";
      break;

    case SSL_aECDSA:
      au = "ECDSA";
      break;

    case SSL_aPSK:
      au = "PSK";
      break;

    case SSL_aGENERIC:
      au = "GENERIC";
      break;

    default:
      au = "unknown";
      break;
  }

  switch (alg_enc) {
    case SSL_3DES:
      enc = "3DES(168)";
      break;

    case SSL_AES128:
      enc = "AES(128)";
      break;

    case SSL_AES256:
      enc = "AES(256)";
      break;

    case SSL_AES128GCM:
      enc = "AESGCM(128)";
      break;

    case SSL_AES256GCM:
      enc = "AESGCM(256)";
      break;

    case SSL_CHACHA20POLY1305:
      enc = "ChaCha20-Poly1305";
      break;

    default:
      enc = "unknown";
      break;
  }

  switch (alg_mac) {
    case SSL_SHA1:
      mac = "SHA1";
      break;

    case SSL_SHA256:
      mac = "SHA256";
      break;

    case SSL_AEAD:
      mac = "AEAD";
      break;

    default:
      mac = "unknown";
      break;
  }

  if (buf == NULL) {
    len = 128;
    buf = (char *)OPENSSL_malloc(len);
    if (buf == NULL) {
      return NULL;
    }
  } else if (len < 128) {
    return "Buffer too small";
  }

  snprintf(buf, len, "%-23s Kx=%-8s Au=%-4s Enc=%-9s Mac=%-4s\n", cipher->name,
           kx, au, enc, mac);
  return buf;
}

const char *SSL_CIPHER_get_version(const SSL_CIPHER *cipher) {
  return "TLSv1/SSLv3";
}

STACK_OF(SSL_COMP) *SSL_COMP_get_compression_methods(void) { return NULL; }

int SSL_COMP_add_compression_method(int id, COMP_METHOD *cm) { return 1; }

const char *SSL_COMP_get_name(const COMP_METHOD *comp) { return NULL; }

const char *SSL_COMP_get0_name(const SSL_COMP *comp) { return comp->name; }

int SSL_COMP_get_id(const SSL_COMP *comp) { return comp->id; }

void SSL_COMP_free_compression_methods(void) {}

size_t SSL_get_all_cipher_names(const char **out, size_t max_out) {
  return GetAllNames(out, max_out, Span(&kUnknownCipher, 1), &SSL_CIPHER::name,
                     Span(kCiphers));
}

size_t SSL_get_all_standard_cipher_names(const char **out, size_t max_out) {
  return GetAllNames(out, max_out, Span<const char *>(),
                     &SSL_CIPHER::standard_name, Span(kCiphers));
}
