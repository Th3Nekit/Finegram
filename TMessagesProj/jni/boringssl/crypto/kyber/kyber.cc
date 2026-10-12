// Copyright 2023 The BoringSSL Authors
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

#define OPENSSL_UNSTABLE_EXPERIMENTAL_KYBER
#include <openssl/experimental/kyber.h>

#include <assert.h>
#include <stdlib.h>

#include <openssl/bytestring.h>
#include <openssl/rand.h>

#include "../fipsmodule/keccak/internal.h"
#include "../internal.h"
#include "./internal.h"

static void prf(uint8_t *out, size_t out_len, const uint8_t in[33]) {
  BORINGSSL_keccak(out, out_len, in, 33, boringssl_shake256);
}

static void hash_h(uint8_t out[32], const uint8_t *in, size_t len) {
  BORINGSSL_keccak(out, 32, in, len, boringssl_sha3_256);
}

static void hash_g(uint8_t out[64], const uint8_t *in, size_t len) {
  BORINGSSL_keccak(out, 64, in, len, boringssl_sha3_512);
}

static void kdf(uint8_t *out, size_t out_len, const uint8_t *in, size_t len) {
  BORINGSSL_keccak(out, out_len, in, len, boringssl_shake256);
}

#define DEGREE 256
#define RANK 3

static const size_t kBarrettMultiplier = 5039;
static const unsigned kBarrettShift = 24;
static const uint16_t kPrime = 3329;
static const int kLog2Prime = 12;
static const uint16_t kHalfPrime = (           3329 - 1) / 2;
static const int kDU = 10;
static const int kDV = 4;

static const uint16_t kInverseDegree = 3303;
static const size_t kEncodedVectorSize =
    (               12 * DEGREE / 8) * RANK;
static const size_t kCompressedVectorSize =         10 * RANK * DEGREE / 8;

typedef struct scalar {

  uint16_t c[DEGREE];
} scalar;

typedef struct vector {
  scalar v[RANK];
} vector;

typedef struct matrix {
  scalar v[RANK][RANK];
} matrix;

static const uint16_t kNTTRoots[128] = {
    1,    1729, 2580, 3289, 2642, 630,  1897, 848,  1062, 1919, 193,  797,
    2786, 3260, 569,  1746, 296,  2447, 1339, 1476, 3046, 56,   2240, 1333,
    1426, 2094, 535,  2882, 2393, 2879, 1974, 821,  289,  331,  3253, 1756,
    1197, 2304, 2277, 2055, 650,  1977, 2513, 632,  2865, 33,   1320, 1915,
    2319, 1435, 807,  452,  1438, 2868, 1534, 2402, 2647, 2617, 1481, 648,
    2474, 3110, 1227, 910,  17,   2761, 583,  2649, 1637, 723,  2288, 1100,
    1409, 2662, 3281, 233,  756,  2156, 3015, 3050, 1703, 1651, 2789, 1789,
    1847, 952,  1461, 2687, 939,  2308, 2437, 2388, 733,  2337, 268,  641,
    1584, 2298, 2037, 3220, 375,  2549, 2090, 1645, 1063, 319,  2773, 757,
    2099, 561,  2466, 2594, 2804, 1092, 403,  1026, 1143, 2150, 2775, 886,
    1722, 1212, 1874, 1029, 2110, 2935, 885,  2154,
};

static const uint16_t kInverseNTTRoots[128] = {
    1,    1600, 40,   749,  2481, 1432, 2699, 687,  1583, 2760, 69,   543,
    2532, 3136, 1410, 2267, 2508, 1355, 450,  936,  447,  2794, 1235, 1903,
    1996, 1089, 3273, 283,  1853, 1990, 882,  3033, 2419, 2102, 219,  855,
    2681, 1848, 712,  682,  927,  1795, 461,  1891, 2877, 2522, 1894, 1010,
    1414, 2009, 3296, 464,  2697, 816,  1352, 2679, 1274, 1052, 1025, 2132,
    1573, 76,   2998, 3040, 1175, 2444, 394,  1219, 2300, 1455, 2117, 1607,
    2443, 554,  1179, 2186, 2303, 2926, 2237, 525,  735,  863,  2768, 1230,
    2572, 556,  3010, 2266, 1684, 1239, 780,  2954, 109,  1292, 1031, 1745,
    2688, 3061, 992,  2596, 941,  892,  1021, 2390, 642,  1868, 2377, 1482,
    1540, 540,  1678, 1626, 279,  314,  1173, 2573, 3096, 48,   667,  1920,
    2229, 1041, 2606, 1692, 680,  2746, 568,  3312,
};

static const uint16_t kModRoots[128] = {
    17,   3312, 2761, 568,  583,  2746, 2649, 680,  1637, 1692, 723,  2606,
    2288, 1041, 1100, 2229, 1409, 1920, 2662, 667,  3281, 48,   233,  3096,
    756,  2573, 2156, 1173, 3015, 314,  3050, 279,  1703, 1626, 1651, 1678,
    2789, 540,  1789, 1540, 1847, 1482, 952,  2377, 1461, 1868, 2687, 642,
    939,  2390, 2308, 1021, 2437, 892,  2388, 941,  733,  2596, 2337, 992,
    268,  3061, 641,  2688, 1584, 1745, 2298, 1031, 2037, 1292, 3220, 109,
    375,  2954, 2549, 780,  2090, 1239, 1645, 1684, 1063, 2266, 319,  3010,
    2773, 556,  757,  2572, 2099, 1230, 561,  2768, 2466, 863,  2594, 735,
    2804, 525,  1092, 2237, 403,  2926, 1026, 2303, 1143, 2186, 2150, 1179,
    2775, 554,  886,  2443, 1722, 1607, 1212, 2117, 1874, 1455, 1029, 2300,
    2110, 1219, 2935, 394,  885,  2444, 2154, 1175,
};

static uint16_t reduce_once(uint16_t x) {
  declassify_assert(x < 2 * kPrime);
  const uint16_t subtracted = x - kPrime;
  uint16_t mask = 0u - (subtracted >> 15);

  return (mask & x) | (~mask & subtracted);
}

static uint16_t reduce(uint32_t x) {
  declassify_assert(x < kPrime + 2u * kPrime * kPrime);
  uint64_t product = (uint64_t)x * kBarrettMultiplier;
  uint32_t quotient = (uint32_t)(product >> kBarrettShift);
  uint32_t remainder = x - quotient * kPrime;
  return reduce_once(remainder);
}

static void scalar_zero(scalar *out) { OPENSSL_memset(out, 0, sizeof(*out)); }

static void vector_zero(vector *out) { OPENSSL_memset(out, 0, sizeof(*out)); }

static void scalar_ntt(scalar *s) {
  int offset = DEGREE;

  for (int step = 1; step < DEGREE / 2; step <<= 1) {
    offset >>= 1;
    int k = 0;
    for (int i = 0; i < step; i++) {
      const uint32_t step_root = kNTTRoots[i + step];
      for (int j = k; j < k + offset; j++) {
        uint16_t odd = reduce(step_root * s->c[j + offset]);
        uint16_t even = s->c[j];
        s->c[j] = reduce_once(odd + even);
        s->c[j + offset] = reduce_once(even - odd + kPrime);
      }
      k += 2 * offset;
    }
  }
}

static void vector_ntt(vector *a) {
  for (int i = 0; i < RANK; i++) {
    scalar_ntt(&a->v[i]);
  }
}

static void scalar_inverse_ntt(scalar *s) {
  int step = DEGREE / 2;

  for (int offset = 2; offset < DEGREE; offset <<= 1) {
    step >>= 1;
    int k = 0;
    for (int i = 0; i < step; i++) {
      uint32_t step_root = kInverseNTTRoots[i + step];
      for (int j = k; j < k + offset; j++) {
        uint16_t odd = s->c[j + offset];
        uint16_t even = s->c[j];
        s->c[j] = reduce_once(odd + even);
        s->c[j + offset] = reduce(step_root * (even - odd + kPrime));
      }
      k += 2 * offset;
    }
  }
  for (int i = 0; i < DEGREE; i++) {
    s->c[i] = reduce(s->c[i] * kInverseDegree);
  }
}

static void vector_inverse_ntt(vector *a) {
  for (int i = 0; i < RANK; i++) {
    scalar_inverse_ntt(&a->v[i]);
  }
}

static void scalar_add(scalar *lhs, const scalar *rhs) {
  for (int i = 0; i < DEGREE; i++) {
    lhs->c[i] = reduce_once(lhs->c[i] + rhs->c[i]);
  }
}

static void scalar_sub(scalar *lhs, const scalar *rhs) {
  for (int i = 0; i < DEGREE; i++) {
    lhs->c[i] = reduce_once(lhs->c[i] - rhs->c[i] + kPrime);
  }
}

static void scalar_mult(scalar *out, const scalar *lhs, const scalar *rhs) {
  for (int i = 0; i < DEGREE / 2; i++) {
    uint32_t real_real = (uint32_t)lhs->c[2 * i] * rhs->c[2 * i];
    uint32_t img_img = (uint32_t)lhs->c[2 * i + 1] * rhs->c[2 * i + 1];
    uint32_t real_img = (uint32_t)lhs->c[2 * i] * rhs->c[2 * i + 1];
    uint32_t img_real = (uint32_t)lhs->c[2 * i + 1] * rhs->c[2 * i];
    out->c[2 * i] =
        reduce(real_real + (uint32_t)reduce(img_img) * kModRoots[i]);
    out->c[2 * i + 1] = reduce(img_real + real_img);
  }
}

static void vector_add(vector *lhs, const vector *rhs) {
  for (int i = 0; i < RANK; i++) {
    scalar_add(&lhs->v[i], &rhs->v[i]);
  }
}

static void matrix_mult(vector *out, const matrix *m, const vector *a) {
  vector_zero(out);
  for (int i = 0; i < RANK; i++) {
    for (int j = 0; j < RANK; j++) {
      scalar product;
      scalar_mult(&product, &m->v[i][j], &a->v[j]);
      scalar_add(&out->v[i], &product);
    }
  }
}

static void matrix_mult_transpose(vector *out, const matrix *m,
                                  const vector *a) {
  vector_zero(out);
  for (int i = 0; i < RANK; i++) {
    for (int j = 0; j < RANK; j++) {
      scalar product;
      scalar_mult(&product, &m->v[j][i], &a->v[j]);
      scalar_add(&out->v[i], &product);
    }
  }
}

static void scalar_inner_product(scalar *out, const vector *lhs,
                                 const vector *rhs) {
  scalar_zero(out);
  for (int i = 0; i < RANK; i++) {
    scalar product;
    scalar_mult(&product, &lhs->v[i], &rhs->v[i]);
    scalar_add(out, &product);
  }
}

static void scalar_from_keccak_vartime(scalar *out,
                                       struct BORINGSSL_keccak_st *keccak_ctx) {
  assert(keccak_ctx->squeeze_offset == 0);
  assert(keccak_ctx->rate_bytes == 168);
  static_assert(168 % 3 == 0, "block and coefficient boundaries do not align");

  int done = 0;
  while (done < DEGREE) {
    uint8_t block[168];
    BORINGSSL_keccak_squeeze(keccak_ctx, block, sizeof(block));
    for (size_t i = 0; i < sizeof(block) && done < DEGREE; i += 3) {
      uint16_t d1 = block[i] + 256 * (block[i + 1] % 16);
      uint16_t d2 = block[i + 1] / 16 + 16 * block[i + 2];
      if (d1 < kPrime) {
        out->c[done++] = d1;
      }
      if (d2 < kPrime && done < DEGREE) {
        out->c[done++] = d2;
      }
    }
  }
}

static void scalar_centered_binomial_distribution_eta_2_with_prf(
    scalar *out, const uint8_t input[33]) {
  uint8_t entropy[128];
  static_assert(sizeof(entropy) == 2 *          2 * DEGREE / 8, "");
  prf(entropy, sizeof(entropy), input);

  for (int i = 0; i < DEGREE; i += 2) {
    uint8_t byte = entropy[i / 2];

    uint16_t value = (byte & 1) + ((byte >> 1) & 1);
    value -= ((byte >> 2) & 1) + ((byte >> 3) & 1);

    uint16_t mask = 0u - (value >> 15);
    out->c[i] = value + (kPrime & mask);

    byte >>= 4;
    value = (byte & 1) + ((byte >> 1) & 1);
    value -= ((byte >> 2) & 1) + ((byte >> 3) & 1);

    mask = 0u - (value >> 15);
    out->c[i + 1] = value + (kPrime & mask);
  }
}

static void vector_generate_secret_eta_2(vector *out, uint8_t *counter,
                                         const uint8_t seed[32]) {
  uint8_t input[33];
  OPENSSL_memcpy(input, seed, 32);
  for (int i = 0; i < RANK; i++) {
    input[32] = (*counter)++;
    scalar_centered_binomial_distribution_eta_2_with_prf(&out->v[i], input);
  }
}

static void matrix_expand(matrix *out, const uint8_t rho[32]) {
  uint8_t input[34];
  OPENSSL_memcpy(input, rho, 32);
  for (int i = 0; i < RANK; i++) {
    for (int j = 0; j < RANK; j++) {
      input[32] = i;
      input[33] = j;
      struct BORINGSSL_keccak_st keccak_ctx;
      BORINGSSL_keccak_init(&keccak_ctx, boringssl_shake128);
      BORINGSSL_keccak_absorb(&keccak_ctx, input, sizeof(input));
      scalar_from_keccak_vartime(&out->v[i][j], &keccak_ctx);
    }
  }
}

static const uint8_t kMasks[8] = {0x01, 0x03, 0x07, 0x0f,
                                  0x1f, 0x3f, 0x7f, 0xff};

static void scalar_encode(uint8_t *out, const scalar *s, int bits) {
  assert(bits <= (int)sizeof(*s->c) * 8 && bits != 1);

  uint8_t out_byte = 0;
  int out_byte_bits = 0;

  for (int i = 0; i < DEGREE; i++) {
    uint16_t element = s->c[i];
    int element_bits_done = 0;

    while (element_bits_done < bits) {
      int chunk_bits = bits - element_bits_done;
      int out_bits_remaining = 8 - out_byte_bits;
      if (chunk_bits >= out_bits_remaining) {
        chunk_bits = out_bits_remaining;
        out_byte |= (element & kMasks[chunk_bits - 1]) << out_byte_bits;
        *out = out_byte;
        out++;
        out_byte_bits = 0;
        out_byte = 0;
      } else {
        out_byte |= (element & kMasks[chunk_bits - 1]) << out_byte_bits;
        out_byte_bits += chunk_bits;
      }

      element_bits_done += chunk_bits;
      element >>= chunk_bits;
    }
  }

  if (out_byte_bits > 0) {
    *out = out_byte;
  }
}

static void scalar_encode_1(uint8_t out[32], const scalar *s) {
  for (int i = 0; i < DEGREE; i += 8) {
    uint8_t out_byte = 0;
    for (int j = 0; j < 8; j++) {
      out_byte |= (s->c[i + j] & 1) << j;
    }
    *out = out_byte;
    out++;
  }
}

static void vector_encode(uint8_t *out, const vector *a, int bits) {
  for (int i = 0; i < RANK; i++) {
    scalar_encode(out + i * bits * DEGREE / 8, &a->v[i], bits);
  }
}

static int scalar_decode(scalar *out, const uint8_t *in, int bits) {
  assert(bits <= (int)sizeof(*out->c) * 8 && bits != 1);

  uint8_t in_byte = 0;
  int in_byte_bits_left = 0;

  for (int i = 0; i < DEGREE; i++) {
    uint16_t element = 0;
    int element_bits_done = 0;

    while (element_bits_done < bits) {
      if (in_byte_bits_left == 0) {
        in_byte = *in;
        in++;
        in_byte_bits_left = 8;
      }

      int chunk_bits = bits - element_bits_done;
      if (chunk_bits > in_byte_bits_left) {
        chunk_bits = in_byte_bits_left;
      }

      element |= (in_byte & kMasks[chunk_bits - 1]) << element_bits_done;
      in_byte_bits_left -= chunk_bits;
      in_byte >>= chunk_bits;

      element_bits_done += chunk_bits;
    }

    if (constant_time_declassify_int(element >= kPrime)) {
      return 0;
    }
    out->c[i] = element;
  }

  return 1;
}

static void scalar_decode_1(scalar *out, const uint8_t in[32]) {
  for (int i = 0; i < DEGREE; i += 8) {
    uint8_t in_byte = *in;
    in++;
    for (int j = 0; j < 8; j++) {
      out->c[i + j] = in_byte & 1;
      in_byte >>= 1;
    }
  }
}

static int vector_decode(vector *out, const uint8_t *in, int bits) {
  for (int i = 0; i < RANK; i++) {
    if (!scalar_decode(&out->v[i], in + i * bits * DEGREE / 8, bits)) {
      return 0;
    }
  }
  return 1;
}

static uint16_t compress(uint16_t x, int bits) {
  uint32_t shifted = (uint32_t)x << bits;
  uint64_t product = (uint64_t)shifted * kBarrettMultiplier;
  uint32_t quotient = (uint32_t)(product >> kBarrettShift);
  uint32_t remainder = shifted - quotient * kPrime;

  declassify_assert(remainder < 2u * kPrime);
  quotient += 1 & constant_time_lt_w(kHalfPrime, remainder);
  quotient += 1 & constant_time_lt_w(kPrime + kHalfPrime, remainder);
  return quotient & ((1 << bits) - 1);
}

static uint16_t decompress(uint16_t x, int bits) {
  uint32_t product = (uint32_t)x * kPrime;
  uint32_t power = 1 << bits;

  uint32_t remainder = product & (power - 1);

  uint32_t lower = product >> bits;

  return lower + (remainder >> (bits - 1));
}

static void scalar_compress(scalar *s, int bits) {
  for (int i = 0; i < DEGREE; i++) {
    s->c[i] = compress(s->c[i], bits);
  }
}

static void scalar_decompress(scalar *s, int bits) {
  for (int i = 0; i < DEGREE; i++) {
    s->c[i] = decompress(s->c[i], bits);
  }
}

static void vector_compress(vector *a, int bits) {
  for (int i = 0; i < RANK; i++) {
    scalar_compress(&a->v[i], bits);
  }
}

static void vector_decompress(vector *a, int bits) {
  for (int i = 0; i < RANK; i++) {
    scalar_decompress(&a->v[i], bits);
  }
}

namespace {

struct public_key {
  vector t;
  uint8_t rho[32];
  uint8_t public_key_hash[32];
  matrix m;
};

static struct public_key *public_key_from_external(
    const struct KYBER_public_key *external) {
  static_assert(sizeof(struct KYBER_public_key) >= sizeof(struct public_key),
                "Kyber public key is too small");
  static_assert(alignof(struct KYBER_public_key) >= alignof(struct public_key),
                "Kyber public key align incorrect");
  return (struct public_key *)external;
}

struct private_key {
  struct public_key pub;
  vector s;
  uint8_t fo_failure_secret[32];
};

static struct private_key *private_key_from_external(
    const struct KYBER_private_key *external) {
  static_assert(sizeof(struct KYBER_private_key) >= sizeof(struct private_key),
                "Kyber private key too small");
  static_assert(
      alignof(struct KYBER_private_key) >= alignof(struct private_key),
      "Kyber private key align incorrect");
  return (struct private_key *)external;
}

}

void KYBER_generate_key(uint8_t out_encoded_public_key[KYBER_PUBLIC_KEY_BYTES],
                        struct KYBER_private_key *out_private_key) {
  uint8_t entropy[KYBER_GENERATE_KEY_ENTROPY];
  RAND_bytes(entropy, sizeof(entropy));
  CONSTTIME_SECRET(entropy, sizeof(entropy));
  KYBER_generate_key_external_entropy(out_encoded_public_key, out_private_key,
                                      entropy);
}

static int kyber_marshal_public_key(CBB *out, const struct public_key *pub) {
  uint8_t *vector_output;
  if (!CBB_add_space(out, &vector_output, kEncodedVectorSize)) {
    return 0;
  }
  vector_encode(vector_output, &pub->t, kLog2Prime);
  if (!CBB_add_bytes(out, pub->rho, sizeof(pub->rho))) {
    return 0;
  }
  return 1;
}

void KYBER_generate_key_external_entropy(
    uint8_t out_encoded_public_key[KYBER_PUBLIC_KEY_BYTES],
    struct KYBER_private_key *out_private_key,
    const uint8_t entropy[KYBER_GENERATE_KEY_ENTROPY]) {
  struct private_key *priv = private_key_from_external(out_private_key);
  uint8_t hashed[64];
  hash_g(hashed, entropy, 32);
  const uint8_t *const rho = hashed;
  const uint8_t *const sigma = hashed + 32;

  CONSTTIME_DECLASSIFY(rho, 32);
  OPENSSL_memcpy(priv->pub.rho, hashed, sizeof(priv->pub.rho));
  matrix_expand(&priv->pub.m, rho);
  uint8_t counter = 0;
  vector_generate_secret_eta_2(&priv->s, &counter, sigma);
  vector_ntt(&priv->s);
  vector error;
  vector_generate_secret_eta_2(&error, &counter, sigma);
  vector_ntt(&error);
  matrix_mult_transpose(&priv->pub.t, &priv->pub.m, &priv->s);
  vector_add(&priv->pub.t, &error);

  CONSTTIME_DECLASSIFY(&priv->pub.t, sizeof(priv->pub.t));

  CBB cbb;
  CBB_init_fixed(&cbb, out_encoded_public_key, KYBER_PUBLIC_KEY_BYTES);
  if (!kyber_marshal_public_key(&cbb, &priv->pub)) {
    abort();
  }

  hash_h(priv->pub.public_key_hash, out_encoded_public_key,
         KYBER_PUBLIC_KEY_BYTES);
  OPENSSL_memcpy(priv->fo_failure_secret, entropy + 32, 32);
}

void KYBER_public_from_private(struct KYBER_public_key *out_public_key,
                               const struct KYBER_private_key *private_key) {
  struct public_key *const pub = public_key_from_external(out_public_key);
  const struct private_key *const priv = private_key_from_external(private_key);
  *pub = priv->pub;
}

static void encrypt_cpa(uint8_t out[KYBER_CIPHERTEXT_BYTES],
                        const struct public_key *pub, const uint8_t message[32],
                        const uint8_t randomness[32]) {
  uint8_t counter = 0;
  vector secret;
  vector_generate_secret_eta_2(&secret, &counter, randomness);
  vector_ntt(&secret);
  vector error;
  vector_generate_secret_eta_2(&error, &counter, randomness);
  uint8_t input[33];
  OPENSSL_memcpy(input, randomness, 32);
  input[32] = counter;
  scalar scalar_error;
  scalar_centered_binomial_distribution_eta_2_with_prf(&scalar_error, input);
  vector u;
  matrix_mult(&u, &pub->m, &secret);
  vector_inverse_ntt(&u);
  vector_add(&u, &error);
  scalar v;
  scalar_inner_product(&v, &pub->t, &secret);
  scalar_inverse_ntt(&v);
  scalar_add(&v, &scalar_error);
  scalar expanded_message;
  scalar_decode_1(&expanded_message, message);
  scalar_decompress(&expanded_message, 1);
  scalar_add(&v, &expanded_message);
  vector_compress(&u, kDU);
  vector_encode(out, &u, kDU);
  scalar_compress(&v, kDV);
  scalar_encode(out + kCompressedVectorSize, &v, kDV);
}

void KYBER_encap(uint8_t out_ciphertext[KYBER_CIPHERTEXT_BYTES],
                 uint8_t out_shared_secret[KYBER_SHARED_SECRET_BYTES],
                 const struct KYBER_public_key *public_key) {
  uint8_t entropy[KYBER_ENCAP_ENTROPY];
  RAND_bytes(entropy, KYBER_ENCAP_ENTROPY);
  CONSTTIME_SECRET(entropy, KYBER_ENCAP_ENTROPY);
  KYBER_encap_external_entropy(out_ciphertext, out_shared_secret, public_key,
                               entropy);
}

void KYBER_encap_external_entropy(
    uint8_t out_ciphertext[KYBER_CIPHERTEXT_BYTES],
    uint8_t out_shared_secret[KYBER_SHARED_SECRET_BYTES],
    const struct KYBER_public_key *public_key,
    const uint8_t entropy[KYBER_ENCAP_ENTROPY]) {
  const struct public_key *pub = public_key_from_external(public_key);
  uint8_t input[64];
  OPENSSL_memcpy(input, entropy, KYBER_ENCAP_ENTROPY);
  OPENSSL_memcpy(input + KYBER_ENCAP_ENTROPY, pub->public_key_hash,
                 sizeof(input) - KYBER_ENCAP_ENTROPY);
  uint8_t prekey_and_randomness[64];
  hash_g(prekey_and_randomness, input, sizeof(input));
  encrypt_cpa(out_ciphertext, pub, entropy, prekey_and_randomness + 32);

  CONSTTIME_DECLASSIFY(out_ciphertext, KYBER_CIPHERTEXT_BYTES);
  hash_h(prekey_and_randomness + 32, out_ciphertext, KYBER_CIPHERTEXT_BYTES);
  kdf(out_shared_secret, KYBER_SHARED_SECRET_BYTES, prekey_and_randomness,
      sizeof(prekey_and_randomness));
}

static void decrypt_cpa(uint8_t out[32], const struct private_key *priv,
                        const uint8_t ciphertext[KYBER_CIPHERTEXT_BYTES]) {
  vector u;
  vector_decode(&u, ciphertext, kDU);
  vector_decompress(&u, kDU);
  vector_ntt(&u);
  scalar v;
  scalar_decode(&v, ciphertext + kCompressedVectorSize, kDV);
  scalar_decompress(&v, kDV);
  scalar mask;
  scalar_inner_product(&mask, &priv->s, &u);
  scalar_inverse_ntt(&mask);
  scalar_sub(&v, &mask);
  scalar_compress(&v, 1);
  scalar_encode_1(out, &v);
}

void KYBER_decap(uint8_t out_shared_secret[KYBER_SHARED_SECRET_BYTES],
                 const uint8_t ciphertext[KYBER_CIPHERTEXT_BYTES],
                 const struct KYBER_private_key *private_key) {
  const struct private_key *priv = private_key_from_external(private_key);
  uint8_t decrypted[64];
  decrypt_cpa(decrypted, priv, ciphertext);
  OPENSSL_memcpy(decrypted + 32, priv->pub.public_key_hash,
                 sizeof(decrypted) - 32);
  uint8_t prekey_and_randomness[64];
  hash_g(prekey_and_randomness, decrypted, sizeof(decrypted));
  uint8_t expected_ciphertext[KYBER_CIPHERTEXT_BYTES];
  encrypt_cpa(expected_ciphertext, &priv->pub, decrypted,
              prekey_and_randomness + 32);
  uint8_t mask =
      constant_time_eq_int_8(CRYPTO_memcmp(ciphertext, expected_ciphertext,
                                           sizeof(expected_ciphertext)),
                             0);
  uint8_t input[64];
  for (int i = 0; i < 32; i++) {
    input[i] = constant_time_select_8(mask, prekey_and_randomness[i],
                                      priv->fo_failure_secret[i]);
  }
  hash_h(input + 32, ciphertext, KYBER_CIPHERTEXT_BYTES);
  kdf(out_shared_secret, KYBER_SHARED_SECRET_BYTES, input, sizeof(input));
}

int KYBER_marshal_public_key(CBB *out,
                             const struct KYBER_public_key *public_key) {
  return kyber_marshal_public_key(out, public_key_from_external(public_key));
}

static int kyber_parse_public_key_no_hash(struct public_key *pub, CBS *in) {
  CBS t_bytes;
  if (!CBS_get_bytes(in, &t_bytes, kEncodedVectorSize) ||
      !vector_decode(&pub->t, CBS_data(&t_bytes), kLog2Prime) ||
      !CBS_copy_bytes(in, pub->rho, sizeof(pub->rho))) {
    return 0;
  }
  matrix_expand(&pub->m, pub->rho);
  return 1;
}

int KYBER_parse_public_key(struct KYBER_public_key *public_key, CBS *in) {
  struct public_key *pub = public_key_from_external(public_key);
  CBS orig_in = *in;
  if (!kyber_parse_public_key_no_hash(pub, in) ||
      CBS_len(in) != 0) {
    return 0;
  }
  hash_h(pub->public_key_hash, CBS_data(&orig_in), CBS_len(&orig_in));
  return 1;
}

int KYBER_marshal_private_key(CBB *out,
                              const struct KYBER_private_key *private_key) {
  const struct private_key *const priv = private_key_from_external(private_key);
  uint8_t *s_output;
  if (!CBB_add_space(out, &s_output, kEncodedVectorSize)) {
    return 0;
  }
  vector_encode(s_output, &priv->s, kLog2Prime);
  if (!kyber_marshal_public_key(out, &priv->pub) ||
      !CBB_add_bytes(out, priv->pub.public_key_hash,
                     sizeof(priv->pub.public_key_hash)) ||
      !CBB_add_bytes(out, priv->fo_failure_secret,
                     sizeof(priv->fo_failure_secret))) {
    return 0;
  }
  return 1;
}

int KYBER_parse_private_key(struct KYBER_private_key *out_private_key,
                            CBS *in) {
  struct private_key *const priv = private_key_from_external(out_private_key);

  CBS s_bytes;
  if (!CBS_get_bytes(in, &s_bytes, kEncodedVectorSize) ||
      !vector_decode(&priv->s, CBS_data(&s_bytes), kLog2Prime) ||
      !kyber_parse_public_key_no_hash(&priv->pub, in) ||
      !CBS_copy_bytes(in, priv->pub.public_key_hash,
                      sizeof(priv->pub.public_key_hash)) ||
      !CBS_copy_bytes(in, priv->fo_failure_secret,
                      sizeof(priv->fo_failure_secret)) ||
      CBS_len(in) != 0) {
    return 0;
  }
  return 1;
}
