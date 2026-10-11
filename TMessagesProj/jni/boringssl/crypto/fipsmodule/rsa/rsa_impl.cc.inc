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

#include <openssl/rsa.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/thread.h>

#include "../../bcm_support.h"
#include "../../internal.h"
#include "../bn/internal.h"
#include "../delocate.h"
#include "../service_indicator/internal.h"
#include "internal.h"

static_assert(OPENSSL_RSA_MAX_MODULUS_BITS <=
                  BN_MONTGOMERY_MAX_WORDS * BN_BITS2,
              "Max RSA size too big for Montgomery arithmetic");

int rsa_check_public_key(const RSA *rsa) {
  if (rsa->n == NULL) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_VALUE_MISSING);
    return 0;
  }

  unsigned n_bits = BN_num_bits(rsa->n);
  if (n_bits > OPENSSL_RSA_MAX_MODULUS_BITS) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_MODULUS_TOO_LARGE);
    return 0;
  }

  if (n_bits < 512) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_KEY_SIZE_TOO_SMALL);
    return 0;
  }

  if (!BN_is_odd(rsa->n) || BN_is_negative(rsa->n)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_RSA_PARAMETERS);
    return 0;
  }

  static const unsigned kMaxExponentBits = 33;
  if (rsa->e != NULL) {

    unsigned e_bits = BN_num_bits(rsa->e);
    if (e_bits < 2 || BN_is_negative(rsa->e) || !BN_is_odd(rsa->e)) {
      OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_E_VALUE);
      return 0;
    }
    if (rsa->flags & RSA_FLAG_LARGE_PUBLIC_EXPONENT) {

      if (BN_ucmp(rsa->n, rsa->e) <= 0) {
        OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_E_VALUE);
        return 0;
      }
    } else {

      if (e_bits > kMaxExponentBits) {
        OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_E_VALUE);
        return 0;
      }

      assert(BN_ucmp(rsa->n, rsa->e) > 0);
    }
  } else if (!(rsa->flags & RSA_FLAG_NO_PUBLIC_EXPONENT)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_VALUE_MISSING);
    return 0;
  }

  return 1;
}

static int ensure_fixed_copy(BIGNUM **out, const BIGNUM *in, int width) {
  if (*out != NULL) {
    return 1;
  }
  BIGNUM *copy = BN_dup(in);
  if (copy == NULL || !bn_resize_words(copy, width)) {
    BN_free(copy);
    return 0;
  }
  *out = copy;
  bn_secret(copy);

  return 1;
}

static int freeze_private_key(RSA *rsa, BN_CTX *ctx) {
  CRYPTO_MUTEX_lock_read(&rsa->lock);
  int frozen = rsa->private_key_frozen;
  CRYPTO_MUTEX_unlock_read(&rsa->lock);
  if (frozen) {
    return 1;
  }

  int ret = 0;
  const BIGNUM *n_fixed;
  CRYPTO_MUTEX_lock_write(&rsa->lock);
  if (rsa->private_key_frozen) {
    ret = 1;
    goto err;
  }

  if (!rsa_check_public_key(rsa)) {
    goto err;
  }

  if (rsa->mont_n == NULL) {
    rsa->mont_n = BN_MONT_CTX_new_for_modulus(rsa->n, ctx);
    if (rsa->mont_n == NULL) {
      goto err;
    }
  }
  n_fixed = &rsa->mont_n->N;

  if (rsa->d != NULL &&
      !ensure_fixed_copy(&rsa->d_fixed, rsa->d, n_fixed->width)) {
    goto err;
  }

  if (rsa->e != NULL && rsa->p != NULL && rsa->q != NULL) {

    if (rsa->mont_p == NULL) {
      rsa->mont_p = BN_MONT_CTX_new_consttime(rsa->p, ctx);
      if (rsa->mont_p == NULL) {
        goto err;
      }
    }

    if (rsa->mont_q == NULL) {
      rsa->mont_q = BN_MONT_CTX_new_consttime(rsa->q, ctx);
      if (rsa->mont_q == NULL) {
        goto err;
      }
    }

    if (rsa->dmp1 != NULL && rsa->dmq1 != NULL && rsa->iqmp != NULL) {

      const BIGNUM *p_fixed = &rsa->mont_p->N;
      const BIGNUM *q_fixed = &rsa->mont_q->N;
      if (!ensure_fixed_copy(&rsa->dmp1_fixed, rsa->dmp1, p_fixed->width) ||
          !ensure_fixed_copy(&rsa->dmq1_fixed, rsa->dmq1, q_fixed->width)) {
        goto err;
      }

      if (rsa->iqmp_mont == NULL) {
        BIGNUM *iqmp_mont = BN_new();
        if (iqmp_mont == NULL ||
            !BN_to_montgomery(iqmp_mont, rsa->iqmp, rsa->mont_p, ctx)) {
          BN_free(iqmp_mont);
          goto err;
        }
        rsa->iqmp_mont = iqmp_mont;
        bn_secret(rsa->iqmp_mont);
      }
    }
  }

  rsa->private_key_frozen = 1;
  ret = 1;

err:
  CRYPTO_MUTEX_unlock_write(&rsa->lock);
  return ret;
}

void rsa_invalidate_key(RSA *rsa) {
  rsa->private_key_frozen = 0;

  BN_MONT_CTX_free(rsa->mont_n);
  rsa->mont_n = NULL;
  BN_MONT_CTX_free(rsa->mont_p);
  rsa->mont_p = NULL;
  BN_MONT_CTX_free(rsa->mont_q);
  rsa->mont_q = NULL;

  BN_free(rsa->d_fixed);
  rsa->d_fixed = NULL;
  BN_free(rsa->dmp1_fixed);
  rsa->dmp1_fixed = NULL;
  BN_free(rsa->dmq1_fixed);
  rsa->dmq1_fixed = NULL;
  BN_free(rsa->iqmp_mont);
  rsa->iqmp_mont = NULL;

  for (size_t i = 0; i < rsa->num_blindings; i++) {
    BN_BLINDING_free(rsa->blindings[i]);
  }
  OPENSSL_free(rsa->blindings);
  rsa->blindings = NULL;
  rsa->num_blindings = 0;
  OPENSSL_free(rsa->blindings_inuse);
  rsa->blindings_inuse = NULL;
  rsa->blinding_fork_generation = 0;
}

#if defined(OPENSSL_TSAN)

#define MAX_BLINDINGS_PER_RSA 2
#else
#define MAX_BLINDINGS_PER_RSA 1024
#endif

static BN_BLINDING *rsa_blinding_get(RSA *rsa, size_t *index_used,
                                     BN_CTX *ctx) {
  assert(ctx != NULL);
  assert(rsa->mont_n != NULL);

  BN_BLINDING *ret = NULL;
  const uint64_t fork_generation = CRYPTO_get_fork_generation();
  CRYPTO_MUTEX_lock_write(&rsa->lock);

  if (rsa->blinding_fork_generation != fork_generation) {
    for (size_t i = 0; i < rsa->num_blindings; i++) {

      assert(rsa->blindings_inuse[i] == 0);
      BN_BLINDING_invalidate(rsa->blindings[i]);
    }
    rsa->blinding_fork_generation = fork_generation;
  }

  uint8_t *const free_inuse_flag = reinterpret_cast<uint8_t *>(
      OPENSSL_memchr(rsa->blindings_inuse, 0, rsa->num_blindings));
  size_t new_num_blindings;
  BN_BLINDING **new_blindings;
  uint8_t *new_blindings_inuse;
  if (free_inuse_flag != NULL) {
    *free_inuse_flag = 1;
    *index_used = free_inuse_flag - rsa->blindings_inuse;
    ret = rsa->blindings[*index_used];
    goto out;
  }

  if (rsa->num_blindings >= MAX_BLINDINGS_PER_RSA) {

    *index_used = MAX_BLINDINGS_PER_RSA;
    ret = BN_BLINDING_new();
    goto out;
  }

  static_assert(MAX_BLINDINGS_PER_RSA < UINT_MAX / 2,
                "MAX_BLINDINGS_PER_RSA too large");
  new_num_blindings = rsa->num_blindings * 2;
  if (new_num_blindings == 0) {
    new_num_blindings = 1;
  }
  if (new_num_blindings > MAX_BLINDINGS_PER_RSA) {
    new_num_blindings = MAX_BLINDINGS_PER_RSA;
  }
  assert(new_num_blindings > rsa->num_blindings);

  new_blindings = reinterpret_cast<BN_BLINDING **>(
      OPENSSL_calloc(new_num_blindings, sizeof(BN_BLINDING *)));
  new_blindings_inuse =
      reinterpret_cast<uint8_t *>(OPENSSL_malloc(new_num_blindings));
  if (new_blindings == NULL || new_blindings_inuse == NULL) {
    goto err;
  }

  OPENSSL_memcpy(new_blindings, rsa->blindings,
                 sizeof(BN_BLINDING *) * rsa->num_blindings);
  OPENSSL_memcpy(new_blindings_inuse, rsa->blindings_inuse, rsa->num_blindings);

  for (size_t i = rsa->num_blindings; i < new_num_blindings; i++) {
    new_blindings[i] = BN_BLINDING_new();
    if (new_blindings[i] == NULL) {
      for (size_t j = rsa->num_blindings; j < i; j++) {
        BN_BLINDING_free(new_blindings[j]);
      }
      goto err;
    }
  }
  memset(&new_blindings_inuse[rsa->num_blindings], 0,
         new_num_blindings - rsa->num_blindings);

  new_blindings_inuse[rsa->num_blindings] = 1;
  *index_used = rsa->num_blindings;
  assert(*index_used != MAX_BLINDINGS_PER_RSA);
  ret = new_blindings[rsa->num_blindings];

  OPENSSL_free(rsa->blindings);
  rsa->blindings = new_blindings;
  OPENSSL_free(rsa->blindings_inuse);
  rsa->blindings_inuse = new_blindings_inuse;
  rsa->num_blindings = new_num_blindings;

  goto out;

err:
  OPENSSL_free(new_blindings_inuse);
  OPENSSL_free(new_blindings);

out:
  CRYPTO_MUTEX_unlock_write(&rsa->lock);
  return ret;
}

static void rsa_blinding_release(RSA *rsa, BN_BLINDING *blinding,
                                 size_t blinding_index) {
  if (blinding_index == MAX_BLINDINGS_PER_RSA) {

    BN_BLINDING_free(blinding);
    return;
  }

  CRYPTO_MUTEX_lock_write(&rsa->lock);
  rsa->blindings_inuse[blinding_index] = 0;
  CRYPTO_MUTEX_unlock_write(&rsa->lock);
}

int rsa_default_sign_raw(RSA *rsa, size_t *out_len, uint8_t *out,
                         size_t max_out, const uint8_t *in, size_t in_len,
                         int padding) {
  const unsigned rsa_size = RSA_size(rsa);
  uint8_t *buf = NULL;
  int i, ret = 0;

  if (max_out < rsa_size) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_OUTPUT_BUFFER_TOO_SMALL);
    return 0;
  }

  buf = reinterpret_cast<uint8_t *>(OPENSSL_malloc(rsa_size));
  if (buf == NULL) {
    goto err;
  }

  switch (padding) {
    case RSA_PKCS1_PADDING:
      i = RSA_padding_add_PKCS1_type_1(buf, rsa_size, in, in_len);
      break;
    case RSA_NO_PADDING:
      i = RSA_padding_add_none(buf, rsa_size, in, in_len);
      break;
    default:
      OPENSSL_PUT_ERROR(RSA, RSA_R_UNKNOWN_PADDING_TYPE);
      goto err;
  }

  if (i <= 0) {
    goto err;
  }

  if (!rsa_private_transform_no_self_test(rsa, out, buf, rsa_size)) {
    goto err;
  }

  CONSTTIME_DECLASSIFY(out, rsa_size);
  *out_len = rsa_size;
  ret = 1;

err:
  OPENSSL_free(buf);

  return ret;
}

static int rsa_mod_exp_crt(BIGNUM *r0, const BIGNUM *I, RSA *rsa, BN_CTX *ctx);

int rsa_verify_raw_no_self_test(RSA *rsa, size_t *out_len, uint8_t *out,
                                size_t max_out, const uint8_t *in,
                                size_t in_len, int padding) {
  if (rsa->n == NULL || rsa->e == NULL) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_VALUE_MISSING);
    return 0;
  }

  if (!rsa_check_public_key(rsa)) {
    return 0;
  }

  const unsigned rsa_size = RSA_size(rsa);
  BIGNUM *f, *result;

  if (max_out < rsa_size) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_OUTPUT_BUFFER_TOO_SMALL);
    return 0;
  }

  if (in_len != rsa_size) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_LEN_NOT_EQUAL_TO_MOD_LEN);
    return 0;
  }

  BN_CTX *ctx = BN_CTX_new();
  if (ctx == NULL) {
    return 0;
  }

  int ret = 0;
  uint8_t *buf = NULL;

  BN_CTX_start(ctx);
  f = BN_CTX_get(ctx);
  result = BN_CTX_get(ctx);
  if (f == NULL || result == NULL) {
    goto err;
  }

  if (padding == RSA_NO_PADDING) {
    buf = out;
  } else {

    buf = reinterpret_cast<uint8_t *>(OPENSSL_malloc(rsa_size));
    if (buf == NULL) {
      goto err;
    }
  }

  if (BN_bin2bn(in, in_len, f) == NULL) {
    goto err;
  }

  if (BN_ucmp(f, rsa->n) >= 0) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_LARGE_FOR_MODULUS);
    goto err;
  }

  if (!BN_MONT_CTX_set_locked(&rsa->mont_n, &rsa->lock, rsa->n, ctx) ||
      !BN_mod_exp_mont(result, f, rsa->e, &rsa->mont_n->N, ctx, rsa->mont_n)) {
    goto err;
  }

  if (!BN_bn2bin_padded(buf, rsa_size, result)) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
    goto err;
  }

  switch (padding) {
    case RSA_PKCS1_PADDING:
      ret =
          RSA_padding_check_PKCS1_type_1(out, out_len, rsa_size, buf, rsa_size);
      break;
    case RSA_NO_PADDING:
      ret = 1;
      *out_len = rsa_size;
      break;
    default:
      OPENSSL_PUT_ERROR(RSA, RSA_R_UNKNOWN_PADDING_TYPE);
      goto err;
  }

  if (!ret) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_PADDING_CHECK_FAILED);
    goto err;
  }

err:
  BN_CTX_end(ctx);
  BN_CTX_free(ctx);
  if (buf != out) {
    OPENSSL_free(buf);
  }
  return ret;
}

int RSA_verify_raw(RSA *rsa, size_t *out_len, uint8_t *out, size_t max_out,
                   const uint8_t *in, size_t in_len, int padding) {
  boringssl_ensure_rsa_self_test();
  return rsa_verify_raw_no_self_test(rsa, out_len, out, max_out, in, in_len,
                                     padding);
}

int rsa_default_private_transform(RSA *rsa, uint8_t *out, const uint8_t *in,
                                  size_t len) {
  if (rsa->n == NULL || rsa->d == NULL) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_VALUE_MISSING);
    return 0;
  }

  BIGNUM *f, *result;
  BN_CTX *ctx = NULL;
  size_t blinding_index = 0;
  BN_BLINDING *blinding = NULL;
  int ret = 0, do_blinding;

  ctx = BN_CTX_new();
  if (ctx == NULL) {
    goto err;
  }
  BN_CTX_start(ctx);
  f = BN_CTX_get(ctx);
  result = BN_CTX_get(ctx);

  if (f == NULL || result == NULL) {
    goto err;
  }

  assert(len == BN_num_bytes(rsa->n));
  if (BN_bin2bn(in, len, f) == NULL) {
    goto err;
  }

  if (constant_time_declassify_int(BN_ucmp(f, rsa->n) >= 0)) {

    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_LARGE_FOR_MODULUS);
    goto err;
  }

  if (!freeze_private_key(rsa, ctx)) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
    goto err;
  }

  do_blinding =
      (rsa->flags & (RSA_FLAG_NO_BLINDING | RSA_FLAG_NO_PUBLIC_EXPONENT)) == 0;

  if (rsa->e == NULL && do_blinding) {

    OPENSSL_PUT_ERROR(RSA, RSA_R_NO_PUBLIC_EXPONENT);
    goto err;
  }

  if (do_blinding) {
    blinding = rsa_blinding_get(rsa, &blinding_index, ctx);
    if (blinding == NULL) {
      OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
      goto err;
    }
    if (!BN_BLINDING_convert(f, blinding, rsa->e, rsa->mont_n, ctx)) {
      goto err;
    }
  }

  if (rsa->p != NULL && rsa->q != NULL && rsa->e != NULL && rsa->dmp1 != NULL &&
      rsa->dmq1 != NULL && rsa->iqmp != NULL &&

      bn_less_than_montgomery_R(rsa->q, rsa->mont_p) &&
      bn_less_than_montgomery_R(rsa->p, rsa->mont_q)) {
    if (!rsa_mod_exp_crt(result, f, rsa, ctx)) {
      goto err;
    }
  } else if (!BN_mod_exp_mont_consttime(result, f, rsa->d_fixed, rsa->n, ctx,
                                        rsa->mont_n)) {
    goto err;
  }

  if (rsa->e != NULL) {
    BIGNUM *vrfy = BN_CTX_get(ctx);
    if (vrfy == NULL ||
        !BN_mod_exp_mont(vrfy, result, rsa->e, rsa->n, ctx, rsa->mont_n) ||
        !constant_time_declassify_int(BN_equal_consttime(vrfy, f))) {
      OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
      goto err;
    }
  }

  if (do_blinding && !BN_BLINDING_invert(result, blinding, rsa->mont_n, ctx)) {
    goto err;
  }

  assert(result->width == rsa->mont_n->N.width);
  bn_assert_fits_in_bytes(result, len);
  if (!BN_bn2bin_padded(out, len, result)) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
    goto err;
  }

  ret = 1;

err:
  if (ctx != NULL) {
    BN_CTX_end(ctx);
    BN_CTX_free(ctx);
  }
  if (blinding != NULL) {
    rsa_blinding_release(rsa, blinding, blinding_index);
  }

  return ret;
}

static int mod_montgomery(BIGNUM *r, const BIGNUM *I, const BIGNUM *p,
                          const BN_MONT_CTX *mont_p, const BIGNUM *q,
                          BN_CTX *ctx) {

  if (!bn_less_than_montgomery_R(q, mont_p)) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  if (
      !BN_from_montgomery(r, I, mont_p, ctx) ||

      !BN_to_montgomery(r, r, mont_p, ctx)) {
    return 0;
  }

  return 1;
}

static int rsa_mod_exp_crt(BIGNUM *r0, const BIGNUM *I, RSA *rsa, BN_CTX *ctx) {
  assert(ctx != NULL);

  assert(rsa->n != NULL);
  assert(rsa->e != NULL);
  assert(rsa->d != NULL);
  assert(rsa->p != NULL);
  assert(rsa->q != NULL);
  assert(rsa->dmp1 != NULL);
  assert(rsa->dmq1 != NULL);
  assert(rsa->iqmp != NULL);

  BIGNUM *r1, *m1;
  int ret = 0;

  BN_CTX_start(ctx);
  r1 = BN_CTX_get(ctx);
  m1 = BN_CTX_get(ctx);
  BIGNUM *n, *p, *q;
  if (r1 == NULL || m1 == NULL) {
    goto err;
  }

  n = &rsa->mont_n->N;
  p = &rsa->mont_p->N;
  q = &rsa->mont_q->N;

  declassify_assert(BN_ucmp(I, n) < 0);

  if (
      !mod_montgomery(r1, I, q, rsa->mont_q, p, ctx) ||
      !BN_mod_exp_mont_consttime(m1, r1, rsa->dmq1_fixed, q, ctx,
                                 rsa->mont_q) ||

      !mod_montgomery(r1, I, p, rsa->mont_p, q, ctx) ||
      !BN_mod_exp_mont_consttime(r0, r1, rsa->dmp1_fixed, p, ctx,
                                 rsa->mont_p) ||

      !mod_montgomery(r1, m1, p, rsa->mont_p, q, ctx) ||
      !bn_mod_sub_consttime(r0, r0, r1, p, ctx) ||

      !BN_mod_mul_montgomery(r0, r0, rsa->iqmp_mont, rsa->mont_p, ctx) ||

      !bn_mul_consttime(r0, r0, q, ctx) ||
      !bn_uadd_consttime(r0, r0, m1)) {
    goto err;
  }

  declassify_assert(BN_cmp(r0, n) < 0);
  bn_assert_fits_in_bytes(r0, BN_num_bytes(n));
  if (!bn_resize_words(r0, n->width)) {
    goto err;
  }

  ret = 1;

err:
  BN_CTX_end(ctx);
  return ret;
}

static int ensure_bignum(BIGNUM **out) {
  if (*out == NULL) {
    *out = BN_new();
  }
  return *out != NULL;
}

const BN_ULONG kBoringSSLRSASqrtTwo[] = {
    TOBN(0x4d7c60a5, 0xe633e3e1), TOBN(0x5fcf8f7b, 0xca3ea33b),
    TOBN(0xc246785e, 0x92957023), TOBN(0xf9acce41, 0x797f2805),
    TOBN(0xfdfe170f, 0xd3b1f780), TOBN(0xd24f4a76, 0x3facb882),
    TOBN(0x18838a2e, 0xaff5f3b2), TOBN(0xc1fcbdde, 0xa2f7dc33),
    TOBN(0xdea06241, 0xf7aa81c2), TOBN(0xf6a1be3f, 0xca221307),
    TOBN(0x332a5e9f, 0x7bda1ebf), TOBN(0x0104dc01, 0xfe32352f),
    TOBN(0xb8cf341b, 0x6f8236c7), TOBN(0x4264dabc, 0xd528b651),
    TOBN(0xf4d3a02c, 0xebc93e0c), TOBN(0x81394ab6, 0xd8fd0efd),
    TOBN(0xeaa4a089, 0x9040ca4a), TOBN(0xf52f120f, 0x836e582e),
    TOBN(0xcb2a6343, 0x31f3c84d), TOBN(0xc6d5a8a3, 0x8bb7e9dc),
    TOBN(0x460abc72, 0x2f7c4e33), TOBN(0xcab1bc91, 0x1688458a),
    TOBN(0x53059c60, 0x11bc337b), TOBN(0xd2202e87, 0x42af1f4e),
    TOBN(0x78048736, 0x3dfa2768), TOBN(0x0f74a85e, 0x439c7b4a),
    TOBN(0xa8b1fe6f, 0xdc83db39), TOBN(0x4afc8304, 0x3ab8a2c3),
    TOBN(0xed17ac85, 0x83339915), TOBN(0x1d6f60ba, 0x893ba84c),
    TOBN(0x597d89b3, 0x754abe9f), TOBN(0xb504f333, 0xf9de6484),
};
const size_t kBoringSSLRSASqrtTwoLen = OPENSSL_ARRAY_SIZE(kBoringSSLRSASqrtTwo);

static int generate_prime(BIGNUM *out, int bits, const BIGNUM *e,
                          const BIGNUM *p, const BIGNUM *sqrt2,
                          const BIGNUM *pow2_bits_100, BN_CTX *ctx,
                          BN_GENCB *cb) {
  if (bits < 128 || (bits % BN_BITS2) != 0) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
    return 0;
  }
  assert(BN_is_pow2(pow2_bits_100));
  assert(BN_is_bit_set(pow2_bits_100, bits - 100));

  if (bits >= INT_MAX / 32) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_MODULUS_TOO_LARGE);
    return 0;
  }
  int limit = BN_is_word(e, 3) ? bits * 8 : bits * 5;

  int ret = 0, tries = 0, rand_tries = 0;
  BN_CTX_start(ctx);
  BIGNUM *tmp = BN_CTX_get(ctx);
  if (tmp == NULL) {
    goto err;
  }

  for (;;) {

    if (!BN_rand(out, bits, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ODD) ||
        !BN_GENCB_call(cb, BN_GENCB_GENERATED, rand_tries++)) {
      goto err;
    }

    if (p != NULL) {

      if (!bn_abs_sub_consttime(tmp, out, p, ctx)) {
        goto err;
      }
      if (BN_cmp(tmp, pow2_bits_100) <= 0) {
        continue;
      }
    }

    if (constant_time_declassify_int(BN_cmp(out, sqrt2) <= 0)) {
      continue;
    }

    if (!bn_odd_number_is_obviously_composite(out)) {

      int relatively_prime;
      if (!bn_usub_consttime(tmp, out, BN_value_one()) ||
          !bn_is_relatively_prime(&relatively_prime, tmp, e, ctx)) {
        goto err;
      }
      if (constant_time_declassify_int(relatively_prime)) {

        int is_probable_prime;
        if (!BN_primality_test(&is_probable_prime, out,
                               BN_prime_checks_for_generation, ctx, 0, cb)) {
          goto err;
        }
        if (is_probable_prime) {
          ret = 1;
          goto err;
        }
      }
    }

    tries++;
    if (tries >= limit) {
      OPENSSL_PUT_ERROR(RSA, RSA_R_TOO_MANY_ITERATIONS);
      goto err;
    }
    if (!BN_GENCB_call(cb, 2, tries)) {
      goto err;
    }
  }

err:
  BN_CTX_end(ctx);
  return ret;
}

static int rsa_generate_key_impl(RSA *rsa, int bits, const BIGNUM *e_value,
                                 BN_GENCB *cb) {

  bits &= ~127;

  if (bits < 256) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_KEY_SIZE_TOO_SMALL);
    return 0;
  }

  if (BN_num_bits(e_value) > 32) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_E_VALUE);
    return 0;
  }

  int ret = 0;
  int prime_bits = bits / 2;
  BN_CTX *ctx = BN_CTX_new();
  BIGNUM *totient, *pm1, *qm1, *sqrt2, *pow2_prime_bits_100, *pow2_prime_bits;
  int sqrt2_bits;
  if (ctx == NULL) {
    goto bn_err;
  }
  BN_CTX_start(ctx);
  totient = BN_CTX_get(ctx);
  pm1 = BN_CTX_get(ctx);
  qm1 = BN_CTX_get(ctx);
  sqrt2 = BN_CTX_get(ctx);
  pow2_prime_bits_100 = BN_CTX_get(ctx);
  pow2_prime_bits = BN_CTX_get(ctx);
  if (totient == NULL || pm1 == NULL || qm1 == NULL || sqrt2 == NULL ||
      pow2_prime_bits_100 == NULL || pow2_prime_bits == NULL ||
      !BN_set_bit(pow2_prime_bits_100, prime_bits - 100) ||
      !BN_set_bit(pow2_prime_bits, prime_bits)) {
    goto bn_err;
  }

  if (!ensure_bignum(&rsa->n) ||
      !ensure_bignum(&rsa->d) ||
      !ensure_bignum(&rsa->e) ||
      !ensure_bignum(&rsa->p) ||
      !ensure_bignum(&rsa->q) ||
      !ensure_bignum(&rsa->dmp1) ||
      !ensure_bignum(&rsa->dmq1) ||
      !ensure_bignum(&rsa->iqmp)) {
    goto bn_err;
  }

  if (!BN_copy(rsa->e, e_value)) {
    goto bn_err;
  }

  if (!bn_set_words(sqrt2, kBoringSSLRSASqrtTwo, kBoringSSLRSASqrtTwoLen)) {
    goto bn_err;
  }
  sqrt2_bits = kBoringSSLRSASqrtTwoLen * BN_BITS2;
  assert(sqrt2_bits == (int)BN_num_bits(sqrt2));
  if (sqrt2_bits > prime_bits) {

    if (!BN_rshift(sqrt2, sqrt2, sqrt2_bits - prime_bits)) {
      goto bn_err;
    }
  } else if (prime_bits > sqrt2_bits) {

    if (!BN_add_word(sqrt2, 1) ||
        !BN_lshift(sqrt2, sqrt2, prime_bits - sqrt2_bits)) {
      goto bn_err;
    }
  }
  assert(prime_bits == (int)BN_num_bits(sqrt2));

  do {

    if (!generate_prime(rsa->p, prime_bits, rsa->e, NULL, sqrt2,
                        pow2_prime_bits_100, ctx, cb) ||
        !BN_GENCB_call(cb, 3, 0) ||
        !generate_prime(rsa->q, prime_bits, rsa->e, rsa->p, sqrt2,
                        pow2_prime_bits_100, ctx, cb) ||
        !BN_GENCB_call(cb, 3, 1)) {
      goto bn_err;
    }

    if (BN_cmp(rsa->p, rsa->q) < 0) {
      BIGNUM *tmp = rsa->p;
      rsa->p = rsa->q;
      rsa->q = tmp;
    }

    int no_inverse;
    if (!bn_usub_consttime(pm1, rsa->p, BN_value_one()) ||
        !bn_usub_consttime(qm1, rsa->q, BN_value_one()) ||
        !bn_lcm_consttime(totient, pm1, qm1, ctx) ||
        !bn_mod_inverse_consttime(rsa->d, &no_inverse, rsa->e, totient, ctx)) {
      goto bn_err;
    }

  } while (constant_time_declassify_int(BN_cmp(rsa->d, pow2_prime_bits) <= 0));

  assert(BN_num_bits(pm1) == (unsigned)prime_bits);
  assert(BN_num_bits(qm1) == (unsigned)prime_bits);
  if (
      !bn_mul_consttime(rsa->n, rsa->p, rsa->q, ctx) ||

      !bn_div_consttime(NULL, rsa->dmp1, rsa->d, pm1, prime_bits, ctx) ||

      !bn_div_consttime(NULL, rsa->dmq1, rsa->d, qm1, prime_bits, ctx)) {
    goto bn_err;
  }
  bn_set_minimal_width(rsa->n);

  bn_declassify(rsa->n);

  rsa->mont_p = BN_MONT_CTX_new_consttime(rsa->p, ctx);
  if (rsa->mont_p == NULL ||
      !bn_mod_inverse_secret_prime(rsa->iqmp, rsa->q, rsa->p, ctx,
                                   rsa->mont_p)) {
    goto bn_err;
  }

  if (BN_num_bits(rsa->n) != (unsigned)bits) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
    goto err;
  }

  if (!RSA_check_key(rsa) ||
      !freeze_private_key(rsa, ctx)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_INTERNAL_ERROR);
    goto err;
  }

  ret = 1;

bn_err:
  if (!ret) {
    OPENSSL_PUT_ERROR(RSA, ERR_LIB_BN);
  }
err:
  if (ctx != NULL) {
    BN_CTX_end(ctx);
    BN_CTX_free(ctx);
  }
  return ret;
}

static void replace_bignum(BIGNUM **out, BIGNUM **in) {
  BN_free(*out);
  *out = *in;
  *in = NULL;
}

static void replace_bn_mont_ctx(BN_MONT_CTX **out, BN_MONT_CTX **in) {
  BN_MONT_CTX_free(*out);
  *out = *in;
  *in = NULL;
}

static int RSA_generate_key_ex_maybe_fips(RSA *rsa, int bits,
                                          const BIGNUM *e_value, BN_GENCB *cb,
                                          int check_fips) {
  boringssl_ensure_rsa_self_test();

  if (rsa == NULL) {
    OPENSSL_PUT_ERROR(EC, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  RSA *tmp = NULL;
  uint32_t err;
  int ret = 0;

  int failures = 0;
  do {
    ERR_clear_error();

    tmp = RSA_new();
    if (tmp == NULL) {
      goto out;
    }

    if (rsa_generate_key_impl(tmp, bits, e_value, cb)) {
      break;
    }

    err = ERR_peek_error();
    RSA_free(tmp);
    tmp = NULL;
    failures++;

  } while (failures < 4 && ERR_GET_LIB(err) == ERR_LIB_RSA &&
           ERR_GET_REASON(err) == RSA_R_TOO_MANY_ITERATIONS);

  if (tmp == NULL || (check_fips && !RSA_check_fips(tmp))) {
    goto out;
  }

  rsa_invalidate_key(rsa);
  replace_bignum(&rsa->n, &tmp->n);
  replace_bignum(&rsa->e, &tmp->e);
  replace_bignum(&rsa->d, &tmp->d);
  replace_bignum(&rsa->p, &tmp->p);
  replace_bignum(&rsa->q, &tmp->q);
  replace_bignum(&rsa->dmp1, &tmp->dmp1);
  replace_bignum(&rsa->dmq1, &tmp->dmq1);
  replace_bignum(&rsa->iqmp, &tmp->iqmp);
  replace_bn_mont_ctx(&rsa->mont_n, &tmp->mont_n);
  replace_bn_mont_ctx(&rsa->mont_p, &tmp->mont_p);
  replace_bn_mont_ctx(&rsa->mont_q, &tmp->mont_q);
  replace_bignum(&rsa->d_fixed, &tmp->d_fixed);
  replace_bignum(&rsa->dmp1_fixed, &tmp->dmp1_fixed);
  replace_bignum(&rsa->dmq1_fixed, &tmp->dmq1_fixed);
  replace_bignum(&rsa->iqmp_mont, &tmp->iqmp_mont);
  rsa->private_key_frozen = tmp->private_key_frozen;
  ret = 1;

out:
  RSA_free(tmp);
  return ret;
}

int RSA_generate_key_ex(RSA *rsa, int bits, const BIGNUM *e_value,
                        BN_GENCB *cb) {
  return RSA_generate_key_ex_maybe_fips(rsa, bits, e_value, cb,
                                                       0);
}

int RSA_generate_key_fips(RSA *rsa, int bits, BN_GENCB *cb) {

  if (bits != 2048 && bits != 3072 && bits != 4096) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_RSA_PARAMETERS);
    return 0;
  }

  BIGNUM *e = BN_new();
  int ret = e != NULL && BN_set_word(e, RSA_F4) &&
            RSA_generate_key_ex_maybe_fips(rsa, bits, e, cb,                1);
  BN_free(e);

  if (ret) {
    FIPS_service_indicator_update_state();
  }
  return ret;
}

DEFINE_METHOD_FUNCTION(RSA_METHOD, RSA_default_method) {

  OPENSSL_memset(out, 0, sizeof(RSA_METHOD));
  out->common.is_static = 1;
}
