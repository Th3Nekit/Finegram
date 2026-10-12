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

#ifndef OPENSSL_HEADER_CONF_H
#define OPENSSL_HEADER_CONF_H

#include <openssl/base.h>

#include <openssl/stack.h>
#include <openssl/lhash.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct conf_value_st {
  char *section;
  char *name;
  char *value;
};

DEFINE_STACK_OF(CONF_VALUE)
DECLARE_LHASH_OF(CONF_VALUE)

OPENSSL_EXPORT CONF *NCONF_new(void *method);

OPENSSL_EXPORT void NCONF_free(CONF *conf);

OPENSSL_EXPORT int NCONF_load(CONF *conf, const char *filename,
                              long *out_error_line);

OPENSSL_EXPORT int NCONF_load_bio(CONF *conf, BIO *bio, long *out_error_line);

OPENSSL_EXPORT const STACK_OF(CONF_VALUE) *NCONF_get_section(
    const CONF *conf, const char *section);

OPENSSL_EXPORT const char *NCONF_get_string(const CONF *conf,
                                            const char *section,
                                            const char *name);

#define CONF_MFLAGS_DEFAULT_SECTION 0
#define CONF_MFLAGS_IGNORE_MISSING_FILE 0

OPENSSL_EXPORT int CONF_modules_load_file(const char *filename,
                                          const char *appname,
                                          unsigned long flags);

OPENSSL_EXPORT void CONF_modules_free(void);

OPENSSL_EXPORT void OPENSSL_config(const char *config_name);

OPENSSL_EXPORT void OPENSSL_no_config(void);

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(CONF, NCONF_free)

BSSL_NAMESPACE_END

}

#endif

#define CONF_R_LIST_CANNOT_BE_NULL 100
#define CONF_R_MISSING_CLOSE_SQUARE_BRACKET 101
#define CONF_R_MISSING_EQUAL_SIGN 102
#define CONF_R_NO_CLOSE_BRACE 103
#define CONF_R_UNABLE_TO_CREATE_NEW_SECTION 104
#define CONF_R_VARIABLE_HAS_NO_VALUE 105
#define CONF_R_VARIABLE_EXPANSION_TOO_LONG 106
#define CONF_R_VARIABLE_EXPANSION_NOT_SUPPORTED 107

#endif
