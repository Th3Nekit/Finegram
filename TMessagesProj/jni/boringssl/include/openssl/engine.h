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

#ifndef OPENSSL_HEADER_ENGINE_H
#define OPENSSL_HEADER_ENGINE_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT ENGINE *ENGINE_new(void);

OPENSSL_EXPORT int ENGINE_free(ENGINE *engine);

OPENSSL_EXPORT int ENGINE_set_RSA_method(ENGINE *engine,
                                         const RSA_METHOD *method,
                                         size_t method_size);
OPENSSL_EXPORT RSA_METHOD *ENGINE_get_RSA_method(const ENGINE *engine);

OPENSSL_EXPORT int ENGINE_set_ECDSA_method(ENGINE *engine,
                                           const ECDSA_METHOD *method,
                                           size_t method_size);
OPENSSL_EXPORT ECDSA_METHOD *ENGINE_get_ECDSA_method(const ENGINE *engine);

void METHOD_ref(void *method);

void METHOD_unref(void *method);

struct openssl_method_common_st {
  int references;
  char is_static;
};

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(ENGINE, ENGINE_free)

BSSL_NAMESPACE_END

}

#endif

#define ENGINE_R_OPERATION_NOT_SUPPORTED 100

#endif
