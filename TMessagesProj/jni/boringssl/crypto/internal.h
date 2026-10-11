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

#ifndef OPENSSL_HEADER_CRYPTO_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_INTERNAL_H

#include <openssl/crypto.h>
#include <openssl/ex_data.h>
#include <openssl/stack.h>
#include <openssl/thread.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#if defined(BORINGSSL_CONSTANT_TIME_VALIDATION)
#include <valgrind/memcheck.h>
#endif

#if defined(BORINGSSL_FIPS_BREAK_TESTS)
#include <stdlib.h>
#endif

#if defined(OPENSSL_THREADS) && \
    (!defined(OPENSSL_WINDOWS) || defined(__MINGW32__))
#include <pthread.h>
#define OPENSSL_PTHREADS
#endif

#if defined(OPENSSL_THREADS) && !defined(OPENSSL_PTHREADS) && \
    defined(OPENSSL_WINDOWS)
#define OPENSSL_WINDOWS_THREADS
#endif

#if defined(OPENSSL_THREADS)
#include <atomic>
#endif

#if defined(OPENSSL_WINDOWS_THREADS)
#include <windows.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(OPENSSL_NO_ASM) && !defined(OPENSSL_STATIC_ARMCAP) && \
    (defined(OPENSSL_X86) || defined(OPENSSL_X86_64) ||            \
     defined(OPENSSL_ARM) || defined(OPENSSL_AARCH64))

#define NEED_CPUID

void OPENSSL_cpuid_setup(void);

void OPENSSL_init_cpuid(void);
#else
inline void OPENSSL_init_cpuid(void) {}
#endif

#if (defined(OPENSSL_ARM) || defined(OPENSSL_AARCH64)) && \
    !defined(OPENSSL_STATIC_ARMCAP)

OPENSSL_EXPORT uint32_t *OPENSSL_get_armcap_pointer_for_test(void);
#endif

#if (!defined(_MSC_VER) || defined(__clang__)) && defined(OPENSSL_64_BIT)
#define BORINGSSL_HAS_UINT128
typedef __int128_t int128_t;
typedef __uint128_t uint128_t;

#if !defined(_MSC_VER) && !defined(OPENSSL_NANOLIBC) && \
    !defined(__EDK2_BORINGSSL__)
#define BORINGSSL_CAN_DIVIDE_UINT128
#endif
#endif

#define OPENSSL_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

#if defined(__clang__) && __clang_major__ >= 5
#if __has_attribute(fallthrough)
#define OPENSSL_CAN_USE_ATTR_FALLTHROUGH
#endif
#endif

#if defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64) || \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define OPENSSL_SSE2
#endif

#if defined(OPENSSL_X86) && !defined(OPENSSL_NO_ASM) && !defined(OPENSSL_SSE2)
#error \
    "x86 assembly requires SSE2. Build with -msse2 (recommended), or disable assembly optimizations with -DOPENSSL_NO_ASM."
#endif

#if defined(OPENSSL_SSE2) && defined(OPENSSL_NO_SSE2_FOR_TESTING)
#undef OPENSSL_SSE2
#endif

#if defined(__GNUC__) || defined(__clang__)
#define OPENSSL_ATTR_CONST __attribute__((const))
#else
#define OPENSSL_ATTR_CONST
#endif

#if defined(BORINGSSL_MALLOC_FAILURE_TESTING)

OPENSSL_EXPORT void OPENSSL_reset_malloc_counter_for_testing(void);

OPENSSL_EXPORT void OPENSSL_disable_malloc_failures_for_testing(void);

OPENSSL_EXPORT void OPENSSL_enable_malloc_failures_for_testing(void);
#else
inline void OPENSSL_reset_malloc_counter_for_testing(void) {}
inline void OPENSSL_disable_malloc_failures_for_testing(void) {}
inline void OPENSSL_enable_malloc_failures_for_testing(void) {}
#endif

#if defined(__has_builtin)
#define OPENSSL_HAS_BUILTIN(x) __has_builtin(x)
#else
#define OPENSSL_HAS_BUILTIN(x) 0
#endif

static inline int buffers_alias(const void *a, size_t a_bytes, const void *b,
                                size_t b_bytes) {

  uintptr_t a_u = (uintptr_t)a;
  uintptr_t b_u = (uintptr_t)b;
  return a_u + a_bytes > b_u && b_u + b_bytes > a_u;
}

static inline void *align_pointer(void *ptr, size_t alignment) {

  assert(alignment != 0 && (alignment & (alignment - 1)) == 0);

  uintptr_t offset = (0u - (uintptr_t)ptr) & (alignment - 1);
  ptr = (char *)ptr + offset;
  assert(((uintptr_t)ptr & (alignment - 1)) == 0);
  return ptr;
}

#if defined(OPENSSL_64_BIT)
typedef uint64_t crypto_word_t;
#elif defined(OPENSSL_32_BIT)
typedef uint32_t crypto_word_t;
#else
#error "Must define either OPENSSL_32_BIT or OPENSSL_64_BIT"
#endif

#define CONSTTIME_TRUE_W ~((crypto_word_t)0)
#define CONSTTIME_FALSE_W ((crypto_word_t)0)
#define CONSTTIME_TRUE_8 ((uint8_t)0xff)
#define CONSTTIME_FALSE_8 ((uint8_t)0)

static inline crypto_word_t value_barrier_w(crypto_word_t a) {
#if defined(__GNUC__) || defined(__clang__)
  __asm__("" : "+r"(a) :                );
#endif
  return a;
}

static inline uint32_t value_barrier_u32(uint32_t a) {
#if defined(__GNUC__) || defined(__clang__)
  __asm__("" : "+r"(a) :                );
#endif
  return a;
}

static inline uint64_t value_barrier_u64(uint64_t a) {
#if defined(__GNUC__) || defined(__clang__)
  __asm__("" : "+r"(a) :                );
#endif
  return a;
}

static inline crypto_word_t constant_time_msb_w(crypto_word_t a) {
  return 0u - (a >> (sizeof(a) * 8 - 1));
}

static inline crypto_word_t constant_time_lt_w(crypto_word_t a,
                                               crypto_word_t b) {

  return constant_time_msb_w(a ^ ((a ^ b) | ((a - b) ^ a)));
}

static inline uint8_t constant_time_lt_8(crypto_word_t a, crypto_word_t b) {
  return (uint8_t)(constant_time_lt_w(a, b));
}

static inline crypto_word_t constant_time_ge_w(crypto_word_t a,
                                               crypto_word_t b) {
  return ~constant_time_lt_w(a, b);
}

static inline uint8_t constant_time_ge_8(crypto_word_t a, crypto_word_t b) {
  return (uint8_t)(constant_time_ge_w(a, b));
}

static inline crypto_word_t constant_time_is_zero_w(crypto_word_t a) {

  return constant_time_msb_w(~a & (a - 1));
}

static inline uint8_t constant_time_is_zero_8(crypto_word_t a) {
  return (uint8_t)(constant_time_is_zero_w(a));
}

static inline crypto_word_t constant_time_eq_w(crypto_word_t a,
                                               crypto_word_t b) {
  return constant_time_is_zero_w(a ^ b);
}

static inline uint8_t constant_time_eq_8(crypto_word_t a, crypto_word_t b) {
  return (uint8_t)(constant_time_eq_w(a, b));
}

static inline crypto_word_t constant_time_eq_int(int a, int b) {
  return constant_time_eq_w((crypto_word_t)(a), (crypto_word_t)(b));
}

static inline uint8_t constant_time_eq_int_8(int a, int b) {
  return constant_time_eq_8((crypto_word_t)(a), (crypto_word_t)(b));
}

static inline crypto_word_t constant_time_select_w(crypto_word_t mask,
                                                   crypto_word_t a,
                                                   crypto_word_t b) {

  mask = value_barrier_w(mask);
  return (mask & a) | (~mask & b);
}

static inline uint8_t constant_time_select_8(crypto_word_t mask, uint8_t a,
                                             uint8_t b) {

  uint8_t m = value_barrier_w(mask);
  return (m & a) | (~m & b);
}

static inline int constant_time_select_int(crypto_word_t mask, int a, int b) {
  return (int)(constant_time_select_w(mask, (crypto_word_t)(a),
                                      (crypto_word_t)(b)));
}

static inline void constant_time_conditional_memcpy(void *dst, const void *src,
                                                    const size_t n,
                                                    const crypto_word_t mask) {
  assert(!buffers_alias(dst, n, src, n));
  uint8_t *out = (uint8_t *)dst;
  const uint8_t *in = (const uint8_t *)src;
  for (size_t i = 0; i < n; i++) {
    out[i] = constant_time_select_8(mask, in[i], out[i]);
  }
}

static inline void constant_time_conditional_memxor(void *dst, const void *src,
                                                    size_t n,
                                                    const crypto_word_t mask) {
  assert(!buffers_alias(dst, n, src, n));
  uint8_t *out = (uint8_t *)dst;
  const uint8_t *in = (const uint8_t *)src;
#if defined(__GNUC__) && !defined(__clang__)

  typedef uint8_t v32u8 __attribute__((vector_size(32), aligned(1), may_alias));
  size_t n_vec = n & ~(size_t)31;
  v32u8 masks = ((uint8_t)mask - (v32u8){});
  for (size_t i = 0; i < n_vec; i += 32) {
    *(v32u8 *)&out[i] ^= masks & *(v32u8 *)&in[i];
  }
  out += n_vec;
  n -= n_vec;
#endif
  for (size_t i = 0; i < n; i++) {
    out[i] ^= value_barrier_w(mask) & in[i];
  }
}

#if defined(BORINGSSL_CONSTANT_TIME_VALIDATION)

#define CONSTTIME_SECRET(ptr, len) VALGRIND_MAKE_MEM_UNDEFINED(ptr, len)

#define CONSTTIME_DECLASSIFY(ptr, len) VALGRIND_MAKE_MEM_DEFINED(ptr, len)

#else

#define CONSTTIME_SECRET(ptr, len)
#define CONSTTIME_DECLASSIFY(ptr, len)

#endif

static inline crypto_word_t constant_time_declassify_w(crypto_word_t v) {

  CONSTTIME_DECLASSIFY(&v, sizeof(v));
  return value_barrier_w(v);
}

static inline int constant_time_declassify_int(int v) {
  static_assert(sizeof(uint32_t) == sizeof(int),
                "int is not the same size as uint32_t");

  CONSTTIME_DECLASSIFY(&v, sizeof(v));
  return value_barrier_u32(v);
}

#define declassify_assert(expr) assert(constant_time_declassify_int(expr))

#if !defined(OPENSSL_THREADS)
typedef uint32_t CRYPTO_once_t;
#define CRYPTO_ONCE_INIT 0
#elif defined(OPENSSL_WINDOWS_THREADS)
typedef INIT_ONCE CRYPTO_once_t;
#define CRYPTO_ONCE_INIT INIT_ONCE_STATIC_INIT
#elif defined(OPENSSL_PTHREADS)
typedef pthread_once_t CRYPTO_once_t;
#define CRYPTO_ONCE_INIT PTHREAD_ONCE_INIT
#else
#error "Unknown threading library"
#endif

OPENSSL_EXPORT void CRYPTO_once(CRYPTO_once_t *once, void (*init)(void));

#if defined(OPENSSL_THREADS)

using CRYPTO_atomic_u32 = std::atomic<uint32_t>;

static_assert(sizeof(CRYPTO_atomic_u32) == sizeof(uint32_t), "");

inline uint32_t CRYPTO_atomic_load_u32(const CRYPTO_atomic_u32 *val) {
  return val->load(std::memory_order_seq_cst);
}

inline bool CRYPTO_atomic_compare_exchange_weak_u32(CRYPTO_atomic_u32 *val,
                                                    uint32_t *expected,
                                                    uint32_t desired) {
  return val->compare_exchange_weak(
      *expected, desired, std::memory_order_seq_cst, std::memory_order_seq_cst);
}

inline void CRYPTO_atomic_store_u32(CRYPTO_atomic_u32 *val, uint32_t desired) {
  val->store(desired, std::memory_order_seq_cst);
}

#else

typedef uint32_t CRYPTO_atomic_u32;

inline uint32_t CRYPTO_atomic_load_u32(CRYPTO_atomic_u32 *val) { return *val; }

inline int CRYPTO_atomic_compare_exchange_weak_u32(CRYPTO_atomic_u32 *val,
                                                   uint32_t *expected,
                                                   uint32_t desired) {
  if (*val != *expected) {
    *expected = *val;
    return 0;
  }
  *val = desired;
  return 1;
}

inline void CRYPTO_atomic_store_u32(CRYPTO_atomic_u32 *val, uint32_t desired) {
  *val = desired;
}

#endif

static_assert(sizeof(CRYPTO_atomic_u32) == sizeof(uint32_t),
              "CRYPTO_atomic_u32 does not match uint32_t size");
static_assert(alignof(CRYPTO_atomic_u32) == alignof(uint32_t),
              "CRYPTO_atomic_u32 does not match uint32_t alignment");

#define CRYPTO_REFCOUNT_MAX 0xffffffff

OPENSSL_EXPORT void CRYPTO_refcount_inc(CRYPTO_refcount_t *count);

OPENSSL_EXPORT int CRYPTO_refcount_dec_and_test_zero(CRYPTO_refcount_t *count);

#if !defined(OPENSSL_THREADS)
typedef struct crypto_mutex_st {
  char padding;
} CRYPTO_MUTEX;
#define CRYPTO_MUTEX_INIT {0}
#elif defined(OPENSSL_WINDOWS_THREADS)
typedef SRWLOCK CRYPTO_MUTEX;
#define CRYPTO_MUTEX_INIT SRWLOCK_INIT
#elif defined(OPENSSL_PTHREADS)
typedef pthread_rwlock_t CRYPTO_MUTEX;
#define CRYPTO_MUTEX_INIT PTHREAD_RWLOCK_INITIALIZER
#else
#error "Unknown threading library"
#endif

OPENSSL_EXPORT void CRYPTO_MUTEX_init(CRYPTO_MUTEX *lock);

OPENSSL_EXPORT void CRYPTO_MUTEX_lock_read(CRYPTO_MUTEX *lock);

OPENSSL_EXPORT void CRYPTO_MUTEX_lock_write(CRYPTO_MUTEX *lock);

OPENSSL_EXPORT void CRYPTO_MUTEX_unlock_read(CRYPTO_MUTEX *lock);

OPENSSL_EXPORT void CRYPTO_MUTEX_unlock_write(CRYPTO_MUTEX *lock);

OPENSSL_EXPORT void CRYPTO_MUTEX_cleanup(CRYPTO_MUTEX *lock);

#if defined(__cplusplus)
extern "C++" {

BSSL_NAMESPACE_BEGIN

namespace internal {

template <void (*LockFunc)(CRYPTO_MUTEX *), void (*ReleaseFunc)(CRYPTO_MUTEX *)>
class MutexLockBase {
 public:
  explicit MutexLockBase(CRYPTO_MUTEX *mu) : mu_(mu) {
    assert(mu_ != nullptr);
    LockFunc(mu_);
  }
  ~MutexLockBase() { ReleaseFunc(mu_); }
  MutexLockBase(const MutexLockBase<LockFunc, ReleaseFunc> &) = delete;
  MutexLockBase &operator=(const MutexLockBase<LockFunc, ReleaseFunc> &) =
      delete;

 private:
  CRYPTO_MUTEX *const mu_;
};

}

using MutexWriteLock =
    internal::MutexLockBase<CRYPTO_MUTEX_lock_write, CRYPTO_MUTEX_unlock_write>;
using MutexReadLock =
    internal::MutexLockBase<CRYPTO_MUTEX_lock_read, CRYPTO_MUTEX_unlock_read>;

BSSL_NAMESPACE_END

}
#endif

typedef enum {
  OPENSSL_THREAD_LOCAL_ERR = 0,
  OPENSSL_THREAD_LOCAL_RAND,
  OPENSSL_THREAD_LOCAL_FIPS_COUNTERS,
  OPENSSL_THREAD_LOCAL_FIPS_SERVICE_INDICATOR_STATE,
  OPENSSL_THREAD_LOCAL_TEST,
  NUM_OPENSSL_THREAD_LOCALS,
} thread_local_data_t;

typedef void (*thread_local_destructor_t)(void *);

OPENSSL_EXPORT void *CRYPTO_get_thread_local(thread_local_data_t value);

OPENSSL_EXPORT int CRYPTO_set_thread_local(
    thread_local_data_t index, void *value,
    thread_local_destructor_t destructor);

typedef struct crypto_ex_data_func_st CRYPTO_EX_DATA_FUNCS;

typedef struct {
  CRYPTO_MUTEX lock;

  CRYPTO_EX_DATA_FUNCS *funcs, *last;

  CRYPTO_atomic_u32 num_funcs;

  uint8_t num_reserved;
} CRYPTO_EX_DATA_CLASS;

#define CRYPTO_EX_DATA_CLASS_INIT {CRYPTO_MUTEX_INIT, NULL, NULL, {}, 0}
#define CRYPTO_EX_DATA_CLASS_INIT_WITH_APP_DATA \
  {CRYPTO_MUTEX_INIT, NULL, NULL, {}, 1}

OPENSSL_EXPORT int CRYPTO_get_ex_new_index_ex(
    CRYPTO_EX_DATA_CLASS *ex_data_class, long argl, void *argp,
    CRYPTO_EX_free *free_func);

OPENSSL_EXPORT int CRYPTO_set_ex_data(CRYPTO_EX_DATA *ad, int index, void *val);

OPENSSL_EXPORT void *CRYPTO_get_ex_data(const CRYPTO_EX_DATA *ad, int index);

OPENSSL_EXPORT void CRYPTO_new_ex_data(CRYPTO_EX_DATA *ad);

OPENSSL_EXPORT void CRYPTO_free_ex_data(CRYPTO_EX_DATA_CLASS *ex_data_class,
                                        void *obj, CRYPTO_EX_DATA *ad);

#if defined(__GNUC__) && __GNUC__ >= 2
static inline uint16_t CRYPTO_bswap2(uint16_t x) {
  return __builtin_bswap16(x);
}

static inline uint32_t CRYPTO_bswap4(uint32_t x) {
  return __builtin_bswap32(x);
}

static inline uint64_t CRYPTO_bswap8(uint64_t x) {
  return __builtin_bswap64(x);
}
#elif defined(_MSC_VER)
#pragma intrinsic(_byteswap_uint64, _byteswap_ulong, _byteswap_ushort)
static inline uint16_t CRYPTO_bswap2(uint16_t x) { return _byteswap_ushort(x); }

static inline uint32_t CRYPTO_bswap4(uint32_t x) { return _byteswap_ulong(x); }

static inline uint64_t CRYPTO_bswap8(uint64_t x) { return _byteswap_uint64(x); }
#else
static inline uint16_t CRYPTO_bswap2(uint16_t x) { return (x >> 8) | (x << 8); }

static inline uint32_t CRYPTO_bswap4(uint32_t x) {
  x = (x >> 16) | (x << 16);
  x = ((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8);
  return x;
}

static inline uint64_t CRYPTO_bswap8(uint64_t x) {
  return CRYPTO_bswap4(x >> 32) | (((uint64_t)CRYPTO_bswap4(x)) << 32);
}
#endif

#if defined(__cplusplus)
extern "C++" {

static inline const void *OPENSSL_memchr(const void *s, int c, size_t n) {
  if (n == 0) {
    return NULL;
  }

  return memchr(s, c, n);
}

static inline void *OPENSSL_memchr(void *s, int c, size_t n) {
  if (n == 0) {
    return NULL;
  }

  return memchr(s, c, n);
}

}
#else

static inline void *OPENSSL_memchr(const void *s, int c, size_t n) {
  if (n == 0) {
    return NULL;
  }

  return memchr(s, c, n);
}

#endif

static inline int OPENSSL_memcmp(const void *s1, const void *s2, size_t n) {
  if (n == 0) {
    return 0;
  }

  return memcmp(s1, s2, n);
}

static inline void *OPENSSL_memcpy(void *dst, const void *src, size_t n) {
  if (n == 0) {
    return dst;
  }

  return memcpy(dst, src, n);
}

static inline void *OPENSSL_memmove(void *dst, const void *src, size_t n) {
  if (n == 0) {
    return dst;
  }

  return memmove(dst, src, n);
}

static inline void *OPENSSL_memset(void *dst, int c, size_t n) {
  if (n == 0) {
    return dst;
  }

  return memset(dst, c, n);
}

static inline uint16_t CRYPTO_load_u16_be(const void *in) {
  uint16_t v;
  OPENSSL_memcpy(&v, in, sizeof(v));
  return CRYPTO_bswap2(v);
}

static inline void CRYPTO_store_u16_be(void *out, uint16_t v) {
  v = CRYPTO_bswap2(v);
  OPENSSL_memcpy(out, &v, sizeof(v));
}

static inline uint32_t CRYPTO_load_u32_le(const void *in) {
  uint32_t v;
  OPENSSL_memcpy(&v, in, sizeof(v));
  return v;
}

static inline void CRYPTO_store_u32_le(void *out, uint32_t v) {
  OPENSSL_memcpy(out, &v, sizeof(v));
}

static inline uint32_t CRYPTO_load_u32_be(const void *in) {
  uint32_t v;
  OPENSSL_memcpy(&v, in, sizeof(v));
  return CRYPTO_bswap4(v);
}

static inline void CRYPTO_store_u32_be(void *out, uint32_t v) {
  v = CRYPTO_bswap4(v);
  OPENSSL_memcpy(out, &v, sizeof(v));
}

static inline uint64_t CRYPTO_load_u64_le(const void *in) {
  uint64_t v;
  OPENSSL_memcpy(&v, in, sizeof(v));
  return v;
}

static inline void CRYPTO_store_u64_le(void *out, uint64_t v) {
  OPENSSL_memcpy(out, &v, sizeof(v));
}

static inline uint64_t CRYPTO_load_u64_be(const void *ptr) {
  uint64_t ret;
  OPENSSL_memcpy(&ret, ptr, sizeof(ret));
  return CRYPTO_bswap8(ret);
}

static inline void CRYPTO_store_u64_be(void *out, uint64_t v) {
  v = CRYPTO_bswap8(v);
  OPENSSL_memcpy(out, &v, sizeof(v));
}

static inline crypto_word_t CRYPTO_load_word_le(const void *in) {
  crypto_word_t v;
  OPENSSL_memcpy(&v, in, sizeof(v));
  return v;
}

static inline void CRYPTO_store_word_le(void *out, crypto_word_t v) {
  OPENSSL_memcpy(out, &v, sizeof(v));
}

static inline crypto_word_t CRYPTO_load_word_be(const void *in) {
  crypto_word_t v;
  OPENSSL_memcpy(&v, in, sizeof(v));
#if defined(OPENSSL_64_BIT)
  static_assert(sizeof(v) == 8, "crypto_word_t has unexpected size");
  return CRYPTO_bswap8(v);
#else
  static_assert(sizeof(v) == 4, "crypto_word_t has unexpected size");
  return CRYPTO_bswap4(v);
#endif
}

static inline uint32_t CRYPTO_rotl_u32(uint32_t value, int shift) {
#if defined(_MSC_VER)
  return _rotl(value, shift);
#else
  return (value << shift) | (value >> ((-shift) & 31));
#endif
}

static inline uint32_t CRYPTO_rotr_u32(uint32_t value, int shift) {
#if defined(_MSC_VER)
  return _rotr(value, shift);
#else
  return (value >> shift) | (value << ((-shift) & 31));
#endif
}

static inline uint64_t CRYPTO_rotl_u64(uint64_t value, int shift) {
#if defined(_MSC_VER)
  return _rotl64(value, shift);
#else
  return (value << shift) | (value >> ((-shift) & 63));
#endif
}

static inline uint64_t CRYPTO_rotr_u64(uint64_t value, int shift) {
#if defined(_MSC_VER)
  return _rotr64(value, shift);
#else
  return (value >> shift) | (value << ((-shift) & 63));
#endif
}

#if defined(BORINGSSL_FIPS)

void BORINGSSL_FIPS_abort(void) __attribute__((noreturn));

int boringssl_self_test_startup(void);

void boringssl_ensure_rsa_self_test(void);

void boringssl_ensure_ecc_self_test(void);

void boringssl_ensure_ffdh_self_test(void);

#else

inline void boringssl_ensure_rsa_self_test(void) {}
inline void boringssl_ensure_ecc_self_test(void) {}
inline void boringssl_ensure_ffdh_self_test(void) {}

#endif

int BORINGSSL_check_test(const void *expected, const void *actual,
                         size_t expected_len, const char *name);

int boringssl_self_test_sha256(void);

int boringssl_self_test_sha512(void);

int boringssl_self_test_hmac_sha256(void);

OPENSSL_EXPORT int boringssl_self_test_mlkem(void);

OPENSSL_EXPORT int boringssl_self_test_mldsa(void);

OPENSSL_EXPORT int boringssl_self_test_slhdsa(void);

#if defined(BORINGSSL_FIPS_COUNTERS)
void boringssl_fips_inc_counter(enum fips_counter_t counter);
#else
inline void boringssl_fips_inc_counter(enum fips_counter_t counter) {}
#endif

#if defined(BORINGSSL_FIPS_BREAK_TESTS)
inline int boringssl_fips_break_test(const char *test) {
  const char *const value = getenv("BORINGSSL_FIPS_BREAK_TEST");
  return value != NULL && strcmp(value, test) == 0;
}
#else
inline int boringssl_fips_break_test(const char *test) { return 0; }
#endif

#if defined(OPENSSL_X86) || defined(OPENSSL_X86_64)

extern uint32_t OPENSSL_ia32cap_P[4];

OPENSSL_ATTR_CONST uint32_t OPENSSL_get_ia32cap(int idx);

inline int CRYPTO_is_intel_cpu(void) {

  return (OPENSSL_get_ia32cap(0) & (1u << 30)) != 0;
}

inline int CRYPTO_is_PCLMUL_capable(void) {
#if defined(__PCLMUL__)
  return 1;
#else
  return (OPENSSL_get_ia32cap(1) & (1u << 1)) != 0;
#endif
}

inline int CRYPTO_is_SSSE3_capable(void) {
#if defined(__SSSE3__)
  return 1;
#else
  return (OPENSSL_get_ia32cap(1) & (1u << 9)) != 0;
#endif
}

inline int CRYPTO_is_SSE4_1_capable(void) {
#if defined(__SSE4_1__)
  return 1;
#else
  return (OPENSSL_get_ia32cap(1) & (1u << 19)) != 0;
#endif
}

inline int CRYPTO_is_MOVBE_capable(void) {
#if defined(__MOVBE__)
  return 1;
#else
  return (OPENSSL_get_ia32cap(1) & (1u << 22)) != 0;
#endif
}

inline int CRYPTO_is_AESNI_capable(void) {
#if defined(__AES__)
  return 1;
#else
  return (OPENSSL_get_ia32cap(1) & (1u << 25)) != 0;
#endif
}

inline int CRYPTO_is_AVX_capable(void) {
#if defined(__AVX__)
  return 1;
#else
  return (OPENSSL_get_ia32cap(1) & (1u << 28)) != 0;
#endif
}

inline int CRYPTO_is_RDRAND_capable(void) {

  return (OPENSSL_get_ia32cap(1) & (1u << 30)) != 0;
}

inline int CRYPTO_is_BMI1_capable(void) {
#if defined(__BMI__)
  return 1;
#else
  return (OPENSSL_get_ia32cap(2) & (1u << 3)) != 0;
#endif
}

inline int CRYPTO_is_AVX2_capable(void) {
#if defined(__AVX2__)
  return 1;
#else
  return (OPENSSL_get_ia32cap(2) & (1u << 5)) != 0;
#endif
}

inline int CRYPTO_is_BMI2_capable(void) {
#if defined(__BMI2__)
  return 1;
#else
  return (OPENSSL_get_ia32cap(2) & (1u << 8)) != 0;
#endif
}

inline int CRYPTO_is_ADX_capable(void) {
#if defined(__ADX__)
  return 1;
#else
  return (OPENSSL_get_ia32cap(2) & (1u << 19)) != 0;
#endif
}

inline int CRYPTO_is_x86_SHA_capable(void) {
#if defined(__SHA__)
  return 1;
#else
  return (OPENSSL_get_ia32cap(2) & (1u << 29)) != 0;
#endif
}

inline int CRYPTO_cpu_perf_is_like_silvermont(void) {

  int hardware_supports_xsave = (OPENSSL_get_ia32cap(1) & (1u << 26)) != 0;
  return !hardware_supports_xsave && CRYPTO_is_MOVBE_capable();
}

inline int CRYPTO_is_AVX512BW_capable(void) {
#if defined(__AVX512BW__)
  return 1;
#else
  return (OPENSSL_get_ia32cap(2) & (1u << 30)) != 0;
#endif
}

inline int CRYPTO_is_AVX512VL_capable(void) {
#if defined(__AVX512VL__)
  return 1;
#else
  return (OPENSSL_get_ia32cap(2) & (1u << 31)) != 0;
#endif
}

inline int CRYPTO_cpu_avoid_zmm_registers(void) {
  return (OPENSSL_get_ia32cap(2) & (1u << 14)) != 0;
}

inline int CRYPTO_is_VAES_capable(void) {
#if defined(__VAES__)
  return 1;
#else
  return (OPENSSL_get_ia32cap(3) & (1u << 9)) != 0;
#endif
}

inline int CRYPTO_is_VPCLMULQDQ_capable(void) {
#if defined(__VPCLMULQDQ__)
  return 1;
#else
  return (OPENSSL_get_ia32cap(3) & (1u << 10)) != 0;
#endif
}

#endif

#if defined(OPENSSL_ARM) || defined(OPENSSL_AARCH64)

#define ARMV7_NEON (1 << 0)

#define ARMV8_AES (1 << 2)

#define ARMV8_SHA1 (1 << 3)

#define ARMV8_SHA256 (1 << 4)

#define ARMV8_PMULL (1 << 5)

#define ARMV8_SHA512 (1 << 6)

extern uint32_t OPENSSL_armcap_P;

OPENSSL_ATTR_CONST uint32_t OPENSSL_get_armcap(void);

#if defined(__ARM_NEON__) && !defined(__ARM_NEON)
#define __ARM_NEON 1
#endif
#if defined(__ARM_FEATURE_CRYPTO)
#if !defined(__ARM_FEATURE_AES)
#define __ARM_FEATURE_AES 1
#endif
#if !defined(__ARM_FEATURE_SHA2)
#define __ARM_FEATURE_SHA2 1
#endif
#endif

inline int CRYPTO_is_NEON_capable(void) {
#if defined(OPENSSL_STATIC_ARMCAP_NEON) || defined(__ARM_NEON)
  return 1;
#elif defined(OPENSSL_STATIC_ARMCAP)
  return 0;
#else
  return (OPENSSL_get_armcap() & ARMV7_NEON) != 0;
#endif
}

inline int CRYPTO_is_ARMv8_AES_capable(void) {
#if defined(OPENSSL_STATIC_ARMCAP_AES) || defined(__ARM_FEATURE_AES)
  return 1;
#elif defined(OPENSSL_STATIC_ARMCAP)
  return 0;
#else
  return (OPENSSL_get_armcap() & ARMV8_AES) != 0;
#endif
}

inline int CRYPTO_is_ARMv8_PMULL_capable(void) {
#if defined(OPENSSL_STATIC_ARMCAP_PMULL) || defined(__ARM_FEATURE_AES)
  return 1;
#elif defined(OPENSSL_STATIC_ARMCAP)
  return 0;
#else
  return (OPENSSL_get_armcap() & ARMV8_PMULL) != 0;
#endif
}

inline int CRYPTO_is_ARMv8_SHA1_capable(void) {

#if defined(OPENSSL_STATIC_ARMCAP_SHA1) || defined(__ARM_FEATURE_SHA2)
  return 1;
#elif defined(OPENSSL_STATIC_ARMCAP)
  return 0;
#else
  return (OPENSSL_get_armcap() & ARMV8_SHA1) != 0;
#endif
}

inline int CRYPTO_is_ARMv8_SHA256_capable(void) {

#if defined(OPENSSL_STATIC_ARMCAP_SHA256) || defined(__ARM_FEATURE_SHA2)
  return 1;
#elif defined(OPENSSL_STATIC_ARMCAP)
  return 0;
#else
  return (OPENSSL_get_armcap() & ARMV8_SHA256) != 0;
#endif
}

inline int CRYPTO_is_ARMv8_SHA512_capable(void) {

#if defined(__ARM_FEATURE_SHA512)
  return 1;
#elif defined(OPENSSL_STATIC_ARMCAP)
  return 0;
#else
  return (OPENSSL_get_armcap() & ARMV8_SHA512) != 0;
#endif
}

#endif

#if defined(BORINGSSL_DISPATCH_TEST)

extern uint8_t BORINGSSL_function_hit[8];
#endif

OPENSSL_EXPORT int OPENSSL_vasprintf_internal(char **str, const char *format,
                                              va_list args, int system_malloc)
    OPENSSL_PRINTF_FORMAT_FUNC(2, 0);

#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)

int CRYPTO_fuzzer_mode_enabled(void);
#else
inline int CRYPTO_fuzzer_mode_enabled(void) { return 0; }
#endif

#if defined(__cplusplus)
}
#endif

#if OPENSSL_HAS_BUILTIN(__builtin_addc)

inline unsigned int CRYPTO_addc_impl(unsigned int x, unsigned int y,
                                     unsigned int carry,
                                     unsigned int *out_carry) {
  return __builtin_addc(x, y, carry, out_carry);
}

inline unsigned long CRYPTO_addc_impl(unsigned long x, unsigned long y,
                                      unsigned long carry,
                                      unsigned long *out_carry) {
  return __builtin_addcl(x, y, carry, out_carry);
}

inline unsigned long long CRYPTO_addc_impl(unsigned long long x,
                                           unsigned long long y,
                                           unsigned long long carry,
                                           unsigned long long *out_carry) {
  return __builtin_addcll(x, y, carry, out_carry);
}

inline uint32_t CRYPTO_addc_u32(uint32_t x, uint32_t y, uint32_t carry,
                                uint32_t *out_carry) {
  return CRYPTO_addc_impl(x, y, carry, out_carry);
}

inline uint64_t CRYPTO_addc_u64(uint64_t x, uint64_t y, uint64_t carry,
                                uint64_t *out_carry) {
  return CRYPTO_addc_impl(x, y, carry, out_carry);
}

#else

static inline uint32_t CRYPTO_addc_u32(uint32_t x, uint32_t y, uint32_t carry,
                                       uint32_t *out_carry) {
  declassify_assert(carry <= 1);
  uint64_t ret = carry;
  ret += (uint64_t)x + y;
  *out_carry = (uint32_t)(ret >> 32);
  return (uint32_t)ret;
}

static inline uint64_t CRYPTO_addc_u64(uint64_t x, uint64_t y, uint64_t carry,
                                       uint64_t *out_carry) {
  declassify_assert(carry <= 1);
#if defined(BORINGSSL_HAS_UINT128)
  uint128_t ret = carry;
  ret += (uint128_t)x + y;
  *out_carry = (uint64_t)(ret >> 64);
  return (uint64_t)ret;
#else
  x += carry;
  carry = x < carry;
  uint64_t ret = x + y;
  carry += ret < x;
  *out_carry = carry;
  return ret;
#endif
}
#endif

#if OPENSSL_HAS_BUILTIN(__builtin_subc)

inline unsigned int CRYPTO_subc_impl(unsigned int x, unsigned int y,
                                     unsigned int borrow,
                                     unsigned int *out_borrow) {
  return __builtin_subc(x, y, borrow, out_borrow);
}

inline unsigned long CRYPTO_subc_impl(unsigned long x, unsigned long y,
                                      unsigned long borrow,
                                      unsigned long *out_borrow) {
  return __builtin_subcl(x, y, borrow, out_borrow);
}

inline unsigned long long CRYPTO_subc_impl(unsigned long long x,
                                           unsigned long long y,
                                           unsigned long long borrow,
                                           unsigned long long *out_borrow) {
  return __builtin_subcll(x, y, borrow, out_borrow);
}

inline uint32_t CRYPTO_subc_u32(uint32_t x, uint32_t y, uint32_t borrow,
                                uint32_t *out_borrow) {
  return CRYPTO_subc_impl(x, y, borrow, out_borrow);
}

inline uint64_t CRYPTO_subc_u64(uint64_t x, uint64_t y, uint64_t borrow,
                                uint64_t *out_borrow) {
  return CRYPTO_subc_impl(x, y, borrow, out_borrow);
}

#else

static inline uint32_t CRYPTO_subc_u32(uint32_t x, uint32_t y, uint32_t borrow,
                                       uint32_t *out_borrow) {
  declassify_assert(borrow <= 1);
  uint32_t ret = x - y - borrow;
  *out_borrow = (x < y) | ((x == y) & borrow);
  return ret;
}

static inline uint64_t CRYPTO_subc_u64(uint64_t x, uint64_t y, uint64_t borrow,
                                       uint64_t *out_borrow) {
  declassify_assert(borrow <= 1);
  uint64_t ret = x - y - borrow;
  *out_borrow = (x < y) | ((x == y) & borrow);
  return ret;
}
#endif

#if defined(OPENSSL_64_BIT)
#define CRYPTO_addc_w CRYPTO_addc_u64
#define CRYPTO_subc_w CRYPTO_subc_u64
#else
#define CRYPTO_addc_w CRYPTO_addc_u32
#define CRYPTO_subc_w CRYPTO_subc_u32
#endif

#endif
