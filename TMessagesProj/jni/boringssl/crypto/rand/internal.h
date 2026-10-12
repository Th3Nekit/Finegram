// Copyright 2024 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_RAND_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_RAND_INTERNAL_H

#include <openssl/base.h>

#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
#define OPENSSL_RAND_DETERMINISTIC
#elif defined(OPENSSL_TRUSTY)
#define OPENSSL_RAND_TRUSTY
#elif defined(OPENSSL_WINDOWS)
#define OPENSSL_RAND_WINDOWS
#elif defined(OPENSSL_LINUX)
#define OPENSSL_RAND_URANDOM
#elif defined(OPENSSL_APPLE) && !defined(OPENSSL_MACOS)

#define OPENSSL_RAND_IOS
#else

#define OPENSSL_RAND_GETENTROPY
#endif

#if defined(OPENSSL_LINUX)

#define OPENSSL_FORK_DETECTION
#define OPENSSL_FORK_DETECTION_MADVISE
#elif defined(OPENSSL_MACOS) || defined(OPENSSL_IOS) || \
    defined(OPENSSL_OPENBSD) || defined(OPENSSL_FREEBSD)

#define OPENSSL_FORK_DETECTION
#define OPENSSL_FORK_DETECTION_PTHREAD_ATFORK
#elif defined(OPENSSL_WINDOWS) || defined(OPENSSL_TRUSTY) || \
    defined(__ZEPHYR__) || defined(CROS_EC)

#define OPENSSL_DOES_NOT_FORK
#endif

#endif
