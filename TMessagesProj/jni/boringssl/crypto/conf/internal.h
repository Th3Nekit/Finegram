// Copyright 2015 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_CONF_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_CONF_INTERNAL_H

#include <openssl/base.h>

#include "../lhash/internal.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct conf_section_st CONF_SECTION;

DEFINE_LHASH_OF(CONF_SECTION)
DEFINE_LHASH_OF(CONF_VALUE)

struct conf_st {
  LHASH_OF(CONF_VALUE) *values;
  LHASH_OF(CONF_SECTION) *sections;
};

CONF_VALUE *CONF_VALUE_new(void);

OPENSSL_EXPORT int CONF_parse_list(
    const char *list, char sep, int remove_whitespace,
    int (*list_cb)(const char *elem, size_t len, void *usr), void *arg);

#if defined(__cplusplus)
}
#endif

#endif
