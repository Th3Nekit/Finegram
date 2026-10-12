// Copyright 2022 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_POSIX_TIME_H
#define OPENSSL_HEADER_POSIX_TIME_H

#include <openssl/base.h>

#include <time.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT int OPENSSL_posix_to_tm(int64_t time, struct tm *out_tm);

OPENSSL_EXPORT int OPENSSL_tm_to_posix(const struct tm *tm, int64_t *out);

OPENSSL_EXPORT int OPENSSL_timegm(const struct tm *tm, time_t *out);

#if defined(__cplusplus)
}
#endif

#endif
