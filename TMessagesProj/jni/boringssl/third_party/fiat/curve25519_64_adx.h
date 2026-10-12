#include <openssl/base.h>
#include "../../crypto/internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <immintrin.h>

typedef uint64_t fe4[4];
typedef uint8_t fiat_uint1;
typedef int8_t fiat_int1;

static __inline__ uint64_t fiat_value_barrier_u64(uint64_t a) {
  __asm__("" : "+r"(a) :                );
  return a;
}

__attribute__((target("adx,bmi2")))
static inline void fe4_mul(fe4 out, const fe4 x, const fe4 y) { fiat_curve25519_adx_mul(out, x, y); }

__attribute__((target("adx,bmi2")))
static inline void fe4_sq(fe4 out, const fe4 x) { fiat_curve25519_adx_square(out, x); }

__attribute__((target("adx,bmi2")))
static inline void fiat_mulx_u64(uint64_t* out1, uint64_t* out2, uint64_t arg1, uint64_t arg2) {

#if defined(_M_X64)
  unsigned long long t;
  *out1 = _umul128(arg1, arg2, &t);
  *out2 = t;
#elif defined(_M_ARM64)
  *out1 = arg1 * arg2;
  *out2 = __umulh(arg1, arg2);
#else
  unsigned __int128 t = (unsigned __int128)arg1 * arg2;
  *out1 = t;
  *out2 = (t >> 64);
#endif
}

__attribute__((target("adx,bmi2")))
static inline void fiat_addcarryx_u64(uint64_t* out1, fiat_uint1* out2, fiat_uint1 arg1, uint64_t arg2, uint64_t arg3) {

#if defined(__has_builtin)
#  if __has_builtin(__builtin_ia32_addcarryx_u64)
#    define addcarry64 __builtin_ia32_addcarryx_u64
#  endif
#endif
#if defined(addcarry64)
  long long unsigned int t;
  *out2 = addcarry64(arg1, arg2, arg3, &t);
  *out1 = t;
#elif defined(_M_X64)
  long long unsigned int t;
  *out2 = _addcarry_u64(arg1, arg2, arg3, out1);
  *out1 = t;
#else
  arg2 += arg1;
  arg1 = arg2 < arg1;
  uint64_t ret = arg2 + arg3;
  arg1 += ret < arg2;
  *out1 = ret;
  *out2 = arg1;
#endif
#undef addcarry64
}

__attribute__((target("adx,bmi2")))
static inline void fiat_subborrowx_u64(uint64_t* out1, fiat_uint1* out2, fiat_uint1 arg1, uint64_t arg2, uint64_t arg3) {
#if defined(__has_builtin)
#  if __has_builtin(__builtin_ia32_subborrow_u64)
#    define subborrow64 __builtin_ia32_subborrow_u64
#  endif
#endif
#if defined(subborrow64)
  long long unsigned int t;
  *out2 = subborrow64(arg1, arg2, arg3, &t);
  *out1 = t;
#elif defined(_M_X64)
  long long unsigned int t;
  *out2 = _subborrow_u64(arg1, arg2, arg3, &t);
  *out1 = t;
#else
  *out1 = arg2 - arg3 - arg1;
  *out2 = (arg2 < arg3) | ((arg2 == arg3) & arg1);
#endif
#undef subborrow64
}

__attribute__((target("adx,bmi2")))
static inline void fiat_cmovznz_u64(uint64_t* out1, fiat_uint1 arg1, uint64_t arg2, uint64_t arg3) {
  fiat_uint1 x1;
  uint64_t x2;
  uint64_t x3;
  x1 = (!(!arg1));
  x2 = ((fiat_int1)(0x0 - x1) & UINT64_C(0xffffffffffffffff));
  x3 = ((fiat_value_barrier_u64(x2) & arg3) | (fiat_value_barrier_u64((~x2)) & arg2));
  *out1 = x3;
}

__attribute__((target("adx,bmi2")))
static void fe4_add(uint64_t out1[4], const uint64_t arg1[4], const uint64_t arg2[4]) {
  uint64_t x1;
  fiat_uint1 x2;
  uint64_t x3;
  fiat_uint1 x4;
  uint64_t x5;
  fiat_uint1 x6;
  uint64_t x7;
  fiat_uint1 x8;
  uint64_t x9;
  uint64_t x10;
  fiat_uint1 x11;
  uint64_t x12;
  fiat_uint1 x13;
  uint64_t x14;
  fiat_uint1 x15;
  uint64_t x16;
  fiat_uint1 x17;
  uint64_t x18;
  uint64_t x19;
  fiat_uint1 x20;
  fiat_addcarryx_u64(&x1, &x2, 0x0, (arg1[0]), (arg2[0]));
  fiat_addcarryx_u64(&x3, &x4, x2, (arg1[1]), (arg2[1]));
  fiat_addcarryx_u64(&x5, &x6, x4, (arg1[2]), (arg2[2]));
  fiat_addcarryx_u64(&x7, &x8, x6, (arg1[3]), (arg2[3]));
  fiat_cmovznz_u64(&x9, x8, 0x0, UINT8_C(0x26));
  fiat_addcarryx_u64(&x10, &x11, 0x0, x1, x9);
  fiat_addcarryx_u64(&x12, &x13, x11, x3, 0x0);
  fiat_addcarryx_u64(&x14, &x15, x13, x5, 0x0);
  fiat_addcarryx_u64(&x16, &x17, x15, x7, 0x0);
  fiat_cmovznz_u64(&x18, x17, 0x0, UINT8_C(0x26));
  fiat_addcarryx_u64(&x19, &x20, 0x0, x10, x18);
  out1[0] = x19;
  out1[1] = x12;
  out1[2] = x14;
  out1[3] = x16;
}

__attribute__((target("adx,bmi2")))
static void fe4_sub(uint64_t out1[4], const uint64_t arg1[4], const uint64_t arg2[4]) {
  uint64_t x1;
  uint64_t x2;
  fiat_uint1 x3;
  uint64_t x4;
  uint64_t x5;
  fiat_uint1 x6;
  uint64_t x7;
  uint64_t x8;
  fiat_uint1 x9;
  uint64_t x10;
  uint64_t x11;
  fiat_uint1 x12;
  uint64_t x13;
  uint64_t x14;
  fiat_uint1 x15;
  uint64_t x16;
  fiat_uint1 x17;
  uint64_t x18;
  fiat_uint1 x19;
  uint64_t x20;
  fiat_uint1 x21;
  uint64_t x22;
  uint64_t x23;
  fiat_uint1 x24;
  x1 = (arg2[0]);
  fiat_subborrowx_u64(&x2, &x3, 0x0, (arg1[0]), x1);
  x4 = (arg2[1]);
  fiat_subborrowx_u64(&x5, &x6, x3, (arg1[1]), x4);
  x7 = (arg2[2]);
  fiat_subborrowx_u64(&x8, &x9, x6, (arg1[2]), x7);
  x10 = (arg2[3]);
  fiat_subborrowx_u64(&x11, &x12, x9, (arg1[3]), x10);
  fiat_cmovznz_u64(&x13, x12, 0x0, UINT8_C(0x26));
  fiat_subborrowx_u64(&x14, &x15, 0x0, x2, x13);
  fiat_subborrowx_u64(&x16, &x17, x15, x5, 0x0);
  fiat_subborrowx_u64(&x18, &x19, x17, x8, 0x0);
  fiat_subborrowx_u64(&x20, &x21, x19, x11, 0x0);
  fiat_cmovznz_u64(&x22, x21, 0x0, UINT8_C(0x26));
  fiat_subborrowx_u64(&x23, &x24, 0x0, x14, x22);
  out1[0] = x23;
  out1[1] = x16;
  out1[2] = x18;
  out1[3] = x20;
}

__attribute__((target("adx,bmi2")))
static void fe4_scmul(uint64_t out1[4], const uint64_t arg1[4], uint64_t arg2) {
  uint64_t x1;
  uint64_t x2;
  uint64_t x3;
  uint64_t x4;
  uint64_t x5;
  fiat_uint1 x6;
  uint64_t x7;
  uint64_t x8;
  uint64_t x9;
  fiat_uint1 x10;
  uint64_t x11;
  uint64_t x12;
  uint64_t x13;
  fiat_uint1 x14;
  uint64_t x15;
  uint64_t x16;
  uint64_t x17;
  fiat_uint1 x18;
  uint64_t x19;
  fiat_uint1 x20;
  uint64_t x21;
  fiat_uint1 x22;
  uint64_t x23;
  fiat_uint1 x24;
  uint64_t x25;
  uint64_t x26;
  fiat_uint1 x27;
  fiat_mulx_u64(&x1, &x2, (arg1[0]), arg2);
  fiat_mulx_u64(&x3, &x4, (arg1[1]), arg2);
  fiat_addcarryx_u64(&x5, &x6, 0x0, x2, x3);
  fiat_mulx_u64(&x7, &x8, (arg1[2]), arg2);
  fiat_addcarryx_u64(&x9, &x10, x6, x4, x7);
  fiat_mulx_u64(&x11, &x12, (arg1[3]), arg2);
  fiat_addcarryx_u64(&x13, &x14, x10, x8, x11);
  fiat_mulx_u64(&x15, &x16, (x12 + (uint64_t)x14), UINT8_C(0x26));
  fiat_addcarryx_u64(&x17, &x18, 0x0, x1, x15);
  fiat_addcarryx_u64(&x19, &x20, x18, x5, 0x0);
  fiat_addcarryx_u64(&x21, &x22, x20, x9, 0x0);
  fiat_addcarryx_u64(&x23, &x24, x22, x13, 0x0);
  fiat_cmovznz_u64(&x25, x24, 0x0, UINT8_C(0x26));
  fiat_addcarryx_u64(&x26, &x27, 0x0, x17, x25);
  out1[0] = x26;
  out1[1] = x19;
  out1[2] = x21;
  out1[3] = x23;
}

__attribute__((target("adx,bmi2")))
static void fe4_canon(uint64_t out1[4], const uint64_t arg1[4]) {
  uint64_t x1;
  fiat_uint1 x2;
  uint64_t x3;
  fiat_uint1 x4;
  uint64_t x5;
  fiat_uint1 x6;
  uint64_t x7;
  fiat_uint1 x8;
  uint64_t x9;
  uint64_t x10;
  uint64_t x11;
  uint64_t x12;
  uint64_t x13;
  fiat_uint1 x14;
  uint64_t x15;
  fiat_uint1 x16;
  uint64_t x17;
  fiat_uint1 x18;
  uint64_t x19;
  fiat_uint1 x20;
  uint64_t x21;
  uint64_t x22;
  uint64_t x23;
  uint64_t x24;
  fiat_subborrowx_u64(&x1, &x2, 0x0, (arg1[0]), UINT64_C(0xffffffffffffffed));
  fiat_subborrowx_u64(&x3, &x4, x2, (arg1[1]), UINT64_C(0xffffffffffffffff));
  fiat_subborrowx_u64(&x5, &x6, x4, (arg1[2]), UINT64_C(0xffffffffffffffff));
  fiat_subborrowx_u64(&x7, &x8, x6, (arg1[3]), UINT64_C(0x7fffffffffffffff));
  fiat_cmovznz_u64(&x9, x8, x1, (arg1[0]));
  fiat_cmovznz_u64(&x10, x8, x3, (arg1[1]));
  fiat_cmovznz_u64(&x11, x8, x5, (arg1[2]));
  fiat_cmovznz_u64(&x12, x8, x7, (arg1[3]));
  fiat_subborrowx_u64(&x13, &x14, 0x0, x9, UINT64_C(0xffffffffffffffed));
  fiat_subborrowx_u64(&x15, &x16, x14, x10, UINT64_C(0xffffffffffffffff));
  fiat_subborrowx_u64(&x17, &x18, x16, x11, UINT64_C(0xffffffffffffffff));
  fiat_subborrowx_u64(&x19, &x20, x18, x12, UINT64_C(0x7fffffffffffffff));
  fiat_cmovznz_u64(&x21, x20, x13, x9);
  fiat_cmovznz_u64(&x22, x20, x15, x10);
  fiat_cmovznz_u64(&x23, x20, x17, x11);
  fiat_cmovznz_u64(&x24, x20, x19, x12);
  out1[0] = x21;
  out1[1] = x22;
  out1[2] = x23;
  out1[3] = x24;
}

__attribute__((target("adx,bmi2")))
static void fe4_cswap(uint64_t out1[4], uint64_t out2[4], fiat_uint1 arg1, const uint64_t arg2[4], const uint64_t arg3[4]) {
  uint64_t x1;
  uint64_t x2;
  uint64_t x3;
  uint64_t x4;
  uint64_t x5;
  uint64_t x6;
  uint64_t x7;
  uint64_t x8;

  fiat_cmovznz_u64(&x1, arg1, (arg2[0]), (arg3[0]));
  fiat_cmovznz_u64(&x2, arg1, (arg2[1]), (arg3[1]));
  fiat_cmovznz_u64(&x3, arg1, (arg2[2]), (arg3[2]));
  fiat_cmovznz_u64(&x4, arg1, (arg2[3]), (arg3[3]));
  fiat_cmovznz_u64(&x5, arg1, (arg3[0]), (arg2[0]));
  fiat_cmovznz_u64(&x6, arg1, (arg3[1]), (arg2[1]));
  fiat_cmovznz_u64(&x7, arg1, (arg3[2]), (arg2[2]));
  fiat_cmovznz_u64(&x8, arg1, (arg3[3]), (arg2[3]));
  out1[0] = x1;
  out1[1] = x2;
  out1[2] = x3;
  out1[3] = x4;
  out2[0] = x5;
  out2[1] = x6;
  out2[2] = x7;
  out2[3] = x8;
}

__attribute__((target("adx,bmi2")))
static void fe4_invert(fe4 out, const fe4 z) {
  fe4 t0;
  fe4 t1;
  fe4 t2;
  fe4 t3;
  int i;

  fe4_sq(t0, z);
  fe4_sq(t1, t0);
  for (i = 1; i < 2; ++i) {
    fe4_sq(t1, t1);
  }
  fe4_mul(t1, z, t1);
  fe4_mul(t0, t0, t1);
  fe4_sq(t2, t0);
  fe4_mul(t1, t1, t2);
  fe4_sq(t2, t1);
  for (i = 1; i < 5; ++i) {
    fe4_sq(t2, t2);
  }
  fe4_mul(t1, t2, t1);
  fe4_sq(t2, t1);
  for (i = 1; i < 10; ++i) {
    fe4_sq(t2, t2);
  }
  fe4_mul(t2, t2, t1);
  fe4_sq(t3, t2);
  for (i = 1; i < 20; ++i) {
    fe4_sq(t3, t3);
  }
  fe4_mul(t2, t3, t2);
  fe4_sq(t2, t2);
  for (i = 1; i < 10; ++i) {
    fe4_sq(t2, t2);
  }
  fe4_mul(t1, t2, t1);
  fe4_sq(t2, t1);
  for (i = 1; i < 50; ++i) {
    fe4_sq(t2, t2);
  }
  fe4_mul(t2, t2, t1);
  fe4_sq(t3, t2);
  for (i = 1; i < 100; ++i) {
    fe4_sq(t3, t3);
  }
  fe4_mul(t2, t3, t2);
  fe4_sq(t2, t2);
  for (i = 1; i < 50; ++i) {
    fe4_sq(t2, t2);
  }
  fe4_mul(t1, t2, t1);
  fe4_sq(t1, t1);
  for (i = 1; i < 5; ++i) {
    fe4_sq(t1, t1);
  }
  fe4_mul(out, t1, t0);
}

__attribute__((target("adx,bmi2")))
void x25519_scalar_mult_adx(uint8_t out[32], const uint8_t scalar[32],
                            const uint8_t point[32]) {
  uint8_t e[32];
  OPENSSL_memcpy(e, scalar, 32);
  e[0] &= 248;
  e[31] &= 127;
  e[31] |= 64;

  fe4 x1, x2 = {1}, z2 = {0}, x3, z3 = {1}, tmp0, tmp1;
  OPENSSL_memcpy(x1, point, sizeof(fe4));
  x1[3] &= (uint64_t)(-1)>>1;
  OPENSSL_memcpy(x3, x1, sizeof(fe4));

  unsigned swap = 0;
  int pos;
  for (pos = 254; pos >= 0; --pos) {

    unsigned b = 1 & (e[pos / 8] >> (pos & 7));
    swap ^= b;
    fe4_cswap(x2, x3, swap, x2, x3);
    fe4_cswap(z2, z3, swap, z2, z3);
    swap = b;

    fe4_sub(tmp0, x3, z3);
    fe4_sub(tmp1, x2, z2);
    fe4_add(x2, x2, z2);
    fe4_add(z2, x3, z3);
    fe4_mul(z3, tmp0, x2);
    fe4_mul(z2, z2, tmp1);
    fe4_sq(tmp0, tmp1);
    fe4_sq(tmp1, x2);
    fe4_add(x3, z3, z2);
    fe4_sub(z2, z3, z2);
    fe4_mul(x2, tmp1, tmp0);
    fe4_sub(tmp1, tmp1, tmp0);
    fe4_sq(z2, z2);
    fe4_scmul(z3, tmp1, 121666);
    fe4_sq(x3, x3);
    fe4_add(tmp0, tmp0, z3);
    fe4_mul(z3, x1, z2);
    fe4_mul(z2, tmp1, tmp0);
  }

  fe4_cswap(x2, x3, swap, x2, x3);
  fe4_cswap(z2, z3, swap, z2, z3);

  fe4_invert(z2, z2);
  fe4_mul(x2, x2, z2);
  fe4_canon(x2, x2);
  OPENSSL_memcpy(out, x2, sizeof(fe4));
}

typedef struct {
  fe4 X;
  fe4 Y;
  fe4 Z;
  fe4 T;
} ge_p3_4;

typedef struct {
  fe4 yplusx;
  fe4 yminusx;
  fe4 xy2d;
} ge_precomp_4;

__attribute__((target("adx,bmi2")))
static void inline_x25519_ge_dbl_4(ge_p3_4 *r, const ge_p3_4 *p, bool skip_t) {

  fe4 trX, trZ, trT, t0, cX, cY, cZ, cT;
  fe4_sq(trX, p->X);
  fe4_sq(trZ, p->Y);
  fe4_sq(trT, p->Z);
  fe4_add(trT, trT, trT);
  fe4_add(cY, p->X, p->Y);
  fe4_sq(t0, cY);
  fe4_add(cY, trZ, trX);
  fe4_sub(cZ, trZ, trX);
  fe4_sub(cX, t0, cY);
  fe4_sub(cT, trT, cZ);
  fe4_mul(r->X, cX, cT);
  fe4_mul(r->Y, cY, cZ);
  fe4_mul(r->Z, cZ, cT);
  if (!skip_t) {
    fe4_mul(r->T, cX, cY);
  }
}

__attribute__((target("adx,bmi2")))
__attribute__((always_inline))
static inline void
ge_p3_add_p3_precomp_4(ge_p3_4 *r, const ge_p3_4 *p, const ge_precomp_4 *q) {
  fe4 A, B, C, YplusX, YminusX, D, X3, Y3, Z3, T3;

  fe4_add(YplusX, p->Y, p->X);
  fe4_sub(YminusX, p->Y, p->X);
  fe4_mul(A, YplusX, q->yplusx);
  fe4_mul(B, YminusX, q->yminusx);
  fe4_mul(C, q->xy2d, p->T);
  fe4_add(D, p->Z, p->Z);
  fe4_sub(X3, A, B);
  fe4_add(Y3, A, B);
  fe4_add(Z3, D, C);
  fe4_sub(T3, D, C);
  fe4_mul(r->X, X3, T3);
  fe4_mul(r->Y, Y3, Z3);
  fe4_mul(r->Z, Z3, T3);
  fe4_mul(r->T, X3, Y3);
}

__attribute__((always_inline))
static inline void table_select_4(ge_precomp_4 *t, const int pos,
                                  const signed char b) {
  uint8_t bnegative = constant_time_msb_w(b);
  uint8_t babs = b - ((bnegative & b) << 1);

  uint8_t t_bytes[3][32] = {
    {static_cast<uint8_t>(constant_time_is_zero_w(b) & 1)},
    {static_cast<uint8_t>(constant_time_is_zero_w(b) & 1)},
    {0},
  };
#if defined(__clang__)
  __asm__("" : "+m" (t_bytes) :              );
#endif
  static_assert(sizeof(t_bytes) == sizeof(k25519Precomp[pos][0]), "");
  for (int i = 0; i < 8; i++) {
    constant_time_conditional_memxor(t_bytes, k25519Precomp[pos][i],
                                     sizeof(t_bytes),
                                     constant_time_eq_w(babs, 1 + i));
  }

  static_assert(sizeof(t_bytes) == sizeof(ge_precomp_4), "");

  OPENSSL_memcpy(t, t_bytes, sizeof(ge_precomp_4));

  fe4 xy2d_neg = {0};
  fe4_sub(xy2d_neg, xy2d_neg, t->xy2d);
  constant_time_conditional_memcpy(t->yplusx, t_bytes[1], sizeof(fe4),
                                   bnegative);
  constant_time_conditional_memcpy(t->yminusx, t_bytes[0], sizeof(fe4),
                                   bnegative);
  constant_time_conditional_memcpy(t->xy2d, xy2d_neg, sizeof(fe4), bnegative);
}

__attribute__((target("adx,bmi2")))
void x25519_ge_scalarmult_base_adx(uint8_t h[4][32], const uint8_t a[32]) {
  signed char e[64];
  signed char carry;

  for (unsigned i = 0; i < 32; ++i) {
    e[2 * i + 0] = (a[i] >> 0) & 15;
    e[2 * i + 1] = (a[i] >> 4) & 15;
  }

  carry = 0;
  for (unsigned i = 0; i < 63; ++i) {
    e[i] += carry;
    carry = e[i] + 8;
    carry >>= 4;
    e[i] -= carry << 4;
  }
  e[63] += carry;

  ge_p3_4 r = {{0}, {1}, {1}, {0}};
  for (unsigned i = 1; i < 64; i += 2) {
    ge_precomp_4 t;
    table_select_4(&t, i / 2, e[i]);
    ge_p3_add_p3_precomp_4(&r, &r, &t);
  }

  inline_x25519_ge_dbl_4(&r, &r,            true);
  inline_x25519_ge_dbl_4(&r, &r,            true);
  inline_x25519_ge_dbl_4(&r, &r,            true);
  inline_x25519_ge_dbl_4(&r, &r,            false);

  for (unsigned i = 0; i < 64; i += 2) {
    ge_precomp_4 t;
    table_select_4(&t, i / 2, e[i]);
    ge_p3_add_p3_precomp_4(&r, &r, &t);
  }

  fe4_canon(r.X, r.X);
  fe4_canon(r.Y, r.Y);
  fe4_canon(r.Z, r.Z);
  fe4_canon(r.T, r.T);
  static_assert(sizeof(ge_p3_4) == sizeof(uint8_t[4][32]), "");
  OPENSSL_memcpy(h, &r, sizeof(ge_p3_4));
}
