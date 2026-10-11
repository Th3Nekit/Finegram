// Copyright 2014 The BoringSSL Authors
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

#include <openssl/base.h>

#include <memory>

#include <assert.h>
#include <stdlib.h>

#include <openssl/bytestring.h>
#include <openssl/mem.h>
#include <openssl/rand.h>

#include "../../internal.h"
#include "../bcm_interface.h"
#include "../keccak/internal.h"

namespace mldsa {
namespace {

namespace fips {
void ensure_keygen_self_test();
void ensure_sign_self_test();
void ensure_verify_self_test();
}

constexpr int kDegree = 256;
constexpr int kRhoBytes = 32;
constexpr int kSigmaBytes = 64;
constexpr int kKBytes = 32;
constexpr int kTrBytes = 64;
constexpr int kMuBytes = 64;
constexpr int kRhoPrimeBytes = 64;

constexpr uint32_t kPrime = 8380417;

constexpr uint32_t kPrimeNegInverse = 4236238847;
constexpr int kDroppedBits = 13;
constexpr uint32_t kHalfPrime = (kPrime - 1) / 2;
constexpr uint32_t kGamma2 = (kPrime - 1) / 32;

constexpr uint32_t kInverseDegreeMontgomery = 41978;

template <int K>
constexpr size_t public_key_bytes() {
  if constexpr (K == 6) {
    return BCM_MLDSA65_PUBLIC_KEY_BYTES;
  } else if constexpr (K == 8) {
    return BCM_MLDSA87_PUBLIC_KEY_BYTES;
  }
}

template <int K>
constexpr size_t signature_bytes() {
  if constexpr (K == 6) {
    return BCM_MLDSA65_SIGNATURE_BYTES;
  } else if constexpr (K == 8) {
    return BCM_MLDSA87_SIGNATURE_BYTES;
  }
}

template <int K>
constexpr int tau() {
  if constexpr (K == 6) {
    return 49;
  } else if constexpr (K == 8) {
    return 60;
  }
}

template <int K>
constexpr int lambda_bytes() {
  if constexpr (K == 6) {
    return 192 / 8;
  } else if constexpr (K == 8) {
    return 256 / 8;
  }
}

template <int K>
constexpr int gamma1() {
  if constexpr (K == 6 || K == 8) {
    return 1 << 19;
  }
}

template <int K>
constexpr int beta() {
  if constexpr (K == 6) {
    return 196;
  } else if constexpr (K == 8) {
    return 120;
  }
}

template <int K>
constexpr int omega() {
  if constexpr (K == 6) {
    return 55;
  } else if constexpr (K == 8) {
    return 75;
  }
}

template <int K>
constexpr int eta() {
  if constexpr (K == 6) {
    return 4;
  } else if constexpr (K == 8) {
    return 2;
  }
}

template <int K>
constexpr int plus_minus_eta_bitlen() {
  if constexpr (K == 6) {
    return 4;
  } else if constexpr (K == 8) {
    return 3;
  }
}

typedef struct scalar {
  uint32_t c[kDegree];
} scalar;

template <int K>
struct vector {
  scalar v[K];
};

template <int K, int L>
struct matrix {
  scalar v[K][L];
};

static const uint32_t kNTTRootsMontgomery[256] = {
    4193792, 25847,   5771523, 7861508, 237124,  7602457, 7504169, 466468,
    1826347, 2353451, 8021166, 6288512, 3119733, 5495562, 3111497, 2680103,
    2725464, 1024112, 7300517, 3585928, 7830929, 7260833, 2619752, 6271868,
    6262231, 4520680, 6980856, 5102745, 1757237, 8360995, 4010497, 280005,
    2706023, 95776,   3077325, 3530437, 6718724, 4788269, 5842901, 3915439,
    4519302, 5336701, 3574422, 5512770, 3539968, 8079950, 2348700, 7841118,
    6681150, 6736599, 3505694, 4558682, 3507263, 6239768, 6779997, 3699596,
    811944,  531354,  954230,  3881043, 3900724, 5823537, 2071892, 5582638,
    4450022, 6851714, 4702672, 5339162, 6927966, 3475950, 2176455, 6795196,
    7122806, 1939314, 4296819, 7380215, 5190273, 5223087, 4747489, 126922,
    3412210, 7396998, 2147896, 2715295, 5412772, 4686924, 7969390, 5903370,
    7709315, 7151892, 8357436, 7072248, 7998430, 1349076, 1852771, 6949987,
    5037034, 264944,  508951,  3097992, 44288,   7280319, 904516,  3958618,
    4656075, 8371839, 1653064, 5130689, 2389356, 8169440, 759969,  7063561,
    189548,  4827145, 3159746, 6529015, 5971092, 8202977, 1315589, 1341330,
    1285669, 6795489, 7567685, 6940675, 5361315, 4499357, 4751448, 3839961,
    2091667, 3407706, 2316500, 3817976, 5037939, 2244091, 5933984, 4817955,
    266997,  2434439, 7144689, 3513181, 4860065, 4621053, 7183191, 5187039,
    900702,  1859098, 909542,  819034,  495491,  6767243, 8337157, 7857917,
    7725090, 5257975, 2031748, 3207046, 4823422, 7855319, 7611795, 4784579,
    342297,  286988,  5942594, 4108315, 3437287, 5038140, 1735879, 203044,
    2842341, 2691481, 5790267, 1265009, 4055324, 1247620, 2486353, 1595974,
    4613401, 1250494, 2635921, 4832145, 5386378, 1869119, 1903435, 7329447,
    7047359, 1237275, 5062207, 6950192, 7929317, 1312455, 3306115, 6417775,
    7100756, 1917081, 5834105, 7005614, 1500165, 777191,  2235880, 3406031,
    7838005, 5548557, 6709241, 6533464, 5796124, 4656147, 594136,  4603424,
    6366809, 2432395, 2454455, 8215696, 1957272, 3369112, 185531,  7173032,
    5196991, 162844,  1616392, 3014001, 810149,  1652634, 4686184, 6581310,
    5341501, 3523897, 3866901, 269760,  2213111, 7404533, 1717735, 472078,
    7953734, 1723600, 6577327, 1910376, 6712985, 7276084, 8119771, 4546524,
    5441381, 6144432, 7959518, 6094090, 183443,  7403526, 1612842, 4834730,
    7826001, 3919660, 8332111, 7018208, 3937738, 1400424, 7534263, 1976782};

uint32_t reduce_once(uint32_t x) {
  declassify_assert(x < 2 * kPrime);

  return constant_time_select_int(constant_time_lt_w(x, kPrime), x, x - kPrime);
}

uint32_t abs_signed(uint32_t x) {

  return constant_time_select_int(constant_time_lt_w(x, 0x80000000), x, 0u - x);
}

uint32_t abs_mod_prime(uint32_t x) {
  declassify_assert(x < kPrime);

  return constant_time_select_int(constant_time_lt_w(kHalfPrime, x), kPrime - x,
                                  x);
}

uint32_t maximum(uint32_t x, uint32_t y) {

  return constant_time_select_int(constant_time_lt_w(x, y), y, x);
}

uint32_t mod_sub(uint32_t a, uint32_t b) {
  declassify_assert(a < kPrime);
  declassify_assert(b < kPrime);
  return reduce_once(kPrime + a - b);
}

void scalar_add(scalar *out, const scalar *lhs, const scalar *rhs) {
  for (int i = 0; i < kDegree; i++) {
    out->c[i] = reduce_once(lhs->c[i] + rhs->c[i]);
  }
}

void scalar_sub(scalar *out, const scalar *lhs, const scalar *rhs) {
  for (int i = 0; i < kDegree; i++) {
    out->c[i] = mod_sub(lhs->c[i], rhs->c[i]);
  }
}

uint32_t reduce_montgomery(uint64_t x) {
  declassify_assert(x <= ((uint64_t)kPrime << 32));
  uint64_t a = (uint32_t)x * kPrimeNegInverse;
  uint64_t b = x + a * kPrime;
  declassify_assert((b & 0xffffffff) == 0);
  uint32_t c = b >> 32;
  return reduce_once(c);
}

void scalar_mult(scalar *out, const scalar *lhs, const scalar *rhs) {
  for (int i = 0; i < kDegree; i++) {
    out->c[i] = reduce_montgomery((uint64_t)lhs->c[i] * (uint64_t)rhs->c[i]);
  }
}

static void scalar_ntt(scalar *s) {

  int offset = kDegree;
  for (int step = 1; step < kDegree; step <<= 1) {
    offset >>= 1;
    int k = 0;
    for (int i = 0; i < step; i++) {
      assert(k == 2 * offset * i);
      const uint32_t step_root = kNTTRootsMontgomery[step + i];
      for (int j = k; j < k + offset; j++) {
        uint32_t even = s->c[j];

        uint32_t odd =
            reduce_montgomery((uint64_t)step_root * (uint64_t)s->c[j + offset]);
        s->c[j] = reduce_once(odd + even);
        s->c[j + offset] = mod_sub(even, odd);
      }
      k += 2 * offset;
    }
  }
}

void scalar_inverse_ntt(scalar *s) {

  int step = kDegree;
  for (int offset = 1; offset < kDegree; offset <<= 1) {
    step >>= 1;
    int k = 0;
    for (int i = 0; i < step; i++) {
      assert(k == 2 * offset * i);
      const uint32_t step_root =
          kPrime - kNTTRootsMontgomery[step + (step - 1 - i)];
      for (int j = k; j < k + offset; j++) {
        uint32_t even = s->c[j];
        uint32_t odd = s->c[j + offset];
        s->c[j] = reduce_once(odd + even);

        s->c[j + offset] = reduce_montgomery((uint64_t)step_root *
                                             (uint64_t)(kPrime + even - odd));
      }
      k += 2 * offset;
    }
  }
  for (int i = 0; i < kDegree; i++) {
    s->c[i] = reduce_montgomery((uint64_t)s->c[i] *
                                (uint64_t)kInverseDegreeMontgomery);
  }
}

template <int X>
void vector_zero(vector<X> *out) {
  OPENSSL_memset(out, 0, sizeof(*out));
}

template <int X>
void vector_add(vector<X> *out, const vector<X> *lhs, const vector<X> *rhs) {
  for (int i = 0; i < X; i++) {
    scalar_add(&out->v[i], &lhs->v[i], &rhs->v[i]);
  }
}

template <int X>
void vector_sub(vector<X> *out, const vector<X> *lhs, const vector<X> *rhs) {
  for (int i = 0; i < X; i++) {
    scalar_sub(&out->v[i], &lhs->v[i], &rhs->v[i]);
  }
}

template <int X>
void vector_mult_scalar(vector<X> *out, const vector<X> *lhs,
                        const scalar *rhs) {
  for (int i = 0; i < X; i++) {
    scalar_mult(&out->v[i], &lhs->v[i], rhs);
  }
}

template <int X>
void vector_ntt(vector<X> *a) {
  for (int i = 0; i < X; i++) {
    scalar_ntt(&a->v[i]);
  }
}

template <int X>
void vector_inverse_ntt(vector<X> *a) {
  for (int i = 0; i < X; i++) {
    scalar_inverse_ntt(&a->v[i]);
  }
}

template <int K, int L>
void matrix_mult(vector<K> *out, const matrix<K, L> *m, const vector<L> *a) {
  vector_zero(out);
  for (int i = 0; i < K; i++) {
    for (int j = 0; j < L; j++) {
      scalar product;
      scalar_mult(&product, &m->v[i][j], &a->v[j]);
      scalar_add(&out->v[i], &out->v[i], &product);
    }
  }
}

void power2_round(uint32_t *r1, uint32_t *r0, uint32_t r) {
  *r1 = r >> kDroppedBits;
  *r0 = r - (*r1 << kDroppedBits);

  uint32_t r0_adjusted = mod_sub(*r0, 1 << kDroppedBits);
  uint32_t r1_adjusted = *r1 + 1;

  crypto_word_t mask =
      constant_time_lt_w((uint32_t)(1 << (kDroppedBits - 1)), *r0);

  *r0 = constant_time_select_int(mask, r0_adjusted, *r0);

  *r1 = constant_time_select_int(mask, r1_adjusted, *r1);
}

void scale_power2_round(uint32_t *out, uint32_t r1) {

  assert(r1 < (1u << 10));

  *out = r1 << kDroppedBits;

  assert(*out < kPrime);
}

uint32_t high_bits(uint32_t x) {

  uint32_t r1 = (x + 127) >> 7;
  r1 = (r1 * 1025 + (1 << 21)) >> 22;
  r1 &= 15;
  return r1;
}

void decompose(uint32_t *r1, int32_t *r0, uint32_t r) {
  *r1 = high_bits(r);

  *r0 = r;
  *r0 -= *r1 * 2 * (int32_t)kGamma2;
  *r0 -= (((int32_t)kHalfPrime - *r0) >> 31) & (int32_t)kPrime;
}

int32_t low_bits(uint32_t x) {
  uint32_t r1;
  int32_t r0;
  decompose(&r1, &r0, x);
  return r0;
}

int32_t make_hint(uint32_t ct0, uint32_t cs2, uint32_t w) {
  uint32_t r_plus_z = mod_sub(w, cs2);
  uint32_t r = reduce_once(r_plus_z + ct0);
  return high_bits(r) != high_bits(r_plus_z);
}

uint32_t use_hint_vartime(uint32_t h, uint32_t r) {
  uint32_t r1;
  int32_t r0;
  decompose(&r1, &r0, r);

  if (h) {
    if (r0 > 0) {

      return (r1 + 1) & 15;
    } else {
      return (r1 - 1) & 15;
    }
  }
  return r1;
}

void scalar_power2_round(scalar *s1, scalar *s0, const scalar *s) {
  for (int i = 0; i < kDegree; i++) {
    power2_round(&s1->c[i], &s0->c[i], s->c[i]);
  }
}

void scalar_scale_power2_round(scalar *out, const scalar *in) {
  for (int i = 0; i < kDegree; i++) {
    scale_power2_round(&out->c[i], in->c[i]);
  }
}

void scalar_high_bits(scalar *out, const scalar *in) {
  for (int i = 0; i < kDegree; i++) {
    out->c[i] = high_bits(in->c[i]);
  }
}

void scalar_low_bits(scalar *out, const scalar *in) {
  for (int i = 0; i < kDegree; i++) {
    out->c[i] = low_bits(in->c[i]);
  }
}

void scalar_max(uint32_t *max, const scalar *s) {
  for (int i = 0; i < kDegree; i++) {
    uint32_t abs = abs_mod_prime(s->c[i]);
    *max = maximum(*max, abs);
  }
}

void scalar_max_signed(uint32_t *max, const scalar *s) {
  for (int i = 0; i < kDegree; i++) {
    uint32_t abs = abs_signed(s->c[i]);
    *max = maximum(*max, abs);
  }
}

void scalar_make_hint(scalar *out, const scalar *ct0, const scalar *cs2,
                      const scalar *w) {
  for (int i = 0; i < kDegree; i++) {
    out->c[i] = make_hint(ct0->c[i], cs2->c[i], w->c[i]);
  }
}

void scalar_use_hint_vartime(scalar *out, const scalar *h, const scalar *r) {
  for (int i = 0; i < kDegree; i++) {
    out->c[i] = use_hint_vartime(h->c[i], r->c[i]);
  }
}

template <int X>
void vector_power2_round(vector<X> *t1, vector<X> *t0, const vector<X> *t) {
  for (int i = 0; i < X; i++) {
    scalar_power2_round(&t1->v[i], &t0->v[i], &t->v[i]);
  }
}

template <int X>
void vector_scale_power2_round(vector<X> *out, const vector<X> *in) {
  for (int i = 0; i < X; i++) {
    scalar_scale_power2_round(&out->v[i], &in->v[i]);
  }
}

template <int X>
void vector_high_bits(vector<X> *out, const vector<X> *in) {
  for (int i = 0; i < X; i++) {
    scalar_high_bits(&out->v[i], &in->v[i]);
  }
}

template <int X>
void vector_low_bits(vector<X> *out, const vector<X> *in) {
  for (int i = 0; i < X; i++) {
    scalar_low_bits(&out->v[i], &in->v[i]);
  }
}

template <int X>
uint32_t vector_max(const vector<X> *a) {
  uint32_t max = 0;
  for (int i = 0; i < X; i++) {
    scalar_max(&max, &a->v[i]);
  }
  return max;
}

template <int X>
uint32_t vector_max_signed(const vector<X> *a) {
  uint32_t max = 0;
  for (int i = 0; i < X; i++) {
    scalar_max_signed(&max, &a->v[i]);
  }
  return max;
}

template <int X>
size_t vector_count_ones(const vector<X> *a) {
  size_t count = 0;
  for (int i = 0; i < X; i++) {
    for (int j = 0; j < kDegree; j++) {
      count += a->v[i].c[j];
    }
  }
  return count;
}

template <int X>
void vector_make_hint(vector<X> *out, const vector<X> *ct0,
                      const vector<X> *cs2, const vector<X> *w) {
  for (int i = 0; i < X; i++) {
    scalar_make_hint(&out->v[i], &ct0->v[i], &cs2->v[i], &w->v[i]);
  }
}

template <int X>
void vector_use_hint_vartime(vector<X> *out, const vector<X> *h,
                             const vector<X> *r) {
  for (int i = 0; i < X; i++) {
    scalar_use_hint_vartime(&out->v[i], &h->v[i], &r->v[i]);
  }
}

static void scalar_encode_4(uint8_t out[128], const scalar *s) {

  static_assert(kDegree % 2 == 0, "kDegree must be a multiple of 2");
  for (int i = 0; i < kDegree / 2; i++) {
    uint32_t a = s->c[2 * i];
    uint32_t b = s->c[2 * i + 1];
    declassify_assert(a < 16);
    declassify_assert(b < 16);
    out[i] = a | (b << 4);
  }
}

void scalar_encode_10(uint8_t out[320], const scalar *s) {

  static_assert(kDegree % 4 == 0, "kDegree must be a multiple of 4");
  for (int i = 0; i < kDegree / 4; i++) {
    uint32_t a = s->c[4 * i];
    uint32_t b = s->c[4 * i + 1];
    uint32_t c = s->c[4 * i + 2];
    uint32_t d = s->c[4 * i + 3];
    declassify_assert(a < 1024);
    declassify_assert(b < 1024);
    declassify_assert(c < 1024);
    declassify_assert(d < 1024);
    out[5 * i] = (uint8_t)a;
    out[5 * i + 1] = (uint8_t)((a >> 8) | (b << 2));
    out[5 * i + 2] = (uint8_t)((b >> 6) | (c << 4));
    out[5 * i + 3] = (uint8_t)((c >> 4) | (d << 6));
    out[5 * i + 4] = (uint8_t)(d >> 2);
  }
}

void scalar_encode_signed_4_4(uint8_t out[128], const scalar *s) {

  static_assert(kDegree % 2 == 0, "kDegree must be a multiple of 2");
  for (int i = 0; i < kDegree / 2; i++) {
    uint32_t a = mod_sub(4, s->c[2 * i]);
    uint32_t b = mod_sub(4, s->c[2 * i + 1]);
    declassify_assert(a < 16);
    declassify_assert(b < 16);
    out[i] = a | (b << 4);
  }
}

static void scalar_encode_signed_3_2(uint8_t out[96], const scalar *s) {
  static_assert(kDegree % 8 == 0, "kDegree must be a multiple of 8");
  for (int i = 0; i < kDegree / 8; i++) {
    uint32_t a = mod_sub(2, s->c[8 * i]);
    uint32_t b = mod_sub(2, s->c[8 * i + 1]);
    uint32_t c = mod_sub(2, s->c[8 * i + 2]);
    uint32_t d = mod_sub(2, s->c[8 * i + 3]);
    uint32_t e = mod_sub(2, s->c[8 * i + 4]);
    uint32_t f = mod_sub(2, s->c[8 * i + 5]);
    uint32_t g = mod_sub(2, s->c[8 * i + 6]);
    uint32_t h = mod_sub(2, s->c[8 * i + 7]);
    uint32_t v = (h << 21) | (g << 18) | (f << 15) | (e << 12) | (d << 9) |
                 (c << 6) | (b << 3) | a;
    uint8_t v_bytes[sizeof(v)];
    CRYPTO_store_u32_le(v_bytes, v);
    OPENSSL_memcpy(&out[i * 3], v_bytes, 3);
  }
}

void scalar_encode_signed_13_12(uint8_t out[416], const scalar *s) {
  static const uint32_t kMax = 1u << 12;

  static_assert(kDegree % 8 == 0, "kDegree must be a multiple of 8");
  for (int i = 0; i < kDegree / 8; i++) {
    uint32_t a = mod_sub(kMax, s->c[8 * i]);
    uint32_t b = mod_sub(kMax, s->c[8 * i + 1]);
    uint32_t c = mod_sub(kMax, s->c[8 * i + 2]);
    uint32_t d = mod_sub(kMax, s->c[8 * i + 3]);
    uint32_t e = mod_sub(kMax, s->c[8 * i + 4]);
    uint32_t f = mod_sub(kMax, s->c[8 * i + 5]);
    uint32_t g = mod_sub(kMax, s->c[8 * i + 6]);
    uint32_t h = mod_sub(kMax, s->c[8 * i + 7]);
    declassify_assert(a < (1u << 13));
    declassify_assert(b < (1u << 13));
    declassify_assert(c < (1u << 13));
    declassify_assert(d < (1u << 13));
    declassify_assert(e < (1u << 13));
    declassify_assert(f < (1u << 13));
    declassify_assert(g < (1u << 13));
    declassify_assert(h < (1u << 13));
    a |= b << 13;
    a |= c << 26;
    c >>= 6;
    c |= d << 7;
    c |= e << 20;
    e >>= 12;
    e |= f << 1;
    e |= g << 14;
    e |= h << 27;
    h >>= 5;
    OPENSSL_memcpy(&out[13 * i], &a, sizeof(a));
    OPENSSL_memcpy(&out[13 * i + 4], &c, sizeof(c));
    OPENSSL_memcpy(&out[13 * i + 8], &e, sizeof(e));
    OPENSSL_memcpy(&out[13 * i + 12], &h, 1);
  }
}

void scalar_encode_signed_20_19(uint8_t out[640], const scalar *s) {
  static const uint32_t kMax = 1u << 19;

  static_assert(kDegree % 4 == 0, "kDegree must be a multiple of 4");
  for (int i = 0; i < kDegree / 4; i++) {
    uint32_t a = mod_sub(kMax, s->c[4 * i]);
    uint32_t b = mod_sub(kMax, s->c[4 * i + 1]);
    uint32_t c = mod_sub(kMax, s->c[4 * i + 2]);
    uint32_t d = mod_sub(kMax, s->c[4 * i + 3]);
    declassify_assert(a < (1u << 20));
    declassify_assert(b < (1u << 20));
    declassify_assert(c < (1u << 20));
    declassify_assert(d < (1u << 20));
    a |= b << 20;
    b >>= 12;
    b |= c << 8;
    b |= d << 28;
    d >>= 4;
    OPENSSL_memcpy(&out[10 * i], &a, sizeof(a));
    OPENSSL_memcpy(&out[10 * i + 4], &b, sizeof(b));
    OPENSSL_memcpy(&out[10 * i + 8], &d, 2);
  }
}

void scalar_encode_signed(uint8_t *out, const scalar *s, int bits,
                          uint32_t max) {
  if (bits == 3) {
    assert(max == 2);
    scalar_encode_signed_3_2(out, s);
  } else if (bits == 4) {
    assert(max == 4);
    scalar_encode_signed_4_4(out, s);
  } else if (bits == 20) {
    assert(max == 1u << 19);
    scalar_encode_signed_20_19(out, s);
  } else {
    assert(bits == 13);
    assert(max == 1u << 12);
    scalar_encode_signed_13_12(out, s);
  }
}

void scalar_decode_10(scalar *out, const uint8_t in[320]) {
  uint32_t v;
  static_assert(kDegree % 4 == 0, "kDegree must be a multiple of 4");
  for (int i = 0; i < kDegree / 4; i++) {
    OPENSSL_memcpy(&v, &in[5 * i], sizeof(v));
    out->c[4 * i] = v & 0x3ff;
    out->c[4 * i + 1] = (v >> 10) & 0x3ff;
    out->c[4 * i + 2] = (v >> 20) & 0x3ff;
    out->c[4 * i + 3] = (v >> 30) | (((uint32_t)in[5 * i + 4]) << 2);
  }
}

int scalar_decode_signed_4_4(scalar *out, const uint8_t in[128]) {
  uint32_t v;
  static_assert(kDegree % 8 == 0, "kDegree must be a multiple of 8");
  for (int i = 0; i < kDegree / 8; i++) {
    OPENSSL_memcpy(&v, &in[4 * i], sizeof(v));

    const uint32_t msbs = v & 0x88888888u;

    const uint32_t mask = (msbs >> 1) | (msbs >> 2) | (msbs >> 3);

    if (constant_time_declassify_int((mask & v) != 0)) {
      return 0;
    }

    out->c[i * 8] = mod_sub(4, v & 15);
    out->c[i * 8 + 1] = mod_sub(4, (v >> 4) & 15);
    out->c[i * 8 + 2] = mod_sub(4, (v >> 8) & 15);
    out->c[i * 8 + 3] = mod_sub(4, (v >> 12) & 15);
    out->c[i * 8 + 4] = mod_sub(4, (v >> 16) & 15);
    out->c[i * 8 + 5] = mod_sub(4, (v >> 20) & 15);
    out->c[i * 8 + 6] = mod_sub(4, (v >> 24) & 15);
    out->c[i * 8 + 7] = mod_sub(4, v >> 28);
  }
  return 1;
}

static int scalar_decode_signed_3_2(scalar *out, const uint8_t in[96]) {
  uint32_t v;
  uint8_t v_bytes[sizeof(v)] = {0};
  static_assert(kDegree % 8 == 0, "kDegree must be a multiple of 8");
  for (int i = 0; i < kDegree / 8; i++) {
    OPENSSL_memcpy(v_bytes, &in[3 * i], 3);
    v = CRYPTO_load_u32_le(v_bytes);

    const uint32_t msbs = v & 000044444444u;

    const uint32_t mask = (msbs >> 1) | (msbs >> 2);

    if (constant_time_declassify_int((mask & v) != 0)) {
      return 0;
    }

    out->c[i * 8 + 0] = mod_sub(2, (v >> 0) & 7);
    out->c[i * 8 + 1] = mod_sub(2, (v >> 3) & 7);
    out->c[i * 8 + 2] = mod_sub(2, (v >> 6) & 7);
    out->c[i * 8 + 3] = mod_sub(2, (v >> 9) & 7);
    out->c[i * 8 + 4] = mod_sub(2, (v >> 12) & 7);
    out->c[i * 8 + 5] = mod_sub(2, (v >> 15) & 7);
    out->c[i * 8 + 6] = mod_sub(2, (v >> 18) & 7);
    out->c[i * 8 + 7] = mod_sub(2, v >> 21);
  }
  return 1;
}

void scalar_decode_signed_13_12(scalar *out, const uint8_t in[416]) {
  static const uint32_t kMax = 1u << 12;
  static const uint32_t k13Bits = (1u << 13) - 1;
  static const uint32_t k7Bits = (1u << 7) - 1;

  uint32_t a, b, c;
  uint8_t d;
  static_assert(kDegree % 8 == 0, "kDegree must be a multiple of 8");
  for (int i = 0; i < kDegree / 8; i++) {
    OPENSSL_memcpy(&a, &in[13 * i], sizeof(a));
    OPENSSL_memcpy(&b, &in[13 * i + 4], sizeof(b));
    OPENSSL_memcpy(&c, &in[13 * i + 8], sizeof(c));
    d = in[13 * i + 12];

    out->c[i * 8] = mod_sub(kMax, a & k13Bits);
    out->c[i * 8 + 1] = mod_sub(kMax, (a >> 13) & k13Bits);
    out->c[i * 8 + 2] = mod_sub(kMax, (a >> 26) | ((b & k7Bits) << 6));
    out->c[i * 8 + 3] = mod_sub(kMax, (b >> 7) & k13Bits);
    out->c[i * 8 + 4] = mod_sub(kMax, (b >> 20) | ((c & 1) << 12));
    out->c[i * 8 + 5] = mod_sub(kMax, (c >> 1) & k13Bits);
    out->c[i * 8 + 6] = mod_sub(kMax, (c >> 14) & k13Bits);
    out->c[i * 8 + 7] = mod_sub(kMax, (c >> 27) | ((uint32_t)d) << 5);
  }
}

void scalar_decode_signed_20_19(scalar *out, const uint8_t in[640]) {
  static const uint32_t kMax = 1u << 19;
  static const uint32_t k20Bits = (1u << 20) - 1;

  uint32_t a, b;
  uint16_t c;
  static_assert(kDegree % 4 == 0, "kDegree must be a multiple of 4");
  for (int i = 0; i < kDegree / 4; i++) {
    OPENSSL_memcpy(&a, &in[10 * i], sizeof(a));
    OPENSSL_memcpy(&b, &in[10 * i + 4], sizeof(b));
    OPENSSL_memcpy(&c, &in[10 * i + 8], sizeof(c));

    out->c[i * 4] = mod_sub(kMax, a & k20Bits);
    out->c[i * 4 + 1] = mod_sub(kMax, (a >> 20) | ((b & 0xff) << 12));
    out->c[i * 4 + 2] = mod_sub(kMax, (b >> 8) & k20Bits);
    out->c[i * 4 + 3] = mod_sub(kMax, (b >> 28) | ((uint32_t)c) << 4);
  }
}

int scalar_decode_signed(scalar *out, const uint8_t *in, int bits,
                         uint32_t max) {
  if (bits == 3) {
    assert(max == 2);
    return scalar_decode_signed_3_2(out, in);
  } else if (bits == 4) {
    assert(max == 4);
    return scalar_decode_signed_4_4(out, in);
  } else if (bits == 13) {
    assert(max == (1u << 12));
    scalar_decode_signed_13_12(out, in);
    return 1;
  } else if (bits == 20) {
    assert(max == (1u << 19));
    scalar_decode_signed_20_19(out, in);
    return 1;
  } else {
    abort();
  }
}

void scalar_from_keccak_vartime(scalar *out,
                                const uint8_t derived_seed[kRhoBytes + 2]) {
  struct BORINGSSL_keccak_st keccak_ctx;
  BORINGSSL_keccak_init(&keccak_ctx, boringssl_shake128);
  BORINGSSL_keccak_absorb(&keccak_ctx, derived_seed, kRhoBytes + 2);
  assert(keccak_ctx.squeeze_offset == 0);
  assert(keccak_ctx.rate_bytes == 168);
  static_assert(168 % 3 == 0, "block and coefficient boundaries do not align");

  int done = 0;
  while (done < kDegree) {
    uint8_t block[168];
    BORINGSSL_keccak_squeeze(&keccak_ctx, block, sizeof(block));
    for (size_t i = 0; i < sizeof(block) && done < kDegree; i += 3) {

      uint32_t value = (uint32_t)block[i] | ((uint32_t)block[i + 1] << 8) |
                       (((uint32_t)block[i + 2] & 0x7f) << 16);
      if (value < kPrime) {
        out->c[done++] = value;
      }
    }
  }
}

template <int ETA>
static bool coefficient_from_nibble(uint32_t nibble, uint32_t *result);

template <>
bool coefficient_from_nibble<4>(uint32_t nibble, uint32_t *result) {
  if (constant_time_declassify_int(nibble < 9)) {
    *result = mod_sub(4, nibble);
    return true;
  }
  return false;
}

template <>
bool coefficient_from_nibble<2>(uint32_t nibble, uint32_t *result) {
  if (constant_time_declassify_int(nibble < 15)) {
    *result = mod_sub(2, nibble % 5);
    return true;
  }
  return false;
}

template <int ETA>
void scalar_uniform(scalar *out, const uint8_t derived_seed[kSigmaBytes + 2]) {
  struct BORINGSSL_keccak_st keccak_ctx;
  BORINGSSL_keccak_init(&keccak_ctx, boringssl_shake256);
  BORINGSSL_keccak_absorb(&keccak_ctx, derived_seed, kSigmaBytes + 2);
  assert(keccak_ctx.squeeze_offset == 0);
  assert(keccak_ctx.rate_bytes == 136);

  int done = 0;
  while (done < kDegree) {
    uint8_t block[136];
    BORINGSSL_keccak_squeeze(&keccak_ctx, block, sizeof(block));
    for (size_t i = 0; i < sizeof(block) && done < kDegree; ++i) {
      uint32_t t0 = block[i] & 0x0F;
      uint32_t t1 = block[i] >> 4;

      uint32_t v;
      if (coefficient_from_nibble<ETA>(t0, &v)) {
        out->c[done++] = v;
      }
      if (done < kDegree && coefficient_from_nibble<ETA>(t1, &v)) {
        out->c[done++] = v;
      }
    }
  }
}

void scalar_sample_mask(scalar *out,
                        const uint8_t derived_seed[kRhoPrimeBytes + 2]) {
  uint8_t buf[640];
  BORINGSSL_keccak(buf, sizeof(buf), derived_seed, kRhoPrimeBytes + 2,
                   boringssl_shake256);

  scalar_decode_signed_20_19(out, buf);
}

void scalar_sample_in_ball_vartime(scalar *out, const uint8_t *seed, int len,
                                   int tau) {
  struct BORINGSSL_keccak_st keccak_ctx;
  BORINGSSL_keccak_init(&keccak_ctx, boringssl_shake256);
  BORINGSSL_keccak_absorb(&keccak_ctx, seed, len);
  assert(keccak_ctx.squeeze_offset == 0);
  assert(keccak_ctx.rate_bytes == 136);

  uint8_t block[136];
  BORINGSSL_keccak_squeeze(&keccak_ctx, block, sizeof(block));

  uint64_t signs = CRYPTO_load_u64_le(block);
  int offset = 8;

  CONSTTIME_DECLASSIFY(block + offset, sizeof(block) - offset);

  OPENSSL_memset(out, 0, sizeof(*out));
  for (size_t i = kDegree - tau; i < kDegree; i++) {
    size_t byte;
    for (;;) {
      if (offset == 136) {
        BORINGSSL_keccak_squeeze(&keccak_ctx, block, sizeof(block));

        CONSTTIME_DECLASSIFY(block, sizeof(block));
        offset = 0;
      }

      byte = block[offset++];
      if (byte <= i) {
        break;
      }
    }

    out->c[i] = out->c[byte];
    out->c[byte] = mod_sub(1, 2 * (signs & 1));
    signs >>= 1;
  }
}

template <int K, int L>
void matrix_expand(matrix<K, L> *out, const uint8_t rho[kRhoBytes]) {
  static_assert(K <= 0x100, "K must fit in 8 bits");
  static_assert(L <= 0x100, "L must fit in 8 bits");

  uint8_t derived_seed[kRhoBytes + 2];
  OPENSSL_memcpy(derived_seed, rho, kRhoBytes);
  for (int i = 0; i < K; i++) {
    for (int j = 0; j < L; j++) {
      derived_seed[kRhoBytes + 1] = (uint8_t)i;
      derived_seed[kRhoBytes] = (uint8_t)j;
      scalar_from_keccak_vartime(&out->v[i][j], derived_seed);
    }
  }
}

template <int K, int L>
void vector_expand_short(vector<L> *s1, vector<K> *s2,
                         const uint8_t sigma[kSigmaBytes]) {
  static_assert(K <= 0x100, "K must fit in 8 bits");
  static_assert(L <= 0x100, "L must fit in 8 bits");
  static_assert(K + L <= 0x100, "K+L must fit in 8 bits");

  uint8_t derived_seed[kSigmaBytes + 2];
  OPENSSL_memcpy(derived_seed, sigma, kSigmaBytes);
  derived_seed[kSigmaBytes] = 0;
  derived_seed[kSigmaBytes + 1] = 0;
  for (int i = 0; i < L; i++) {
    scalar_uniform<eta<K>()>(&s1->v[i], derived_seed);
    ++derived_seed[kSigmaBytes];
  }
  for (int i = 0; i < K; i++) {
    scalar_uniform<eta<K>()>(&s2->v[i], derived_seed);
    ++derived_seed[kSigmaBytes];
  }
}

template <int L>
void vector_expand_mask(vector<L> *out, const uint8_t seed[kRhoPrimeBytes],
                        size_t kappa) {
  assert(kappa + L <= 0x10000);

  uint8_t derived_seed[kRhoPrimeBytes + 2];
  OPENSSL_memcpy(derived_seed, seed, kRhoPrimeBytes);
  for (int i = 0; i < L; i++) {
    size_t index = kappa + i;
    derived_seed[kRhoPrimeBytes] = index & 0xFF;
    derived_seed[kRhoPrimeBytes + 1] = (index >> 8) & 0xFF;
    scalar_sample_mask(&out->v[i], derived_seed);
  }
}

template <int K>
void vector_encode(uint8_t *out, const vector<K> *a, int bits) {
  if (bits == 4) {
    for (int i = 0; i < K; i++) {
      scalar_encode_4(out + i * bits * kDegree / 8, &a->v[i]);
    }
  } else {
    assert(bits == 10);
    for (int i = 0; i < K; i++) {
      scalar_encode_10(out + i * bits * kDegree / 8, &a->v[i]);
    }
  }
}

template <int K>
void vector_decode_10(vector<K> *out, const uint8_t *in) {
  for (int i = 0; i < K; i++) {
    scalar_decode_10(&out->v[i], in + i * 10 * kDegree / 8);
  }
}

template <int X>
void vector_encode_signed(uint8_t *out, const vector<X> *a, int bits,
                          uint32_t max) {
  for (int i = 0; i < X; i++) {
    scalar_encode_signed(out + i * bits * kDegree / 8, &a->v[i], bits, max);
  }
}

template <int X>
int vector_decode_signed(vector<X> *out, const uint8_t *in, int bits,
                         uint32_t max) {
  for (int i = 0; i < X; i++) {
    if (!scalar_decode_signed(&out->v[i], in + i * bits * kDegree / 8, bits,
                              max)) {
      return 0;
    }
  }
  return 1;
}

template <int K>
void w1_encode(uint8_t out[128 * K], const vector<K> *w1) {
  vector_encode(out, w1, 4);
}

template <int K>
void hint_bit_pack(uint8_t out[omega<K>() + K], const vector<K> *h) {
  OPENSSL_memset(out, 0, omega<K>() + K);
  int index = 0;
  for (int i = 0; i < K; i++) {
    for (int j = 0; j < kDegree; j++) {
      if (h->v[i].c[j]) {

        BSSL_CHECK(index < omega<K>());
        out[index++] = j;
      }
    }
    out[omega<K>() + i] = index;
  }
}

template <int K>
int hint_bit_unpack(vector<K> *h, const uint8_t in[omega<K>() + K]) {
  vector_zero(h);
  int index = 0;
  for (int i = 0; i < K; i++) {
    const int limit = in[omega<K>() + i];
    if (limit < index || limit > omega<K>()) {
      return 0;
    }

    int last = -1;
    while (index < limit) {
      int byte = in[index++];
      if (last >= 0 && byte <= last) {
        return 0;
      }
      last = byte;
      static_assert(kDegree == 256,
                    "kDegree must be 256 for this write to be in bounds");
      h->v[i].c[byte] = 1;
    }
  }
  for (; index < omega<K>(); index++) {
    if (in[index] != 0) {
      return 0;
    }
  }
  return 1;
}

template <int K>
struct public_key {
  uint8_t rho[kRhoBytes];
  vector<K> t1;

  uint8_t public_key_hash[kTrBytes];
};

template <int K, int L>
struct private_key {
  uint8_t rho[kRhoBytes];
  uint8_t k[kKBytes];
  uint8_t public_key_hash[kTrBytes];
  vector<L> s1;
  vector<K> s2;
  vector<K> t0;
};

template <int K, int L>
struct signature {
  uint8_t c_tilde[2 * lambda_bytes<K>()];
  vector<L> z;
  vector<K> h;
};

template <int K>
int mldsa_marshal_public_key(CBB *out, const struct public_key<K> *pub) {
  if (!CBB_add_bytes(out, pub->rho, sizeof(pub->rho))) {
    return 0;
  }

  uint8_t *vectork_output;
  if (!CBB_add_space(out, &vectork_output, 320 * K)) {
    return 0;
  }
  vector_encode(vectork_output, &pub->t1, 10);

  return 1;
}

template <int K>
int mldsa_parse_public_key(struct public_key<K> *pub, CBS *in) {
  const CBS orig_in = *in;

  if (!CBS_copy_bytes(in, pub->rho, sizeof(pub->rho))) {
    return 0;
  }

  CBS t1_bytes;
  if (!CBS_get_bytes(in, &t1_bytes, 320 * K) || CBS_len(in) != 0) {
    return 0;
  }
  vector_decode_10(&pub->t1, CBS_data(&t1_bytes));

  BORINGSSL_keccak(pub->public_key_hash, sizeof(pub->public_key_hash),
                   CBS_data(&orig_in), CBS_len(&orig_in), boringssl_shake256);

  return 1;
}

template <int K, int L>
int mldsa_marshal_private_key(CBB *out, const struct private_key<K, L> *priv) {
  if (!CBB_add_bytes(out, priv->rho, sizeof(priv->rho)) ||
      !CBB_add_bytes(out, priv->k, sizeof(priv->k)) ||
      !CBB_add_bytes(out, priv->public_key_hash,
                     sizeof(priv->public_key_hash))) {
    return 0;
  }

  constexpr size_t scalar_bytes =
      (kDegree * plus_minus_eta_bitlen<K>() + 7) / 8;
  uint8_t *vectorl_output;
  if (!CBB_add_space(out, &vectorl_output, scalar_bytes * L)) {
    return 0;
  }
  vector_encode_signed(vectorl_output, &priv->s1, plus_minus_eta_bitlen<K>(),
                       eta<K>());

  uint8_t *s2_output;
  if (!CBB_add_space(out, &s2_output, scalar_bytes * K)) {
    return 0;
  }
  vector_encode_signed(s2_output, &priv->s2, plus_minus_eta_bitlen<K>(),
                       eta<K>());

  uint8_t *t0_output;
  if (!CBB_add_space(out, &t0_output, 416 * K)) {
    return 0;
  }
  vector_encode_signed(t0_output, &priv->t0, 13, 1 << 12);

  return 1;
}

template <int K, int L>
int mldsa_parse_private_key(struct private_key<K, L> *priv, CBS *in) {
  CBS s1_bytes;
  CBS s2_bytes;
  CBS t0_bytes;
  constexpr size_t scalar_bytes =
      (kDegree * plus_minus_eta_bitlen<K>() + 7) / 8;
  if (!CBS_copy_bytes(in, priv->rho, sizeof(priv->rho)) ||
      !CBS_copy_bytes(in, priv->k, sizeof(priv->k)) ||
      !CBS_copy_bytes(in, priv->public_key_hash,
                      sizeof(priv->public_key_hash)) ||
      !CBS_get_bytes(in, &s1_bytes, scalar_bytes * L) ||
      !vector_decode_signed(&priv->s1, CBS_data(&s1_bytes),
                            plus_minus_eta_bitlen<K>(), eta<K>()) ||
      !CBS_get_bytes(in, &s2_bytes, scalar_bytes * K) ||
      !vector_decode_signed(&priv->s2, CBS_data(&s2_bytes),
                            plus_minus_eta_bitlen<K>(), eta<K>()) ||
      !CBS_get_bytes(in, &t0_bytes, 416 * K) ||

      !vector_decode_signed(&priv->t0, CBS_data(&t0_bytes), 13, 1 << 12)) {
    return 0;
  }

  return 1;
}

template <int K, int L>
int mldsa_marshal_signature(CBB *out, const struct signature<K, L> *sign) {
  if (!CBB_add_bytes(out, sign->c_tilde, sizeof(sign->c_tilde))) {
    return 0;
  }

  uint8_t *vectorl_output;
  if (!CBB_add_space(out, &vectorl_output, 640 * L)) {
    return 0;
  }
  vector_encode_signed(vectorl_output, &sign->z, 20, 1 << 19);

  uint8_t *hint_output;
  if (!CBB_add_space(out, &hint_output, omega<K>() + K)) {
    return 0;
  }
  hint_bit_pack(hint_output, &sign->h);

  return 1;
}

template <int K, int L>
int mldsa_parse_signature(struct signature<K, L> *sign, CBS *in) {
  CBS z_bytes;
  CBS hint_bytes;
  if (!CBS_copy_bytes(in, sign->c_tilde, sizeof(sign->c_tilde)) ||
      !CBS_get_bytes(in, &z_bytes, 640 * L) ||

      !vector_decode_signed(&sign->z, CBS_data(&z_bytes), 20, 1 << 19) ||
      !CBS_get_bytes(in, &hint_bytes, omega<K>() + K) ||
      !hint_bit_unpack(&sign->h, CBS_data(&hint_bytes))) {
    return 0;
  };

  return 1;
}

template <typename T>
struct DeleterFree {
  void operator()(T *ptr) { OPENSSL_free(ptr); }
};

template <int K, int L>
int mldsa_generate_key_external_entropy_no_self_test(
    uint8_t out_encoded_public_key[public_key_bytes<K>()],
    struct private_key<K, L> *priv,
    const uint8_t entropy[BCM_MLDSA_SEED_BYTES]) {

  struct values_st {
    struct public_key<K> pub;
    matrix<K, L> a_ntt;
    vector<L> s1_ntt;
    vector<K> t;
  };
  std::unique_ptr<values_st, DeleterFree<values_st>> values(
      reinterpret_cast<struct values_st *>(OPENSSL_malloc(sizeof(values_st))));
  if (values == NULL) {
    return 0;
  }

  uint8_t augmented_entropy[BCM_MLDSA_SEED_BYTES + 2];
  OPENSSL_memcpy(augmented_entropy, entropy, BCM_MLDSA_SEED_BYTES);

  augmented_entropy[BCM_MLDSA_SEED_BYTES] = K;
  augmented_entropy[BCM_MLDSA_SEED_BYTES + 1] = L;
  uint8_t expanded_seed[kRhoBytes + kSigmaBytes + kKBytes];
  BORINGSSL_keccak(expanded_seed, sizeof(expanded_seed), augmented_entropy,
                   sizeof(augmented_entropy), boringssl_shake256);
  const uint8_t *const rho = expanded_seed;
  const uint8_t *const sigma = expanded_seed + kRhoBytes;
  const uint8_t *const k = expanded_seed + kRhoBytes + kSigmaBytes;

  CONSTTIME_DECLASSIFY(rho, kRhoBytes);
  OPENSSL_memcpy(values->pub.rho, rho, sizeof(values->pub.rho));
  OPENSSL_memcpy(priv->rho, rho, sizeof(priv->rho));
  OPENSSL_memcpy(priv->k, k, sizeof(priv->k));

  matrix_expand(&values->a_ntt, rho);
  vector_expand_short(&priv->s1, &priv->s2, sigma);

  OPENSSL_memcpy(&values->s1_ntt, &priv->s1, sizeof(values->s1_ntt));
  vector_ntt(&values->s1_ntt);

  matrix_mult(&values->t, &values->a_ntt, &values->s1_ntt);
  vector_inverse_ntt(&values->t);
  vector_add(&values->t, &values->t, &priv->s2);

  vector_power2_round(&values->pub.t1, &priv->t0, &values->t);

  CONSTTIME_DECLASSIFY(&values->pub.t1, sizeof(values->pub.t1));

  CBB cbb;
  CBB_init_fixed(&cbb, out_encoded_public_key, public_key_bytes<K>());
  if (!mldsa_marshal_public_key(&cbb, &values->pub)) {
    return 0;
  }
  assert(CBB_len(&cbb) == public_key_bytes<K>());

  BORINGSSL_keccak(priv->public_key_hash, sizeof(priv->public_key_hash),
                   out_encoded_public_key, public_key_bytes<K>(),
                   boringssl_shake256);

  return 1;
}

template <int K, int L>
int mldsa_generate_key_external_entropy(
    uint8_t out_encoded_public_key[public_key_bytes<K>()],
    struct private_key<K, L> *priv,
    const uint8_t entropy[BCM_MLDSA_SEED_BYTES]) {
  fips::ensure_keygen_self_test();
  return mldsa_generate_key_external_entropy_no_self_test(
      out_encoded_public_key, priv, entropy);
}

template <int K, int L>
int mldsa_public_from_private(struct public_key<K> *pub,
                              const struct private_key<K, L> *priv) {

  struct values_st {
    matrix<K, L> a_ntt;
    vector<L> s1_ntt;
    vector<K> t;
    vector<K> t0;
  };
  std::unique_ptr<values_st, DeleterFree<values_st>> values(
      reinterpret_cast<struct values_st *>(OPENSSL_malloc(sizeof(values_st))));
  if (values == NULL) {
    return 0;
  }

  OPENSSL_memcpy(pub->rho, priv->rho, sizeof(pub->rho));
  OPENSSL_memcpy(pub->public_key_hash, priv->public_key_hash,
                 sizeof(pub->public_key_hash));

  matrix_expand(&values->a_ntt, priv->rho);

  OPENSSL_memcpy(&values->s1_ntt, &priv->s1, sizeof(values->s1_ntt));
  vector_ntt(&values->s1_ntt);

  matrix_mult(&values->t, &values->a_ntt, &values->s1_ntt);
  vector_inverse_ntt(&values->t);
  vector_add(&values->t, &values->t, &priv->s2);

  vector_power2_round(&pub->t1, &values->t0, &values->t);

  CONSTTIME_DECLASSIFY(&pub->t1, sizeof(pub->t1));
  return 1;
}

template <int K, int L>
int mldsa_sign_internal_no_self_test(
    uint8_t out_encoded_signature[signature_bytes<K>()],
    const struct private_key<K, L> *priv, const uint8_t *msg, size_t msg_len,
    const uint8_t *context_prefix, size_t context_prefix_len,
    const uint8_t *context, size_t context_len,
    const uint8_t randomizer[BCM_MLDSA_SIGNATURE_RANDOMIZER_BYTES]) {
  uint8_t mu[kMuBytes];
  struct BORINGSSL_keccak_st keccak_ctx;
  BORINGSSL_keccak_init(&keccak_ctx, boringssl_shake256);
  BORINGSSL_keccak_absorb(&keccak_ctx, priv->public_key_hash,
                          sizeof(priv->public_key_hash));
  BORINGSSL_keccak_absorb(&keccak_ctx, context_prefix, context_prefix_len);
  BORINGSSL_keccak_absorb(&keccak_ctx, context, context_len);
  BORINGSSL_keccak_absorb(&keccak_ctx, msg, msg_len);
  BORINGSSL_keccak_squeeze(&keccak_ctx, mu, kMuBytes);

  uint8_t rho_prime[kRhoPrimeBytes];
  BORINGSSL_keccak_init(&keccak_ctx, boringssl_shake256);
  BORINGSSL_keccak_absorb(&keccak_ctx, priv->k, sizeof(priv->k));
  BORINGSSL_keccak_absorb(&keccak_ctx, randomizer,
                          BCM_MLDSA_SIGNATURE_RANDOMIZER_BYTES);
  BORINGSSL_keccak_absorb(&keccak_ctx, mu, kMuBytes);
  BORINGSSL_keccak_squeeze(&keccak_ctx, rho_prime, kRhoPrimeBytes);

  struct values_st {
    struct signature<K, L> sign;
    vector<L> s1_ntt;
    vector<K> s2_ntt;
    vector<K> t0_ntt;
    matrix<K, L> a_ntt;
    vector<L> y;
    vector<K> w;
    vector<K> w1;
    vector<L> cs1;
    vector<K> cs2;
  };
  std::unique_ptr<values_st, DeleterFree<values_st>> values(
      reinterpret_cast<struct values_st *>(OPENSSL_malloc(sizeof(values_st))));
  if (values == NULL) {
    return 0;
  }
  OPENSSL_memcpy(&values->s1_ntt, &priv->s1, sizeof(values->s1_ntt));
  vector_ntt(&values->s1_ntt);

  OPENSSL_memcpy(&values->s2_ntt, &priv->s2, sizeof(values->s2_ntt));
  vector_ntt(&values->s2_ntt);

  OPENSSL_memcpy(&values->t0_ntt, &priv->t0, sizeof(values->t0_ntt));
  vector_ntt(&values->t0_ntt);

  matrix_expand(&values->a_ntt, priv->rho);

  for (size_t kappa = 0;; kappa += L) {
    vector_expand_mask(&values->y, rho_prime, kappa);

    vector<L> *y_ntt = &values->cs1;
    OPENSSL_memcpy(y_ntt, &values->y, sizeof(*y_ntt));
    vector_ntt(y_ntt);

    matrix_mult(&values->w, &values->a_ntt, y_ntt);
    vector_inverse_ntt(&values->w);

    vector_high_bits(&values->w1, &values->w);
    uint8_t w1_encoded[128 * K];
    w1_encode(w1_encoded, &values->w1);

    BORINGSSL_keccak_init(&keccak_ctx, boringssl_shake256);
    BORINGSSL_keccak_absorb(&keccak_ctx, mu, kMuBytes);
    BORINGSSL_keccak_absorb(&keccak_ctx, w1_encoded, 128 * K);
    BORINGSSL_keccak_squeeze(&keccak_ctx, values->sign.c_tilde,
                             2 * lambda_bytes<K>());

    scalar c_ntt;
    scalar_sample_in_ball_vartime(&c_ntt, values->sign.c_tilde,
                                  sizeof(values->sign.c_tilde), tau<K>());
    scalar_ntt(&c_ntt);

    vector_mult_scalar(&values->cs1, &values->s1_ntt, &c_ntt);
    vector_inverse_ntt(&values->cs1);
    vector_mult_scalar(&values->cs2, &values->s2_ntt, &c_ntt);
    vector_inverse_ntt(&values->cs2);

    vector_add(&values->sign.z, &values->y, &values->cs1);

    vector<K> *r0 = &values->w1;
    vector_sub(r0, &values->w, &values->cs2);
    vector_low_bits(r0, r0);

    uint32_t z_max = vector_max(&values->sign.z);
    uint32_t r0_max = vector_max_signed(r0);
    if (constant_time_declassify_w(
            constant_time_ge_w(z_max, gamma1<K>() - beta<K>()) |
            constant_time_ge_w(r0_max, kGamma2 - beta<K>()))) {
#if defined(BORINGSSL_FIPS_BREAK_TESTS)

      printf("MLDSA signature restart case 1.\n");
#endif
      continue;
    }

    vector<K> *ct0 = &values->w1;
    vector_mult_scalar(ct0, &values->t0_ntt, &c_ntt);
    vector_inverse_ntt(ct0);
    vector_make_hint(&values->sign.h, ct0, &values->cs2, &values->w);

    uint32_t ct0_max = vector_max(ct0);
    size_t h_ones = vector_count_ones(&values->sign.h);
    if (constant_time_declassify_w(constant_time_ge_w(ct0_max, kGamma2) |
                                   constant_time_lt_w(omega<K>(), h_ones))) {
#if defined(BORINGSSL_FIPS_BREAK_TESTS)

      printf("MLDSA signature restart case 2.\n");
#endif
      continue;
    }

    CONSTTIME_DECLASSIFY(values->sign.c_tilde, sizeof(values->sign.c_tilde));
    CONSTTIME_DECLASSIFY(&values->sign.z, sizeof(values->sign.z));
    CONSTTIME_DECLASSIFY(&values->sign.h, sizeof(values->sign.h));

    CBB cbb;
    CBB_init_fixed(&cbb, out_encoded_signature, signature_bytes<K>());
    if (!mldsa_marshal_signature(&cbb, &values->sign)) {
      return 0;
    }

    BSSL_CHECK(CBB_len(&cbb) == signature_bytes<K>());
    return 1;
  }
}

template <int K, int L>
int mldsa_sign_internal(
    uint8_t out_encoded_signature[signature_bytes<K>()],
    const struct private_key<K, L> *priv, const uint8_t *msg, size_t msg_len,
    const uint8_t *context_prefix, size_t context_prefix_len,
    const uint8_t *context, size_t context_len,
    const uint8_t randomizer[BCM_MLDSA_SIGNATURE_RANDOMIZER_BYTES]) {
  fips::ensure_sign_self_test();
  return mldsa_sign_internal_no_self_test(
      out_encoded_signature, priv, msg, msg_len, context_prefix,
      context_prefix_len, context, context_len, randomizer);
}

template <int K, int L>
int mldsa_verify_internal_no_self_test(
    const struct public_key<K> *pub,
    const uint8_t encoded_signature[signature_bytes<K>()], const uint8_t *msg,
    size_t msg_len, const uint8_t *context_prefix, size_t context_prefix_len,
    const uint8_t *context, size_t context_len) {

  struct values_st {
    struct signature<K, L> sign;
    matrix<K, L> a_ntt;
    vector<L> z_ntt;
    vector<K> az_ntt;
    vector<K> ct1_ntt;
  };
  std::unique_ptr<values_st, DeleterFree<values_st>> values(
      reinterpret_cast<struct values_st *>(OPENSSL_malloc(sizeof(values_st))));
  if (values == NULL) {
    return 0;
  }

  CBS cbs;
  CBS_init(&cbs, encoded_signature, signature_bytes<K>());
  if (!mldsa_parse_signature(&values->sign, &cbs)) {
    return 0;
  }

  matrix_expand(&values->a_ntt, pub->rho);

  uint8_t mu[kMuBytes];
  struct BORINGSSL_keccak_st keccak_ctx;
  BORINGSSL_keccak_init(&keccak_ctx, boringssl_shake256);
  BORINGSSL_keccak_absorb(&keccak_ctx, pub->public_key_hash,
                          sizeof(pub->public_key_hash));
  BORINGSSL_keccak_absorb(&keccak_ctx, context_prefix, context_prefix_len);
  BORINGSSL_keccak_absorb(&keccak_ctx, context, context_len);
  BORINGSSL_keccak_absorb(&keccak_ctx, msg, msg_len);
  BORINGSSL_keccak_squeeze(&keccak_ctx, mu, kMuBytes);

  scalar c_ntt;
  scalar_sample_in_ball_vartime(&c_ntt, values->sign.c_tilde,
                                sizeof(values->sign.c_tilde), tau<K>());
  scalar_ntt(&c_ntt);

  OPENSSL_memcpy(&values->z_ntt, &values->sign.z, sizeof(values->z_ntt));
  vector_ntt(&values->z_ntt);

  matrix_mult(&values->az_ntt, &values->a_ntt, &values->z_ntt);

  vector_scale_power2_round(&values->ct1_ntt, &pub->t1);
  vector_ntt(&values->ct1_ntt);

  vector_mult_scalar(&values->ct1_ntt, &values->ct1_ntt, &c_ntt);

  vector<K> *const w1 = &values->az_ntt;
  vector_sub(w1, &values->az_ntt, &values->ct1_ntt);
  vector_inverse_ntt(w1);

  vector_use_hint_vartime(w1, &values->sign.h, w1);
  uint8_t w1_encoded[128 * K];
  w1_encode(w1_encoded, w1);

  uint8_t c_tilde[2 * lambda_bytes<K>()];
  BORINGSSL_keccak_init(&keccak_ctx, boringssl_shake256);
  BORINGSSL_keccak_absorb(&keccak_ctx, mu, kMuBytes);
  BORINGSSL_keccak_absorb(&keccak_ctx, w1_encoded, 128 * K);
  BORINGSSL_keccak_squeeze(&keccak_ctx, c_tilde, 2 * lambda_bytes<K>());

  uint32_t z_max = vector_max(&values->sign.z);
  return z_max < static_cast<uint32_t>(gamma1<K>() - beta<K>()) &&
         OPENSSL_memcmp(c_tilde, values->sign.c_tilde, 2 * lambda_bytes<K>()) ==
             0;
}
template <int K, int L>
int mldsa_verify_internal(const struct public_key<K> *pub,
                          const uint8_t encoded_signature[signature_bytes<K>()],
                          const uint8_t *msg, size_t msg_len,
                          const uint8_t *context_prefix,
                          size_t context_prefix_len, const uint8_t *context,
                          size_t context_len) {
  fips::ensure_verify_self_test();
  return mldsa_verify_internal_no_self_test<K, L>(
      pub, encoded_signature, msg, msg_len, context_prefix, context_prefix_len,
      context, context_len);
}

struct private_key<6, 5> *private_key_from_external_65(
    const struct BCM_mldsa65_private_key *external) {
  static_assert(sizeof(struct BCM_mldsa65_private_key) ==
                    sizeof(struct private_key<6, 5>),
                "MLDSA65 private key size incorrect");
  static_assert(alignof(struct BCM_mldsa65_private_key) ==
                    alignof(struct private_key<6, 5>),
                "MLDSA65 private key align incorrect");
  return (struct private_key<6, 5> *)external;
}

struct public_key<6> *public_key_from_external_65(
    const struct BCM_mldsa65_public_key *external) {
  static_assert(
      sizeof(struct BCM_mldsa65_public_key) == sizeof(struct public_key<6>),
      "MLDSA65 public key size incorrect");
  static_assert(
      alignof(struct BCM_mldsa65_public_key) == alignof(struct public_key<6>),
      "MLDSA65 public key align incorrect");
  return (struct public_key<6> *)external;
}

struct private_key<8, 7> *private_key_from_external_87(
    const struct BCM_mldsa87_private_key *external) {
  static_assert(sizeof(struct BCM_mldsa87_private_key) ==
                    sizeof(struct private_key<8, 7>),
                "MLDSA87 private key size incorrect");
  static_assert(alignof(struct BCM_mldsa87_private_key) ==
                    alignof(struct private_key<8, 7>),
                "MLDSA87 private key align incorrect");
  return (struct private_key<8, 7> *)external;
}

struct public_key<8> *public_key_from_external_87(
    const struct BCM_mldsa87_public_key *external) {
  static_assert(
      sizeof(struct BCM_mldsa87_public_key) == sizeof(struct public_key<8>),
      "MLDSA87 public key size incorrect");
  static_assert(
      alignof(struct BCM_mldsa87_public_key) == alignof(struct public_key<8>),
      "MLDSA87 public key align incorrect");
  return (struct public_key<8> *)external;
}

namespace fips {

#include "fips_known_values.inc"

static int keygen_self_test() {
  private_key<6, 5> priv;
  uint8_t pub_bytes[BCM_MLDSA65_PUBLIC_KEY_BYTES];
  if (!mldsa_generate_key_external_entropy_no_self_test(pub_bytes, &priv,
                                                        kGenerateKeyEntropy)) {
    return 0;
  }

  uint8_t priv_bytes[BCM_MLDSA65_PRIVATE_KEY_BYTES];
  CBB cbb;
  CBB_init_fixed(&cbb, priv_bytes, sizeof(priv_bytes));
  if (!mldsa_marshal_private_key(&cbb, &priv)) {
    return 0;
  }

  static_assert(sizeof(pub_bytes) == sizeof(kExpectedPublicKey));
  static_assert(sizeof(priv_bytes) == sizeof(kExpectedPrivateKey));
  if (!BORINGSSL_check_test(kExpectedPublicKey, pub_bytes, sizeof(pub_bytes),
                            "ML-DSA keygen public key") ||
      !BORINGSSL_check_test(kExpectedPrivateKey, priv_bytes, sizeof(priv_bytes),
                            "ML-DSA keygen private key")) {
    return 0;
  }

  return 1;
}

static int sign_self_test() {
  private_key<6, 5> priv;
  uint8_t pub_bytes[BCM_MLDSA65_PUBLIC_KEY_BYTES];
  if (!mldsa_generate_key_external_entropy(pub_bytes, &priv, kSignEntropy)) {
    return 0;
  }

  const uint8_t randomizer[BCM_MLDSA_SIGNATURE_RANDOMIZER_BYTES] = {};
  uint8_t sig[BCM_MLDSA65_SIGNATURE_BYTES];

  uint8_t message[4] = {0};
  if (!mldsa_sign_internal_no_self_test(sig, &priv, message, sizeof(message),
                                        nullptr, 0, nullptr, 0, randomizer)) {
    return 0;
  }
  static_assert(sizeof(kExpectedCase1Signature) == sizeof(sig));
  if (!BORINGSSL_check_test(kExpectedCase1Signature, sig, sizeof(sig),
                            "ML-DSA sign case 1")) {
    return 0;
  }

  message[0] = 123;
  if (!mldsa_sign_internal_no_self_test(sig, &priv, message, sizeof(message),
                                        nullptr, 0, nullptr, 0, randomizer)) {
    return 0;
  }
  static_assert(sizeof(kExpectedCase2Signature) == sizeof(sig));
  if (!BORINGSSL_check_test(kExpectedCase2Signature, sig, sizeof(sig),
                            "ML-DSA sign case 2")) {
    return 0;
  }

  return 1;
}

static int verify_self_test() {
  struct values_st {
    private_key<6, 5> priv;
    public_key<6> pub;
    uint8_t pub_bytes[BCM_MLDSA65_PUBLIC_KEY_BYTES];
  };
  std::unique_ptr<values_st, DeleterFree<values_st>> values(
      reinterpret_cast<struct values_st *>(OPENSSL_malloc(sizeof(values_st))));
  if (!values) {
    return 0;
  }

  if (!mldsa_generate_key_external_entropy(values->pub_bytes, &values->priv,
                                           kSignEntropy)) {
    return 0;
  }

  const uint8_t message[4] = {1, 0};
  if (!mldsa_public_from_private(&values->pub, &values->priv) ||
      !mldsa_verify_internal_no_self_test<6, 5>(
          &values->pub, kExpectedVerifySignature, message, sizeof(message),
          nullptr, 0, nullptr, 0)) {
    return 0;
  }

  return 1;
}

template <int K, int L>
int check_key(private_key<K, L> *priv) {
  uint8_t sig[signature_bytes<K>()];
  uint8_t randomizer[BCM_MLDSA_SIGNATURE_RANDOMIZER_BYTES] = {};
  mldsa::public_key<K> pub;
  if (!mldsa_public_from_private(&pub, priv) ||
      !mldsa_sign_internal_no_self_test(sig, priv, nullptr, 0, nullptr, 0,
                                        nullptr, 0, randomizer)) {
    return 0;
  }

  if (boringssl_fips_break_test("MLDSA_PWCT")) {
    sig[0] ^= 1;
  }

  if (!mldsa_verify_internal_no_self_test<K, L>(&pub, sig, nullptr, 0, nullptr,
                                                0, nullptr, 0)) {
    return 0;
  }
  return 1;
}

#if defined(BORINGSSL_FIPS)

DEFINE_STATIC_ONCE(g_mldsa_keygen_self_test_once)

void ensure_keygen_self_test(void) {
  CRYPTO_once(g_mldsa_keygen_self_test_once_bss_get(), []() {
    if (!keygen_self_test()) {
      BORINGSSL_FIPS_abort();
    }
  });
}

DEFINE_STATIC_ONCE(g_mldsa_sign_self_test_once)

void ensure_sign_self_test(void) {
  CRYPTO_once(g_mldsa_sign_self_test_once_bss_get(), []() {
    if (!sign_self_test()) {
      BORINGSSL_FIPS_abort();
    }
  });
}

DEFINE_STATIC_ONCE(g_mldsa_verify_self_test_once)

void ensure_verify_self_test(void) {
  CRYPTO_once(g_mldsa_verify_self_test_once_bss_get(), []() {
    if (!verify_self_test()) {
      BORINGSSL_FIPS_abort();
    }
  });
}

#else

void ensure_keygen_self_test(void) {}
void ensure_sign_self_test(void) {}
void ensure_verify_self_test(void) {}

#endif

}

}
}

bcm_status BCM_mldsa65_parse_public_key(
    struct BCM_mldsa65_public_key *public_key, CBS *in) {
  return bcm_as_approved_status(mldsa_parse_public_key(
      mldsa::public_key_from_external_65(public_key), in));
}

bcm_status BCM_mldsa65_marshal_private_key(
    CBB *out, const struct BCM_mldsa65_private_key *private_key) {
  return bcm_as_approved_status(mldsa_marshal_private_key(
      out, mldsa::private_key_from_external_65(private_key)));
}

bcm_status BCM_mldsa65_parse_private_key(
    struct BCM_mldsa65_private_key *private_key, CBS *in) {
  return bcm_as_approved_status(
      mldsa_parse_private_key(mldsa::private_key_from_external_65(private_key),
                              in) &&
      CBS_len(in) == 0);
}

bcm_status BCM_mldsa65_check_key_fips(
    struct BCM_mldsa65_private_key *private_key) {
  return bcm_as_approved_status(
      mldsa::fips::check_key(mldsa::private_key_from_external_65(private_key)));
}

bcm_status BCM_mldsa65_generate_key(
    uint8_t out_encoded_public_key[BCM_MLDSA65_PUBLIC_KEY_BYTES],
    uint8_t out_seed[BCM_MLDSA_SEED_BYTES],
    struct BCM_mldsa65_private_key *out_private_key) {
  BCM_rand_bytes(out_seed, BCM_MLDSA_SEED_BYTES);
  CONSTTIME_SECRET(out_seed, BCM_MLDSA_SEED_BYTES);
  return BCM_mldsa65_generate_key_external_entropy(out_encoded_public_key,
                                                   out_private_key, out_seed);
}

bcm_status BCM_mldsa65_private_key_from_seed(
    struct BCM_mldsa65_private_key *out_private_key,
    const uint8_t seed[BCM_MLDSA_SEED_BYTES]) {
  uint8_t public_key[BCM_MLDSA65_PUBLIC_KEY_BYTES];
  return BCM_mldsa65_generate_key_external_entropy(public_key, out_private_key,
                                                   seed);
}

bcm_status BCM_mldsa65_generate_key_external_entropy(
    uint8_t out_encoded_public_key[BCM_MLDSA65_PUBLIC_KEY_BYTES],
    struct BCM_mldsa65_private_key *out_private_key,
    const uint8_t entropy[BCM_MLDSA_SEED_BYTES]) {
  return bcm_as_not_approved_status(mldsa_generate_key_external_entropy(
      out_encoded_public_key,
      mldsa::private_key_from_external_65(out_private_key), entropy));
}

bcm_status BCM_mldsa65_generate_key_fips(
    uint8_t out_encoded_public_key[BCM_MLDSA65_PUBLIC_KEY_BYTES],
    uint8_t out_seed[BCM_MLDSA_SEED_BYTES],
    struct BCM_mldsa65_private_key *out_private_key) {
  if (out_encoded_public_key == nullptr || out_private_key == nullptr) {
    return bcm_status::failure;
  }
  if (BCM_mldsa65_generate_key(out_encoded_public_key, out_seed,
                               out_private_key) == bcm_status::failure) {
    return bcm_status::failure;
  }
  return BCM_mldsa65_check_key_fips(out_private_key);
}

bcm_status BCM_mldsa65_generate_key_external_entropy_fips(
    uint8_t out_encoded_public_key[BCM_MLDSA65_PUBLIC_KEY_BYTES],
    struct BCM_mldsa65_private_key *out_private_key,
    const uint8_t entropy[BCM_MLDSA_SEED_BYTES]) {
  if (out_encoded_public_key == nullptr || out_private_key == nullptr) {
    return bcm_status::failure;
  }
  if (BCM_mldsa65_generate_key_external_entropy(out_encoded_public_key,
                                                out_private_key, entropy) ==
      bcm_status::failure) {
    return bcm_status::failure;
  }
  return BCM_mldsa65_check_key_fips(out_private_key);
}

bcm_status BCM_mldsa65_private_key_from_seed_fips(
    struct BCM_mldsa65_private_key *out_private_key,
    const uint8_t seed[BCM_MLDSA_SEED_BYTES]) {
  uint8_t public_key[BCM_MLDSA65_PUBLIC_KEY_BYTES];
  if (BCM_mldsa65_generate_key_external_entropy(public_key, out_private_key,
                                                seed) == bcm_status::failure) {
    return bcm_status::failure;
  }
  return BCM_mldsa65_check_key_fips(out_private_key);
}

bcm_status BCM_mldsa65_public_from_private(
    struct BCM_mldsa65_public_key *out_public_key,
    const struct BCM_mldsa65_private_key *private_key) {
  return bcm_as_approved_status(mldsa_public_from_private(
      mldsa::public_key_from_external_65(out_public_key),
      mldsa::private_key_from_external_65(private_key)));
}

bcm_status BCM_mldsa65_sign_internal(
    uint8_t out_encoded_signature[BCM_MLDSA65_SIGNATURE_BYTES],
    const struct BCM_mldsa65_private_key *private_key, const uint8_t *msg,
    size_t msg_len, const uint8_t *context_prefix, size_t context_prefix_len,
    const uint8_t *context, size_t context_len,
    const uint8_t randomizer[BCM_MLDSA_SIGNATURE_RANDOMIZER_BYTES]) {
  return bcm_as_approved_status(mldsa_sign_internal(
      out_encoded_signature, mldsa::private_key_from_external_65(private_key),
      msg, msg_len, context_prefix, context_prefix_len, context, context_len,
      randomizer));
}

bcm_status BCM_mldsa65_sign(
    uint8_t out_encoded_signature[BCM_MLDSA65_SIGNATURE_BYTES],
    const struct BCM_mldsa65_private_key *private_key, const uint8_t *msg,
    size_t msg_len, const uint8_t *context, size_t context_len) {
  BSSL_CHECK(context_len <= 255);
  uint8_t randomizer[BCM_MLDSA_SIGNATURE_RANDOMIZER_BYTES];
  BCM_rand_bytes(randomizer, sizeof(randomizer));
  CONSTTIME_SECRET(randomizer, sizeof(randomizer));

  const uint8_t context_prefix[2] = {0, static_cast<uint8_t>(context_len)};
  return BCM_mldsa65_sign_internal(
      out_encoded_signature, private_key, msg, msg_len, context_prefix,
      sizeof(context_prefix), context, context_len, randomizer);
}

bcm_status BCM_mldsa65_verify(
    const struct BCM_mldsa65_public_key *public_key,
    const uint8_t signature[BCM_MLDSA65_SIGNATURE_BYTES], const uint8_t *msg,
    size_t msg_len, const uint8_t *context, size_t context_len) {
  BSSL_CHECK(context_len <= 255);
  const uint8_t context_prefix[2] = {0, static_cast<uint8_t>(context_len)};
  return BCM_mldsa65_verify_internal(public_key, signature, msg, msg_len,
                                     context_prefix, sizeof(context_prefix),
                                     context, context_len);
}

bcm_status BCM_mldsa65_verify_internal(
    const struct BCM_mldsa65_public_key *public_key,
    const uint8_t encoded_signature[BCM_MLDSA65_SIGNATURE_BYTES],
    const uint8_t *msg, size_t msg_len, const uint8_t *context_prefix,
    size_t context_prefix_len, const uint8_t *context, size_t context_len) {
  return bcm_as_approved_status(mldsa::mldsa_verify_internal<6, 5>(
      mldsa::public_key_from_external_65(public_key), encoded_signature, msg,
      msg_len, context_prefix, context_prefix_len, context, context_len));
}

bcm_status BCM_mldsa65_marshal_public_key(
    CBB *out, const struct BCM_mldsa65_public_key *public_key) {
  return bcm_as_approved_status(mldsa_marshal_public_key(
      out, mldsa::public_key_from_external_65(public_key)));
}

bcm_status BCM_mldsa87_parse_public_key(
    struct BCM_mldsa87_public_key *public_key, CBS *in) {
  return bcm_as_approved_status(mldsa_parse_public_key(
      mldsa::public_key_from_external_87(public_key), in));
}

bcm_status BCM_mldsa87_marshal_private_key(
    CBB *out, const struct BCM_mldsa87_private_key *private_key) {
  return bcm_as_approved_status(mldsa_marshal_private_key(
      out, mldsa::private_key_from_external_87(private_key)));
}

bcm_status BCM_mldsa87_parse_private_key(
    struct BCM_mldsa87_private_key *private_key, CBS *in) {
  return bcm_as_approved_status(
      mldsa_parse_private_key(mldsa::private_key_from_external_87(private_key),
                              in) &&
      CBS_len(in) == 0);
}

bcm_status BCM_mldsa87_check_key_fips(
    struct BCM_mldsa87_private_key *private_key) {
  return bcm_as_approved_status(
      mldsa::fips::check_key(mldsa::private_key_from_external_87(private_key)));
}

bcm_status BCM_mldsa87_generate_key(
    uint8_t out_encoded_public_key[BCM_MLDSA87_PUBLIC_KEY_BYTES],
    uint8_t out_seed[BCM_MLDSA_SEED_BYTES],
    struct BCM_mldsa87_private_key *out_private_key) {
  BCM_rand_bytes(out_seed, BCM_MLDSA_SEED_BYTES);
  return BCM_mldsa87_generate_key_external_entropy(out_encoded_public_key,
                                                   out_private_key, out_seed);
}

bcm_status BCM_mldsa87_private_key_from_seed(
    struct BCM_mldsa87_private_key *out_private_key,
    const uint8_t seed[BCM_MLDSA_SEED_BYTES]) {
  uint8_t public_key[BCM_MLDSA87_PUBLIC_KEY_BYTES];
  return BCM_mldsa87_generate_key_external_entropy(public_key, out_private_key,
                                                   seed);
}

bcm_status BCM_mldsa87_generate_key_external_entropy(
    uint8_t out_encoded_public_key[BCM_MLDSA87_PUBLIC_KEY_BYTES],
    struct BCM_mldsa87_private_key *out_private_key,
    const uint8_t entropy[BCM_MLDSA_SEED_BYTES]) {
  return bcm_as_not_approved_status(mldsa_generate_key_external_entropy(
      out_encoded_public_key,
      mldsa::private_key_from_external_87(out_private_key), entropy));
}

bcm_status BCM_mldsa87_generate_key_fips(
    uint8_t out_encoded_public_key[BCM_MLDSA87_PUBLIC_KEY_BYTES],
    uint8_t out_seed[BCM_MLDSA_SEED_BYTES],
    struct BCM_mldsa87_private_key *out_private_key) {
  if (out_encoded_public_key == nullptr || out_private_key == nullptr) {
    return bcm_status::failure;
  }
  if (BCM_mldsa87_generate_key(out_encoded_public_key, out_seed,
                               out_private_key) == bcm_status::failure) {
    return bcm_status::failure;
  }
  return BCM_mldsa87_check_key_fips(out_private_key);
}

bcm_status BCM_mldsa87_generate_key_external_entropy_fips(
    uint8_t out_encoded_public_key[BCM_MLDSA87_PUBLIC_KEY_BYTES],
    struct BCM_mldsa87_private_key *out_private_key,
    const uint8_t entropy[BCM_MLDSA_SEED_BYTES]) {
  if (out_encoded_public_key == nullptr || out_private_key == nullptr) {
    return bcm_status::failure;
  }
  if (BCM_mldsa87_generate_key_external_entropy(out_encoded_public_key,
                                                out_private_key, entropy) ==
      bcm_status::failure) {
    return bcm_status::failure;
  }
  return BCM_mldsa87_check_key_fips(out_private_key);
}

bcm_status BCM_mldsa87_private_key_from_seed_fips(
    struct BCM_mldsa87_private_key *out_private_key,
    const uint8_t seed[BCM_MLDSA_SEED_BYTES]) {
  uint8_t public_key[BCM_MLDSA87_PUBLIC_KEY_BYTES];
  if (BCM_mldsa87_generate_key_external_entropy(public_key, out_private_key,
                                                seed) == bcm_status::failure) {
    return bcm_status::failure;
  }
  return BCM_mldsa87_check_key_fips(out_private_key);
}

bcm_status BCM_mldsa87_public_from_private(
    struct BCM_mldsa87_public_key *out_public_key,
    const struct BCM_mldsa87_private_key *private_key) {
  return bcm_as_approved_status(mldsa_public_from_private(
      mldsa::public_key_from_external_87(out_public_key),
      mldsa::private_key_from_external_87(private_key)));
}

bcm_status BCM_mldsa87_sign_internal(
    uint8_t out_encoded_signature[BCM_MLDSA87_SIGNATURE_BYTES],
    const struct BCM_mldsa87_private_key *private_key, const uint8_t *msg,
    size_t msg_len, const uint8_t *context_prefix, size_t context_prefix_len,
    const uint8_t *context, size_t context_len,
    const uint8_t randomizer[BCM_MLDSA_SIGNATURE_RANDOMIZER_BYTES]) {
  return bcm_as_approved_status(mldsa_sign_internal(
      out_encoded_signature, mldsa::private_key_from_external_87(private_key),
      msg, msg_len, context_prefix, context_prefix_len, context, context_len,
      randomizer));
}

bcm_status BCM_mldsa87_sign(
    uint8_t out_encoded_signature[BCM_MLDSA87_SIGNATURE_BYTES],
    const struct BCM_mldsa87_private_key *private_key, const uint8_t *msg,
    size_t msg_len, const uint8_t *context, size_t context_len) {
  BSSL_CHECK(context_len <= 255);
  uint8_t randomizer[BCM_MLDSA_SIGNATURE_RANDOMIZER_BYTES];
  BCM_rand_bytes(randomizer, sizeof(randomizer));

  const uint8_t context_prefix[2] = {0, static_cast<uint8_t>(context_len)};
  return BCM_mldsa87_sign_internal(
      out_encoded_signature, private_key, msg, msg_len, context_prefix,
      sizeof(context_prefix), context, context_len, randomizer);
}

bcm_status BCM_mldsa87_verify(const struct BCM_mldsa87_public_key *public_key,
                              const uint8_t *signature, const uint8_t *msg,
                              size_t msg_len, const uint8_t *context,
                              size_t context_len) {
  BSSL_CHECK(context_len <= 255);
  const uint8_t context_prefix[2] = {0, static_cast<uint8_t>(context_len)};
  return BCM_mldsa87_verify_internal(public_key, signature, msg, msg_len,
                                     context_prefix, sizeof(context_prefix),
                                     context, context_len);
}

bcm_status BCM_mldsa87_verify_internal(
    const struct BCM_mldsa87_public_key *public_key,
    const uint8_t encoded_signature[BCM_MLDSA87_SIGNATURE_BYTES],
    const uint8_t *msg, size_t msg_len, const uint8_t *context_prefix,
    size_t context_prefix_len, const uint8_t *context, size_t context_len) {
  return bcm_as_approved_status(mldsa::mldsa_verify_internal<8, 7>(
      mldsa::public_key_from_external_87(public_key), encoded_signature, msg,
      msg_len, context_prefix, context_prefix_len, context, context_len));
}

bcm_status BCM_mldsa87_marshal_public_key(
    CBB *out, const struct BCM_mldsa87_public_key *public_key) {
  return bcm_as_approved_status(mldsa_marshal_public_key(
      out, mldsa::public_key_from_external_87(public_key)));
}

int boringssl_self_test_mldsa() {
  return mldsa::fips::keygen_self_test() && mldsa::fips::sign_self_test() &&
         mldsa::fips::verify_self_test();
}
