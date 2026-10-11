// Copyright 1999-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <openssl/pkcs8.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <openssl/bytestring.h>
#include <openssl/cipher.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/nid.h>
#include <openssl/rand.h>

#include "../bytestring/internal.h"
#include "../internal.h"
#include "internal.h"

static int pkcs12_encode_password(const char *in, size_t in_len, uint8_t **out,
                                  size_t *out_len) {
  CBB cbb;
  if (!CBB_init(&cbb, in_len * 2)) {
    return 0;
  }

  CBS cbs;
  CBS_init(&cbs, (const uint8_t *)in, in_len);
  while (CBS_len(&cbs) != 0) {
    uint32_t c;
    if (!CBS_get_utf8(&cbs, &c) || !CBB_add_ucs2_be(&cbb, c)) {
      OPENSSL_PUT_ERROR(PKCS8, PKCS8_R_INVALID_CHARACTERS);
      goto err;
    }
  }

  if (!CBB_add_ucs2_be(&cbb, 0) || !CBB_finish(&cbb, out, out_len)) {
    goto err;
  }

  return 1;

err:
  CBB_cleanup(&cbb);
  return 0;
}

int pkcs12_key_gen(const char *pass, size_t pass_len, const uint8_t *salt,
                   size_t salt_len, uint8_t id, uint32_t iterations,
                   size_t out_len, uint8_t *out, const EVP_MD *md) {

  if (iterations < 1) {
    OPENSSL_PUT_ERROR(PKCS8, PKCS8_R_BAD_ITERATION_COUNT);
    return 0;
  }

  int ret = 0;
  EVP_MD_CTX ctx;
  EVP_MD_CTX_init(&ctx);
  uint8_t *pass_raw = NULL, *I = NULL;
  size_t pass_raw_len = 0, I_len = 0;

  {

    if (pass != NULL &&
        !pkcs12_encode_password(pass, pass_len, &pass_raw, &pass_raw_len)) {
      goto err;
    }

    size_t block_size = EVP_MD_block_size(md);

    uint8_t D[EVP_MAX_MD_BLOCK_SIZE];
    OPENSSL_memset(D, id, block_size);

    if (salt_len + block_size - 1 < salt_len ||
        pass_raw_len + block_size - 1 < pass_raw_len) {
      OPENSSL_PUT_ERROR(PKCS8, ERR_R_OVERFLOW);
      goto err;
    }
    size_t S_len = block_size * ((salt_len + block_size - 1) / block_size);
    size_t P_len = block_size * ((pass_raw_len + block_size - 1) / block_size);
    I_len = S_len + P_len;
    if (I_len < S_len) {
      OPENSSL_PUT_ERROR(PKCS8, ERR_R_OVERFLOW);
      goto err;
    }

    I = reinterpret_cast<uint8_t *>(OPENSSL_malloc(I_len));
    if (I_len != 0 && I == NULL) {
      goto err;
    }

    for (size_t i = 0; i < S_len; i++) {
      I[i] = salt[i % salt_len];
    }
    for (size_t i = 0; i < P_len; i++) {
      I[i + S_len] = pass_raw[i % pass_raw_len];
    }

    while (out_len != 0) {

      uint8_t A[EVP_MAX_MD_SIZE];
      unsigned A_len;
      if (!EVP_DigestInit_ex(&ctx, md, NULL) ||
          !EVP_DigestUpdate(&ctx, D, block_size) ||
          !EVP_DigestUpdate(&ctx, I, I_len) ||
          !EVP_DigestFinal_ex(&ctx, A, &A_len)) {
        goto err;
      }
      for (uint32_t iter = 1; iter < iterations; iter++) {
        if (!EVP_DigestInit_ex(&ctx, md, NULL) ||
            !EVP_DigestUpdate(&ctx, A, A_len) ||
            !EVP_DigestFinal_ex(&ctx, A, &A_len)) {
          goto err;
        }
      }

      size_t todo = out_len < A_len ? out_len : A_len;
      OPENSSL_memcpy(out, A, todo);
      out += todo;
      out_len -= todo;
      if (out_len == 0) {
        break;
      }

      uint8_t B[EVP_MAX_MD_BLOCK_SIZE];
      for (size_t i = 0; i < block_size; i++) {
        B[i] = A[i % A_len];
      }

      assert(I_len % block_size == 0);
      for (size_t i = 0; i < I_len; i += block_size) {
        unsigned carry = 1;
        for (size_t j = block_size - 1; j < block_size; j--) {
          carry += I[i + j] + B[j];
          I[i + j] = (uint8_t)carry;
          carry >>= 8;
        }
      }
    }

    ret = 1;
  }

err:
  OPENSSL_free(I);
  OPENSSL_free(pass_raw);
  EVP_MD_CTX_cleanup(&ctx);
  return ret;
}

static int pkcs12_pbe_cipher_init(const struct pbe_suite *suite,
                                  EVP_CIPHER_CTX *ctx, uint32_t iterations,
                                  const char *pass, size_t pass_len,
                                  const uint8_t *salt, size_t salt_len,
                                  int is_encrypt) {
  const EVP_CIPHER *cipher = suite->cipher_func();
  const EVP_MD *md = suite->md_func();

  uint8_t key[EVP_MAX_KEY_LENGTH];
  uint8_t iv[EVP_MAX_IV_LENGTH];
  if (!pkcs12_key_gen(pass, pass_len, salt, salt_len, PKCS12_KEY_ID, iterations,
                      EVP_CIPHER_key_length(cipher), key, md) ||
      !pkcs12_key_gen(pass, pass_len, salt, salt_len, PKCS12_IV_ID, iterations,
                      EVP_CIPHER_iv_length(cipher), iv, md)) {
    OPENSSL_PUT_ERROR(PKCS8, PKCS8_R_KEY_GEN_ERROR);
    return 0;
  }

  int ret = EVP_CipherInit_ex(ctx, cipher, NULL, key, iv, is_encrypt);
  OPENSSL_cleanse(key, EVP_MAX_KEY_LENGTH);
  OPENSSL_cleanse(iv, EVP_MAX_IV_LENGTH);
  return ret;
}

static int pkcs12_pbe_decrypt_init(const struct pbe_suite *suite,
                                   EVP_CIPHER_CTX *ctx, const char *pass,
                                   size_t pass_len, CBS *param) {
  CBS pbe_param, salt;
  uint64_t iterations;
  if (!CBS_get_asn1(param, &pbe_param, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1(&pbe_param, &salt, CBS_ASN1_OCTETSTRING) ||
      !CBS_get_asn1_uint64(&pbe_param, &iterations) ||
      CBS_len(&pbe_param) != 0 || CBS_len(param) != 0) {
    OPENSSL_PUT_ERROR(PKCS8, PKCS8_R_DECODE_ERROR);
    return 0;
  }

  if (!pkcs12_iterations_acceptable(iterations)) {
    OPENSSL_PUT_ERROR(PKCS8, PKCS8_R_BAD_ITERATION_COUNT);
    return 0;
  }

  return pkcs12_pbe_cipher_init(suite, ctx, (uint32_t)iterations, pass,
                                pass_len, CBS_data(&salt), CBS_len(&salt),
                                0              );
}

static const struct pbe_suite kBuiltinPBE[] = {
    {
        NID_pbe_WithSHA1And40BitRC2_CBC,

        {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x01, 0x06},
        10,
        EVP_rc2_40_cbc,
        EVP_sha1,
        pkcs12_pbe_decrypt_init,
    },
    {
        NID_pbe_WithSHA1And128BitRC4,

        {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x01, 0x01},
        10,
        EVP_rc4,
        EVP_sha1,
        pkcs12_pbe_decrypt_init,
    },
    {
        NID_pbe_WithSHA1And3_Key_TripleDES_CBC,

        {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x0c, 0x01, 0x03},
        10,
        EVP_des_ede3_cbc,
        EVP_sha1,
        pkcs12_pbe_decrypt_init,
    },
    {
        NID_pbes2,

        {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x05, 0x0d},
        9,
        NULL,
        NULL,
        PKCS5_pbe2_decrypt_init,
    },
};

static const struct pbe_suite *get_pkcs12_pbe_suite(int pbe_nid) {
  for (unsigned i = 0; i < OPENSSL_ARRAY_SIZE(kBuiltinPBE); i++) {
    if (kBuiltinPBE[i].pbe_nid == pbe_nid &&

        kBuiltinPBE[i].cipher_func != NULL && kBuiltinPBE[i].md_func != NULL) {
      return &kBuiltinPBE[i];
    }
  }

  return NULL;
}

int pkcs12_pbe_encrypt_init(CBB *out, EVP_CIPHER_CTX *ctx, int alg_nid,
                            const EVP_CIPHER *alg_cipher, uint32_t iterations,
                            const char *pass, size_t pass_len,
                            const uint8_t *salt, size_t salt_len) {

  if (alg_nid == -1) {
    return PKCS5_pbe2_encrypt_init(out, ctx, alg_cipher, iterations, pass,
                                   pass_len, salt, salt_len);
  }

  const struct pbe_suite *suite = get_pkcs12_pbe_suite(alg_nid);
  if (suite == NULL) {
    OPENSSL_PUT_ERROR(PKCS8, PKCS8_R_UNKNOWN_ALGORITHM);
    return 0;
  }

  CBB algorithm, oid, param, salt_cbb;
  if (!CBB_add_asn1(out, &algorithm, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&algorithm, &oid, CBS_ASN1_OBJECT) ||
      !CBB_add_bytes(&oid, suite->oid, suite->oid_len) ||
      !CBB_add_asn1(&algorithm, &param, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&param, &salt_cbb, CBS_ASN1_OCTETSTRING) ||
      !CBB_add_bytes(&salt_cbb, salt, salt_len) ||
      !CBB_add_asn1_uint64(&param, iterations) || !CBB_flush(out)) {
    return 0;
  }

  return pkcs12_pbe_cipher_init(suite, ctx, iterations, pass, pass_len, salt,
                                salt_len, 1              );
}

int pkcs8_pbe_decrypt(uint8_t **out, size_t *out_len, CBS *algorithm,
                      const char *pass, size_t pass_len, const uint8_t *in,
                      size_t in_len) {
  int ret = 0;
  uint8_t *buf = NULL;
  ;
  EVP_CIPHER_CTX ctx;
  EVP_CIPHER_CTX_init(&ctx);

  CBS obj;
  const struct pbe_suite *suite = NULL;
  if (!CBS_get_asn1(algorithm, &obj, CBS_ASN1_OBJECT)) {
    OPENSSL_PUT_ERROR(PKCS8, PKCS8_R_DECODE_ERROR);
    goto err;
  }

  for (unsigned i = 0; i < OPENSSL_ARRAY_SIZE(kBuiltinPBE); i++) {
    if (CBS_mem_equal(&obj, kBuiltinPBE[i].oid, kBuiltinPBE[i].oid_len)) {
      suite = &kBuiltinPBE[i];
      break;
    }
  }
  if (suite == NULL) {
    OPENSSL_PUT_ERROR(PKCS8, PKCS8_R_UNKNOWN_ALGORITHM);
    goto err;
  }

  if (!suite->decrypt_init(suite, &ctx, pass, pass_len, algorithm)) {
    OPENSSL_PUT_ERROR(PKCS8, PKCS8_R_KEYGEN_FAILURE);
    goto err;
  }

  buf = reinterpret_cast<uint8_t *>(OPENSSL_malloc(in_len));
  if (buf == NULL) {
    goto err;
  }

  if (in_len > INT_MAX) {
    OPENSSL_PUT_ERROR(PKCS8, ERR_R_OVERFLOW);
    goto err;
  }

  int n1, n2;
  if (!EVP_DecryptUpdate(&ctx, buf, &n1, in, (int)in_len) ||
      !EVP_DecryptFinal_ex(&ctx, buf + n1, &n2)) {
    goto err;
  }

  *out = buf;
  *out_len = n1 + n2;
  ret = 1;
  buf = NULL;

err:
  OPENSSL_free(buf);
  EVP_CIPHER_CTX_cleanup(&ctx);
  return ret;
}

EVP_PKEY *PKCS8_parse_encrypted_private_key(CBS *cbs, const char *pass,
                                            size_t pass_len) {

  CBS epki, algorithm, ciphertext;
  if (!CBS_get_asn1(cbs, &epki, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1(&epki, &algorithm, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1(&epki, &ciphertext, CBS_ASN1_OCTETSTRING) ||
      CBS_len(&epki) != 0) {
    OPENSSL_PUT_ERROR(PKCS8, PKCS8_R_DECODE_ERROR);
    return 0;
  }

  uint8_t *out;
  size_t out_len;
  if (!pkcs8_pbe_decrypt(&out, &out_len, &algorithm, pass, pass_len,
                         CBS_data(&ciphertext), CBS_len(&ciphertext))) {
    return 0;
  }

  CBS pki;
  CBS_init(&pki, out, out_len);
  EVP_PKEY *ret = EVP_parse_private_key(&pki);
  OPENSSL_free(out);
  return ret;
}

int PKCS8_marshal_encrypted_private_key(CBB *out, int pbe_nid,
                                        const EVP_CIPHER *cipher,
                                        const char *pass, size_t pass_len,
                                        const uint8_t *salt, size_t salt_len,
                                        int iterations, const EVP_PKEY *pkey) {
  int ret = 0;
  uint8_t *plaintext = NULL, *salt_buf = NULL;
  size_t plaintext_len = 0;
  EVP_CIPHER_CTX ctx;
  EVP_CIPHER_CTX_init(&ctx);

  {

    if (salt == NULL) {
      if (salt_len == 0) {
        salt_len = PKCS5_SALT_LEN;
      }

      salt_buf = reinterpret_cast<uint8_t *>(OPENSSL_malloc(salt_len));
      if (salt_buf == NULL || !RAND_bytes(salt_buf, salt_len)) {
        goto err;
      }

      salt = salt_buf;
    }

    if (iterations <= 0) {
      iterations = PKCS12_DEFAULT_ITER;
    }

    CBB plaintext_cbb;
    if (!CBB_init(&plaintext_cbb, 128) ||
        !EVP_marshal_private_key(&plaintext_cbb, pkey) ||
        !CBB_finish(&plaintext_cbb, &plaintext, &plaintext_len)) {
      CBB_cleanup(&plaintext_cbb);
      goto err;
    }

    CBB epki;
    if (!CBB_add_asn1(out, &epki, CBS_ASN1_SEQUENCE) ||
        !pkcs12_pbe_encrypt_init(&epki, &ctx, pbe_nid, cipher,
                                 (uint32_t)iterations, pass, pass_len, salt,
                                 salt_len)) {
      goto err;
    }

    size_t max_out = plaintext_len + EVP_CIPHER_CTX_block_size(&ctx);
    if (max_out < plaintext_len) {
      OPENSSL_PUT_ERROR(PKCS8, PKCS8_R_TOO_LONG);
      goto err;
    }

    CBB ciphertext;
    uint8_t *ptr;
    int n1, n2;
    if (!CBB_add_asn1(&epki, &ciphertext, CBS_ASN1_OCTETSTRING) ||
        !CBB_reserve(&ciphertext, &ptr, max_out) ||
        !EVP_CipherUpdate(&ctx, ptr, &n1, plaintext, plaintext_len) ||
        !EVP_CipherFinal_ex(&ctx, ptr + n1, &n2) ||
        !CBB_did_write(&ciphertext, n1 + n2) || !CBB_flush(out)) {
      goto err;
    }

    ret = 1;
  }

err:
  OPENSSL_free(plaintext);
  OPENSSL_free(salt_buf);
  EVP_CIPHER_CTX_cleanup(&ctx);
  return ret;
}
