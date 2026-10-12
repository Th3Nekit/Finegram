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

#ifndef OPENSSL_HEADER_TARGET_H
#define OPENSSL_HEADER_TARGET_H

#if defined(__x86_64) || defined(_M_AMD64) || defined(_M_X64)
#define OPENSSL_64_BIT
#define OPENSSL_X86_64
#elif defined(__x86) || defined(__i386) || defined(__i386__) || defined(_M_IX86)
#define OPENSSL_32_BIT
#define OPENSSL_X86
#elif defined(__AARCH64EL__) || defined(_M_ARM64)
#define OPENSSL_64_BIT
#define OPENSSL_AARCH64
#elif defined(__ARMEL__) || defined(_M_ARM)
#define OPENSSL_32_BIT
#define OPENSSL_ARM
#elif defined(__MIPSEL__) && !defined(__LP64__)
#define OPENSSL_32_BIT
#define OPENSSL_MIPS
#elif defined(__MIPSEL__) && defined(__LP64__)
#define OPENSSL_64_BIT
#define OPENSSL_MIPS64
#elif defined(__riscv) && __SIZEOF_POINTER__ == 8
#define OPENSSL_64_BIT
#define OPENSSL_RISCV64
#elif defined(__riscv) && __SIZEOF_POINTER__ == 4
#define OPENSSL_32_BIT
#elif defined(__pnacl__)
#define OPENSSL_32_BIT
#define OPENSSL_PNACL
#elif defined(__wasm__)
#define OPENSSL_32_BIT
#elif defined(__asmjs__)
#define OPENSSL_32_BIT
#elif defined(__myriad2__)
#define OPENSSL_32_BIT
#else

#error "Unknown target CPU"
#endif

#if defined(__APPLE__)
#define OPENSSL_APPLE
#endif

#if defined(_WIN32)
#define OPENSSL_WINDOWS
#endif

#if defined(__linux__) && !defined(__TRUSTY__) && \
    !defined(ANDROID_BAREMETAL) && !defined(OPENSSL_NANOLIBC) && \
    !defined(CROS_EC)
#define OPENSSL_LINUX
#endif

#if defined(__Fuchsia__)
#define OPENSSL_FUCHSIA
#endif

#if defined(__TRUSTY__)
#define OPENSSL_TRUSTY
#define OPENSSL_NO_FILESYSTEM
#define OPENSSL_NO_POSIX_IO
#define OPENSSL_NO_SOCK
#define OPENSSL_NO_THREADS_CORRUPT_MEMORY_AND_LEAK_SECRETS_IF_THREADED
#endif

#if defined(OPENSSL_NANOLIBC)
#define OPENSSL_NO_FILESYSTEM
#define OPENSSL_NO_POSIX_IO
#define OPENSSL_NO_SOCK
#define OPENSSL_NO_THREADS_CORRUPT_MEMORY_AND_LEAK_SECRETS_IF_THREADED
#endif

#if defined(ANDROID_BAREMETAL)
#define OPENSSL_NO_FILESYSTEM
#define OPENSSL_NO_POSIX_IO
#define OPENSSL_NO_SOCK
#define OPENSSL_NO_THREADS_CORRUPT_MEMORY_AND_LEAK_SECRETS_IF_THREADED
#endif

#if defined(CROS_EC)
#define OPENSSL_NO_FILESYSTEM
#define OPENSSL_NO_POSIX_IO
#define OPENSSL_NO_SOCK
#define OPENSSL_NO_THREADS_CORRUPT_MEMORY_AND_LEAK_SECRETS_IF_THREADED
#endif

#if defined(__ZEPHYR__)
#define OPENSSL_NO_FILESYSTEM
#define OPENSSL_NO_POSIX_IO
#define OPENSSL_NO_SOCK
#endif

#if defined(__ANDROID_API__)
#define OPENSSL_ANDROID
#endif

#if defined(__FreeBSD__)
#define OPENSSL_FREEBSD
#endif

#if defined(__OpenBSD__)
#define OPENSSL_OPENBSD
#endif

#if !defined(OPENSSL_NO_THREADS_CORRUPT_MEMORY_AND_LEAK_SECRETS_IF_THREADED)
#define OPENSSL_THREADS
#endif

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define OPENSSL_ASAN
#endif
#if __has_feature(thread_sanitizer)
#define OPENSSL_TSAN
#endif
#if __has_feature(memory_sanitizer)
#define OPENSSL_MSAN
#define OPENSSL_ASM_INCOMPATIBLE
#endif
#if __has_feature(hwaddress_sanitizer)
#define OPENSSL_HWASAN
#endif
#endif

#if defined(OPENSSL_APPLE) && defined(OPENSSL_ARM)
#define OPENSSL_ASM_INCOMPATIBLE
#endif

#if defined(OPENSSL_ASM_INCOMPATIBLE)
#undef OPENSSL_ASM_INCOMPATIBLE
#if !defined(OPENSSL_NO_ASM)
#define OPENSSL_NO_ASM
#endif
#endif

#if defined(OPENSSL_ARM) && !defined(OPENSSL_STATIC_ARMCAP) && \
    (defined(OPENSSL_APPLE) || defined(OPENSSL_OPENBSD) ||     \
     defined(ANDROID_BAREMETAL))
#define OPENSSL_STATIC_ARMCAP
#endif

#endif
