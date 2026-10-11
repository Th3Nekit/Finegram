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

#ifndef OPENSSL_HEADER_STACK_H
#define OPENSSL_HEADER_STACK_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define STACK_OF(type) struct stack_st_##type

#define DECLARE_STACK_OF(type) STACK_OF(type);

#define DEFINE_NAMED_STACK_OF(name, type)                    \
  BORINGSSL_DEFINE_STACK_OF_IMPL(name, type *, const type *) \
  BORINGSSL_DEFINE_STACK_TRAITS(name, type, false)

#define DEFINE_STACK_OF(type) DEFINE_NAMED_STACK_OF(type, type)

#define DEFINE_CONST_STACK_OF(type)                                \
  BORINGSSL_DEFINE_STACK_OF_IMPL(type, const type *, const type *) \
  BORINGSSL_DEFINE_STACK_TRAITS(type, const type, true)

#if 0

typedef void (*sk_SAMPLE_free_func)(SAMPLE *);

typedef SAMPLE *(*sk_SAMPLE_copy_func)(const SAMPLE *);

typedef int (*sk_SAMPLE_cmp_func)(const SAMPLE *const *a,
                                  const SAMPLE *const *b);

STACK_OF(SAMPLE) *sk_SAMPLE_new(sk_SAMPLE_cmp_func comp);

STACK_OF(SAMPLE) *sk_SAMPLE_new_null(void);

size_t sk_SAMPLE_num(const STACK_OF(SAMPLE) *sk);

void sk_SAMPLE_zero(STACK_OF(SAMPLE) *sk);

SAMPLE *sk_SAMPLE_value(const STACK_OF(SAMPLE) *sk, size_t i);

SAMPLE *sk_SAMPLE_set(STACK_OF(SAMPLE) *sk, size_t i, SAMPLE *p);

void sk_SAMPLE_free(STACK_OF(SAMPLE) *sk);

void sk_SAMPLE_pop_free(STACK_OF(SAMPLE) *sk, sk_SAMPLE_free_func free_func);

size_t sk_SAMPLE_insert(STACK_OF(SAMPLE) *sk, SAMPLE *p, size_t where);

SAMPLE *sk_SAMPLE_delete(STACK_OF(SAMPLE) *sk, size_t where);

SAMPLE *sk_SAMPLE_delete_ptr(STACK_OF(SAMPLE) *sk, const SAMPLE *p);

typedef int (*sk_SAMPLE_delete_if_func)(SAMPLE *p, void *data);

void sk_SAMPLE_delete_if(STACK_OF(SAMPLE) *sk, sk_SAMPLE_delete_if_func func,
                         void *data);

int sk_SAMPLE_find(const STACK_OF(SAMPLE) *sk, size_t *out_index,
                   const SAMPLE *p);

SAMPLE *sk_SAMPLE_shift(STACK_OF(SAMPLE) *sk);

size_t sk_SAMPLE_push(STACK_OF(SAMPLE) *sk, SAMPLE *p);

SAMPLE *sk_SAMPLE_pop(STACK_OF(SAMPLE) *sk);

STACK_OF(SAMPLE) *sk_SAMPLE_dup(const STACK_OF(SAMPLE) *sk);

void sk_SAMPLE_sort(STACK_OF(SAMPLE) *sk);

int sk_SAMPLE_is_sorted(const STACK_OF(SAMPLE) *sk);

sk_SAMPLE_cmp_func sk_SAMPLE_set_cmp_func(STACK_OF(SAMPLE) *sk,
                                          sk_SAMPLE_cmp_func comp);

STACK_OF(SAMPLE) *sk_SAMPLE_deep_copy(const STACK_OF(SAMPLE) *sk,
                                      sk_SAMPLE_copy_func copy_func,
                                      sk_SAMPLE_free_func free_func);

#endif

typedef void (*OPENSSL_sk_free_func)(void *ptr);

typedef void *(*OPENSSL_sk_copy_func)(const void *ptr);

typedef int (*OPENSSL_sk_cmp_func)(const void *const *a, const void *const *b);

typedef int (*OPENSSL_sk_delete_if_func)(void *obj, void *data);

typedef void (*OPENSSL_sk_call_free_func)(OPENSSL_sk_free_func, void *);
typedef void *(*OPENSSL_sk_call_copy_func)(OPENSSL_sk_copy_func, const void *);
typedef int (*OPENSSL_sk_call_cmp_func)(OPENSSL_sk_cmp_func, const void *,
                                        const void *);
typedef int (*OPENSSL_sk_call_delete_if_func)(OPENSSL_sk_delete_if_func, void *,
                                              void *);

typedef struct stack_st OPENSSL_STACK;

OPENSSL_EXPORT OPENSSL_STACK *OPENSSL_sk_new(OPENSSL_sk_cmp_func comp);
OPENSSL_EXPORT OPENSSL_STACK *OPENSSL_sk_new_null(void);
OPENSSL_EXPORT size_t OPENSSL_sk_num(const OPENSSL_STACK *sk);
OPENSSL_EXPORT void OPENSSL_sk_zero(OPENSSL_STACK *sk);
OPENSSL_EXPORT void *OPENSSL_sk_value(const OPENSSL_STACK *sk, size_t i);
OPENSSL_EXPORT void *OPENSSL_sk_set(OPENSSL_STACK *sk, size_t i, void *p);
OPENSSL_EXPORT void OPENSSL_sk_free(OPENSSL_STACK *sk);
OPENSSL_EXPORT void OPENSSL_sk_pop_free_ex(
    OPENSSL_STACK *sk, OPENSSL_sk_call_free_func call_free_func,
    OPENSSL_sk_free_func free_func);
OPENSSL_EXPORT size_t OPENSSL_sk_insert(OPENSSL_STACK *sk, void *p,
                                        size_t where);
OPENSSL_EXPORT void *OPENSSL_sk_delete(OPENSSL_STACK *sk, size_t where);
OPENSSL_EXPORT void *OPENSSL_sk_delete_ptr(OPENSSL_STACK *sk, const void *p);
OPENSSL_EXPORT void OPENSSL_sk_delete_if(
    OPENSSL_STACK *sk, OPENSSL_sk_call_delete_if_func call_func,
    OPENSSL_sk_delete_if_func func, void *data);
OPENSSL_EXPORT int OPENSSL_sk_find(const OPENSSL_STACK *sk, size_t *out_index,
                                   const void *p,
                                   OPENSSL_sk_call_cmp_func call_cmp_func);
OPENSSL_EXPORT void *OPENSSL_sk_shift(OPENSSL_STACK *sk);
OPENSSL_EXPORT size_t OPENSSL_sk_push(OPENSSL_STACK *sk, void *p);
OPENSSL_EXPORT void *OPENSSL_sk_pop(OPENSSL_STACK *sk);
OPENSSL_EXPORT OPENSSL_STACK *OPENSSL_sk_dup(const OPENSSL_STACK *sk);
OPENSSL_EXPORT void OPENSSL_sk_sort(OPENSSL_STACK *sk,
                                    OPENSSL_sk_call_cmp_func call_cmp_func);
OPENSSL_EXPORT int OPENSSL_sk_is_sorted(const OPENSSL_STACK *sk);
OPENSSL_EXPORT OPENSSL_sk_cmp_func
OPENSSL_sk_set_cmp_func(OPENSSL_STACK *sk, OPENSSL_sk_cmp_func comp);
OPENSSL_EXPORT OPENSSL_STACK *OPENSSL_sk_deep_copy(
    const OPENSSL_STACK *sk, OPENSSL_sk_call_copy_func call_copy_func,
    OPENSSL_sk_copy_func copy_func, OPENSSL_sk_call_free_func call_free_func,
    OPENSSL_sk_free_func free_func);

typedef OPENSSL_STACK _STACK;

OPENSSL_EXPORT OPENSSL_DEPRECATED OPENSSL_STACK *sk_new_null(void);
OPENSSL_EXPORT OPENSSL_DEPRECATED size_t sk_num(const OPENSSL_STACK *sk);
OPENSSL_EXPORT OPENSSL_DEPRECATED void *sk_value(const OPENSSL_STACK *sk,
                                                 size_t i);
OPENSSL_EXPORT OPENSSL_DEPRECATED void sk_free(OPENSSL_STACK *sk);
OPENSSL_EXPORT OPENSSL_DEPRECATED size_t sk_push(OPENSSL_STACK *sk, void *p);
OPENSSL_EXPORT OPENSSL_DEPRECATED void *sk_pop(OPENSSL_STACK *sk);

OPENSSL_EXPORT OPENSSL_DEPRECATED void sk_pop_free_ex(
    OPENSSL_STACK *sk, OPENSSL_sk_call_free_func call_free_func,
    OPENSSL_sk_free_func free_func);

OPENSSL_EXPORT OPENSSL_DEPRECATED void sk_pop_free(
    OPENSSL_STACK *sk, OPENSSL_sk_free_func free_func);

#if !defined(BORINGSSL_NO_CXX)
extern "C++" {
BSSL_NAMESPACE_BEGIN
namespace internal {
template <typename T>
struct StackTraits {};
}
BSSL_NAMESPACE_END
}

#define BORINGSSL_DEFINE_STACK_TRAITS(name, type, is_const) \
  extern "C++" {                                            \
  BSSL_NAMESPACE_BEGIN                                      \
  namespace internal {                                      \
  template <>                                               \
  struct StackTraits<STACK_OF(name)> {                      \
    static constexpr bool kIsStack = true;                  \
    using Type = type;                                      \
    static constexpr bool kIsConst = is_const;              \
  };                                                        \
  }                                                         \
  BSSL_NAMESPACE_END                                        \
  }

#else
#define BORINGSSL_DEFINE_STACK_TRAITS(name, type, is_const)
#endif

#define BORINGSSL_DEFINE_STACK_OF_IMPL(name, ptrtype, constptrtype)            \
                                                                                

                                                                               \
  OPENSSL_MSVC_PRAGMA(warning(push))                                           \
  OPENSSL_MSVC_PRAGMA(warning(disable : 4191))                                 \
  OPENSSL_CLANG_PRAGMA("clang diagnostic push")                                \
  OPENSSL_CLANG_PRAGMA("clang diagnostic ignored \"-Wunknown-warning-option\"") \
  OPENSSL_CLANG_PRAGMA("clang diagnostic ignored \"-Wcast-function-type-strict\"") \
                                                                               \
  DECLARE_STACK_OF(name)                                                       \
                                                                               \
  typedef void (*sk_##name##_free_func)(ptrtype);                              \
  typedef ptrtype (*sk_##name##_copy_func)(constptrtype);                      \
  typedef int (*sk_##name##_cmp_func)(constptrtype const *,                    \
                                      constptrtype const *);                   \
  typedef int (*sk_##name##_delete_if_func)(ptrtype, void *);                  \
                                                                               \
  OPENSSL_INLINE void sk_##name##_call_free_func(                              \
      OPENSSL_sk_free_func free_func, void *ptr) {                             \
    ((sk_##name##_free_func)free_func)((ptrtype)ptr);                          \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE void *sk_##name##_call_copy_func(                             \
      OPENSSL_sk_copy_func copy_func, const void *ptr) {                       \
    return (void *)((sk_##name##_copy_func)copy_func)((constptrtype)ptr);      \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE int sk_##name##_call_cmp_func(OPENSSL_sk_cmp_func cmp_func,   \
                                               const void *a, const void *b) { \
    constptrtype a_ptr = (constptrtype)a;                                      \
    constptrtype b_ptr = (constptrtype)b;                                      \
                                                                               \
    return ((sk_##name##_cmp_func)cmp_func)(&a_ptr, &b_ptr);                   \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE int sk_##name##_call_delete_if_func(                          \
      OPENSSL_sk_delete_if_func func, void *obj, void *data) {                 \
    return ((sk_##name##_delete_if_func)func)((ptrtype)obj, data);             \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE STACK_OF(name) *sk_##name##_new(sk_##name##_cmp_func comp) {  \
    return (STACK_OF(name) *)OPENSSL_sk_new((OPENSSL_sk_cmp_func)comp);        \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE STACK_OF(name) *sk_##name##_new_null(void) {                  \
    return (STACK_OF(name) *)OPENSSL_sk_new_null();                            \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE size_t sk_##name##_num(const STACK_OF(name) *sk) {            \
    return OPENSSL_sk_num((const OPENSSL_STACK *)sk);                          \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE void sk_##name##_zero(STACK_OF(name) *sk) {                   \
    OPENSSL_sk_zero((OPENSSL_STACK *)sk);                                      \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE ptrtype sk_##name##_value(const STACK_OF(name) *sk,           \
                                           size_t i) {                         \
    return (ptrtype)OPENSSL_sk_value((const OPENSSL_STACK *)sk, i);            \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE ptrtype sk_##name##_set(STACK_OF(name) *sk, size_t i,         \
                                         ptrtype p) {                          \
    return (ptrtype)OPENSSL_sk_set((OPENSSL_STACK *)sk, i, (void *)p);         \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE void sk_##name##_free(STACK_OF(name) *sk) {                   \
    OPENSSL_sk_free((OPENSSL_STACK *)sk);                                      \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE void sk_##name##_pop_free(STACK_OF(name) *sk,                 \
                                           sk_##name##_free_func free_func) {  \
    OPENSSL_sk_pop_free_ex((OPENSSL_STACK *)sk, sk_##name##_call_free_func,    \
                           (OPENSSL_sk_free_func)free_func);                   \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE size_t sk_##name##_insert(STACK_OF(name) *sk, ptrtype p,      \
                                           size_t where) {                     \
    return OPENSSL_sk_insert((OPENSSL_STACK *)sk, (void *)p, where);           \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE ptrtype sk_##name##_delete(STACK_OF(name) *sk,                \
                                            size_t where) {                    \
    return (ptrtype)OPENSSL_sk_delete((OPENSSL_STACK *)sk, where);             \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE ptrtype sk_##name##_delete_ptr(STACK_OF(name) *sk,            \
                                                constptrtype p) {              \
    return (ptrtype)OPENSSL_sk_delete_ptr((OPENSSL_STACK *)sk,                 \
                                          (const void *)p);                    \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE void sk_##name##_delete_if(                                   \
      STACK_OF(name) *sk, sk_##name##_delete_if_func func, void *data) {       \
    OPENSSL_sk_delete_if((OPENSSL_STACK *)sk, sk_##name##_call_delete_if_func, \
                         (OPENSSL_sk_delete_if_func)func, data);               \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE int sk_##name##_find(const STACK_OF(name) *sk,                \
                                      size_t *out_index, constptrtype p) {     \
    return OPENSSL_sk_find((const OPENSSL_STACK *)sk, out_index,               \
                           (const void *)p, sk_##name##_call_cmp_func);        \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE ptrtype sk_##name##_shift(STACK_OF(name) *sk) {               \
    return (ptrtype)OPENSSL_sk_shift((OPENSSL_STACK *)sk);                     \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE size_t sk_##name##_push(STACK_OF(name) *sk, ptrtype p) {      \
    return OPENSSL_sk_push((OPENSSL_STACK *)sk, (void *)p);                    \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE ptrtype sk_##name##_pop(STACK_OF(name) *sk) {                 \
    return (ptrtype)OPENSSL_sk_pop((OPENSSL_STACK *)sk);                       \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE STACK_OF(name) *sk_##name##_dup(const STACK_OF(name) *sk) {   \
    return (STACK_OF(name) *)OPENSSL_sk_dup((const OPENSSL_STACK *)sk);        \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE void sk_##name##_sort(STACK_OF(name) *sk) {                   \
    OPENSSL_sk_sort((OPENSSL_STACK *)sk, sk_##name##_call_cmp_func);           \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE int sk_##name##_is_sorted(const STACK_OF(name) *sk) {         \
    return OPENSSL_sk_is_sorted((const OPENSSL_STACK *)sk);                    \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE sk_##name##_cmp_func sk_##name##_set_cmp_func(                \
      STACK_OF(name) *sk, sk_##name##_cmp_func comp) {                         \
    return (sk_##name##_cmp_func)OPENSSL_sk_set_cmp_func(                      \
        (OPENSSL_STACK *)sk, (OPENSSL_sk_cmp_func)comp);                       \
  }                                                                            \
                                                                               \
  OPENSSL_INLINE STACK_OF(name) *sk_##name##_deep_copy(                        \
      const STACK_OF(name) *sk, sk_##name##_copy_func copy_func,               \
      sk_##name##_free_func free_func) {                                       \
    return (STACK_OF(name) *)OPENSSL_sk_deep_copy(                             \
        (const OPENSSL_STACK *)sk, sk_##name##_call_copy_func,                 \
        (OPENSSL_sk_copy_func)copy_func, sk_##name##_call_free_func,           \
        (OPENSSL_sk_free_func)free_func);                                      \
  }                                                                            \
                                                                               \
  OPENSSL_CLANG_PRAGMA("clang diagnostic pop")                                 \
  OPENSSL_MSVC_PRAGMA(warning(pop))

typedef char *OPENSSL_STRING;

DEFINE_STACK_OF(void)
DEFINE_NAMED_STACK_OF(OPENSSL_STRING, char)

#if defined(__cplusplus)
}
#endif

#if !defined(BORINGSSL_NO_CXX)
extern "C++" {

#include <type_traits>

BSSL_NAMESPACE_BEGIN

namespace internal {

template <typename Stack>
struct DeleterImpl<Stack, std::enable_if_t<StackTraits<Stack>::kIsConst>> {
  static void Free(Stack *sk) {
    OPENSSL_sk_free(reinterpret_cast<OPENSSL_STACK *>(sk));
  }
};

template <typename Stack>
struct DeleterImpl<Stack, std::enable_if_t<!StackTraits<Stack>::kIsConst>> {
  static void Free(Stack *sk) {

    using Type = typename StackTraits<Stack>::Type;
    OPENSSL_sk_pop_free_ex(
        reinterpret_cast<OPENSSL_STACK *>(sk),
        [](OPENSSL_sk_free_func             , void *ptr) {
          DeleterImpl<Type>::Free(reinterpret_cast<Type *>(ptr));
        },
        nullptr);
  }
};

template <typename Stack>
class StackIteratorImpl {
 public:
  using Type = typename StackTraits<Stack>::Type;

  StackIteratorImpl() : sk_(nullptr), idx_(0) {}
  StackIteratorImpl(const Stack *sk, size_t idx) : sk_(sk), idx_(idx) {}

  bool operator==(StackIteratorImpl other) const {
    return sk_ == other.sk_ && idx_ == other.idx_;
  }
  bool operator!=(StackIteratorImpl other) const {
    return !(*this == other);
  }

  Type *operator*() const {
    return reinterpret_cast<Type *>(
        OPENSSL_sk_value(reinterpret_cast<const OPENSSL_STACK *>(sk_), idx_));
  }

  StackIteratorImpl &operator++(            ) {
    idx_++;
    return *this;
  }

  StackIteratorImpl operator++(int              ) {
    StackIteratorImpl copy(*this);
    ++(*this);
    return copy;
  }

 private:
  const Stack *sk_;
  size_t idx_;
};

template <typename Stack>
using StackIterator =
    std::enable_if_t<StackTraits<Stack>::kIsStack, StackIteratorImpl<Stack>>;

}

template <typename Stack>
inline std::enable_if_t<!internal::StackTraits<Stack>::kIsConst, bool>
PushToStack(Stack *sk,
            UniquePtr<typename internal::StackTraits<Stack>::Type> elem) {
  if (!OPENSSL_sk_push(reinterpret_cast<OPENSSL_STACK *>(sk), elem.get())) {
    return false;
  }

  elem.release();
  return true;
}

BSSL_NAMESPACE_END

template <typename Stack>
inline bssl::internal::StackIterator<Stack> begin(const Stack *sk) {
  return bssl::internal::StackIterator<Stack>(sk, 0);
}

template <typename Stack>
inline bssl::internal::StackIterator<Stack> end(const Stack *sk) {
  return bssl::internal::StackIterator<Stack>(
      sk, OPENSSL_sk_num(reinterpret_cast<const OPENSSL_STACK *>(sk)));
}

}
#endif

#endif
