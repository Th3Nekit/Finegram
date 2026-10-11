// Copyright 2018 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_TEST_ABI_TEST_H
#define OPENSSL_HEADER_CRYPTO_TEST_ABI_TEST_H

#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <vector>

#include <openssl/base.h>

#include "../internal.h"

namespace abi_test {

struct Result {
  bool ok() const { return errors.empty(); }

  std::vector<std::string> errors;
};

namespace internal {

template <typename T>
struct DeductionGuard {
  using Type = T;
};

struct alignas(16) Reg128 {
  bool operator==(const Reg128 &x) const { return x.lo == lo && x.hi == hi; }
  bool operator!=(const Reg128 &x) const { return !((*this) == x); }
  uint64_t lo, hi;
};

#if defined(OPENSSL_X86_64)

#if defined(OPENSSL_WINDOWS)
#define LOOP_CALLER_STATE_REGISTERS()  \
  CALLER_STATE_REGISTER(uint64_t, rbx) \
  CALLER_STATE_REGISTER(uint64_t, rbp) \
  CALLER_STATE_REGISTER(uint64_t, rdi) \
  CALLER_STATE_REGISTER(uint64_t, rsi) \
  CALLER_STATE_REGISTER(uint64_t, r12) \
  CALLER_STATE_REGISTER(uint64_t, r13) \
  CALLER_STATE_REGISTER(uint64_t, r14) \
  CALLER_STATE_REGISTER(uint64_t, r15) \
  CALLER_STATE_REGISTER(Reg128, xmm6)  \
  CALLER_STATE_REGISTER(Reg128, xmm7)  \
  CALLER_STATE_REGISTER(Reg128, xmm8)  \
  CALLER_STATE_REGISTER(Reg128, xmm9)  \
  CALLER_STATE_REGISTER(Reg128, xmm10) \
  CALLER_STATE_REGISTER(Reg128, xmm11) \
  CALLER_STATE_REGISTER(Reg128, xmm12) \
  CALLER_STATE_REGISTER(Reg128, xmm13) \
  CALLER_STATE_REGISTER(Reg128, xmm14) \
  CALLER_STATE_REGISTER(Reg128, xmm15)
#else
#define LOOP_CALLER_STATE_REGISTERS()  \
  CALLER_STATE_REGISTER(uint64_t, rbx) \
  CALLER_STATE_REGISTER(uint64_t, rbp) \
  CALLER_STATE_REGISTER(uint64_t, r12) \
  CALLER_STATE_REGISTER(uint64_t, r13) \
  CALLER_STATE_REGISTER(uint64_t, r14) \
  CALLER_STATE_REGISTER(uint64_t, r15)
#endif

#elif defined(OPENSSL_X86)

#define LOOP_CALLER_STATE_REGISTERS()  \
  CALLER_STATE_REGISTER(uint32_t, esi) \
  CALLER_STATE_REGISTER(uint32_t, edi) \
  CALLER_STATE_REGISTER(uint32_t, ebx) \
  CALLER_STATE_REGISTER(uint32_t, ebp)

#elif defined(OPENSSL_ARM)

#define LOOP_CALLER_STATE_REGISTERS_PRE_R9() \
  CALLER_STATE_REGISTER(uint64_t, d8)        \
  CALLER_STATE_REGISTER(uint64_t, d9)        \
  CALLER_STATE_REGISTER(uint64_t, d10)       \
  CALLER_STATE_REGISTER(uint64_t, d11)       \
  CALLER_STATE_REGISTER(uint64_t, d12)       \
  CALLER_STATE_REGISTER(uint64_t, d13)       \
  CALLER_STATE_REGISTER(uint64_t, d14)       \
  CALLER_STATE_REGISTER(uint64_t, d15)       \
  CALLER_STATE_REGISTER(uint32_t, r4)        \
  CALLER_STATE_REGISTER(uint32_t, r5)        \
  CALLER_STATE_REGISTER(uint32_t, r6)        \
  CALLER_STATE_REGISTER(uint32_t, r7)        \
  CALLER_STATE_REGISTER(uint32_t, r8)
#define LOOP_CALLER_STATE_REGISTERS_POST_R9() \
  CALLER_STATE_REGISTER(uint32_t, r10)        \
  CALLER_STATE_REGISTER(uint32_t, r11)
#if defined(OPENSSL_APPLE)
#define LOOP_CALLER_STATE_REGISTERS()  \
  LOOP_CALLER_STATE_REGISTERS_PRE_R9() \
  LOOP_CALLER_STATE_REGISTERS_POST_R9()
#else
#define LOOP_CALLER_STATE_REGISTERS()  \
  LOOP_CALLER_STATE_REGISTERS_PRE_R9() \
  CALLER_STATE_REGISTER(uint32_t, r9)  \
  LOOP_CALLER_STATE_REGISTERS_POST_R9()
#endif

#elif defined(OPENSSL_AARCH64)

#define LOOP_CALLER_STATE_REGISTERS()                                \
                                                                      \
                                                                     \
  CALLER_STATE_REGISTER(uint64_t, d8)                                \
  CALLER_STATE_REGISTER(uint64_t, d9)                                \
  CALLER_STATE_REGISTER(uint64_t, d10)                               \
  CALLER_STATE_REGISTER(uint64_t, d11)                               \
  CALLER_STATE_REGISTER(uint64_t, d12)                               \
  CALLER_STATE_REGISTER(uint64_t, d13)                               \
  CALLER_STATE_REGISTER(uint64_t, d14)                               \
  CALLER_STATE_REGISTER(uint64_t, d15)                               \
                                                                     \
                                                                     \
  CALLER_STATE_REGISTER(uint64_t, x19)                               \
  CALLER_STATE_REGISTER(uint64_t, x20)                               \
  CALLER_STATE_REGISTER(uint64_t, x21)                               \
  CALLER_STATE_REGISTER(uint64_t, x22)                               \
  CALLER_STATE_REGISTER(uint64_t, x23)                               \
  CALLER_STATE_REGISTER(uint64_t, x24)                               \
  CALLER_STATE_REGISTER(uint64_t, x25)                               \
  CALLER_STATE_REGISTER(uint64_t, x26)                               \
  CALLER_STATE_REGISTER(uint64_t, x27)                               \
  CALLER_STATE_REGISTER(uint64_t, x28)                               \
  CALLER_STATE_REGISTER(uint64_t, x29)

#endif

#if defined(LOOP_CALLER_STATE_REGISTERS) && !defined(OPENSSL_NO_ASM) && \
    !defined(BORINGSSL_SHARED_LIBRARY)
#define SUPPORTS_ABI_TEST

struct CallerState {
#define CALLER_STATE_REGISTER(type, name) type name;
  LOOP_CALLER_STATE_REGISTERS()
#undef CALLER_STATE_REGISTER
};

crypto_word_t RunTrampoline(Result *out, crypto_word_t func,
                            const crypto_word_t *argv, size_t argc,
                            bool unwind);

template <typename T>
inline crypto_word_t ToWord(T t) {

  static_assert(std::is_integral<T>::value || std::is_pointer<T>::value,
                "parameter types must be integral or pointer types");

  static_assert(sizeof(T) <= sizeof(crypto_word_t),
                "parameter types must be at most word-sized");

#if defined(OPENSSL_32_BIT)

  static_assert(sizeof(T) == 4, "parameter types must be word-sized");
  return (crypto_word_t)t;
#elif defined(OPENSSL_X86_64) || defined(OPENSSL_AARCH64)

  static_assert(sizeof(T) >= 4, "parameters must be at least 32 bits wide");
  crypto_word_t ret;

  OPENSSL_memset(&ret, 0xaa, sizeof(ret));
  OPENSSL_memcpy(&ret, &t, sizeof(t));
  return ret;
#else
#error "unknown architecture"
#endif
}

template <typename R, typename... Args>
inline crypto_word_t CheckImpl(Result *out, bool unwind, R (*func)(Args...),
                               typename DeductionGuard<Args>::Type... args) {

  static_assert(sizeof...(args) <= 8,
                "too many arguments for abi_test_trampoline");

  crypto_word_t argv[sizeof...(args) + 1] = {
      ToWord(args)...,
  };
  return RunTrampoline(out, reinterpret_cast<crypto_word_t>(func), argv,
                       sizeof...(args), unwind);
}
#else

template <typename R, typename... Args>
inline std::enable_if_t<!std::is_void<R>::value, crypto_word_t> CheckImpl(
    Result *out, bool             , R (*func)(Args...),
    typename DeductionGuard<Args>::Type... args) {
  *out = Result();
  return func(args...);
}

template <typename... Args>
inline crypto_word_t CheckImpl(Result *out, bool             ,
                               void (*func)(Args...),
                               typename DeductionGuard<Args>::Type... args) {
  *out = Result();
  func(args...);
  return 0;
}
#endif

std::string FixVAArgsString(const char *str);

template <typename R, typename... Args>
inline R CheckGTest(const char *va_args_str, const char *file, int line,
                    bool unwind, R (*func)(Args...),
                    typename DeductionGuard<Args>::Type... args) {
  Result result;
  crypto_word_t ret = CheckImpl(&result, unwind, func, args...);
  if (!result.ok()) {
    testing::Message msg;
    msg << "ABI failures in " << FixVAArgsString(va_args_str) << ":\n";
    for (const auto &error : result.errors) {
      msg << "    " << error << "\n";
    }
    ADD_FAILURE_AT(file, line) << msg;
  }
  return (R)ret;
}

}

template <typename R, typename... Args>
inline R Check(Result *out, R (*func)(Args...),
               typename internal::DeductionGuard<Args>::Type... args) {
  return (R)internal::CheckImpl(out, false, func, args...);
}

void EnableUnwindTests();

bool UnwindTestsEnabled();

}

#if defined(OPENSSL_WINDOWS)
#define CHECK_ABI(...) CHECK_ABI_NO_UNWIND(__VA_ARGS__)
#else
#define CHECK_ABI(...) CHECK_ABI_SEH(__VA_ARGS__)
#endif

#define CHECK_ABI_SEH(...)                                               \
  abi_test::internal::CheckGTest(#__VA_ARGS__, __FILE__, __LINE__, true, \
                                 __VA_ARGS__)

#define CHECK_ABI_NO_UNWIND(...)                                          \
  abi_test::internal::CheckGTest(#__VA_ARGS__, __FILE__, __LINE__, false, \
                                 __VA_ARGS__)

#if defined(SUPPORTS_ABI_TEST)
struct Uncallable {
  Uncallable() = delete;
};

extern "C" {

crypto_word_t abi_test_trampoline(crypto_word_t func,
                                  abi_test::internal::CallerState *state,
                                  const crypto_word_t *argv, size_t argc,
                                  crypto_word_t unwind);

#if defined(OPENSSL_X86_64)

void abi_test_unwind_start(Uncallable);

void abi_test_unwind_return(Uncallable);

void abi_test_unwind_stop(Uncallable);

void abi_test_bad_unwind_wrong_register(void);

void abi_test_bad_unwind_temporary(void);

#if defined(OPENSSL_WINDOWS)

void abi_test_bad_unwind_epilog(void);
#endif
#endif

#if defined(OPENSSL_X86_64) || defined(OPENSSL_X86)

int abi_test_get_and_clear_direction_flag(void);

int abi_test_set_direction_flag(void);
#endif

}
#endif

#endif
