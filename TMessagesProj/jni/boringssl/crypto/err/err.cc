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

// Ensure we can't call OPENSSL_malloc circularly.
#define _BORINGSSL_PROHIBIT_OPENSSL_MALLOC
#include <openssl/err.h>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>

#if defined(OPENSSL_WINDOWS)
#include <windows.h>
#endif

#include <openssl/mem.h>
#include <openssl/thread.h>

#include "../internal.h"
#include "./internal.h"

namespace {
struct err_error_st {

  const char *file;

  char *data;

  uint32_t packed;

  uint16_t line;

  unsigned mark : 1;
};

typedef struct err_state_st {

  struct err_error_st errors[ERR_NUM_ERRORS];

  unsigned top;

  unsigned bottom;

  void *to_free;
} ERR_STATE;
}

extern const uint32_t kOpenSSLReasonValues[];
extern const size_t kOpenSSLReasonValuesLen;
extern const char kOpenSSLReasonStringData[];

static char *strdup_libc_malloc(const char *str) {

  size_t len = strlen(str);
  char *ret = reinterpret_cast<char *>(malloc(len + 1));
  if (ret != NULL) {
    memcpy(ret, str, len + 1);
  }
  return ret;
}

static void err_clear(struct err_error_st *error) {
  free(error->data);
  OPENSSL_memset(error, 0, sizeof(struct err_error_st));
}

static void err_copy(struct err_error_st *dst, const struct err_error_st *src) {
  err_clear(dst);
  dst->file = src->file;
  if (src->data != NULL) {

    dst->data = strdup_libc_malloc(src->data);
  }
  dst->packed = src->packed;
  dst->line = src->line;
}

static int global_next_library = ERR_NUM_LIBS;

static CRYPTO_MUTEX global_next_library_mutex = CRYPTO_MUTEX_INIT;

static void err_state_free(void *statep) {
  ERR_STATE *state = reinterpret_cast<ERR_STATE *>(statep);

  if (state == NULL) {
    return;
  }

  for (unsigned i = 0; i < ERR_NUM_ERRORS; i++) {
    err_clear(&state->errors[i]);
  }
  free(state->to_free);
  free(state);
}

static ERR_STATE *err_get_state(void) {
  ERR_STATE *state = reinterpret_cast<ERR_STATE *>(
      CRYPTO_get_thread_local(OPENSSL_THREAD_LOCAL_ERR));
  if (state == NULL) {
    state = reinterpret_cast<ERR_STATE *>(malloc(sizeof(ERR_STATE)));
    if (state == NULL) {
      return NULL;
    }
    OPENSSL_memset(state, 0, sizeof(ERR_STATE));
    if (!CRYPTO_set_thread_local(OPENSSL_THREAD_LOCAL_ERR, state,
                                 err_state_free)) {
      return NULL;
    }
  }

  return state;
}

static uint32_t get_error_values(int inc, int top, const char **file, int *line,
                                 const char **data, int *flags) {
  unsigned i = 0;
  ERR_STATE *state;
  struct err_error_st *error;
  uint32_t ret;

  state = err_get_state();
  if (state == NULL || state->bottom == state->top) {
    return 0;
  }

  if (top) {
    assert(!inc);

    i = state->top;
  } else {
    i = (state->bottom + 1) % ERR_NUM_ERRORS;
  }

  error = &state->errors[i];
  ret = error->packed;

  if (file != NULL && line != NULL) {
    if (error->file == NULL) {
      *file = "NA";
      *line = 0;
    } else {
      *file = error->file;
      *line = error->line;
    }
  }

  if (data != NULL) {
    if (error->data == NULL) {
      *data = "";
      if (flags != NULL) {
        *flags = 0;
      }
    } else {
      *data = error->data;
      if (flags != NULL) {

        *flags = ERR_FLAG_STRING | ERR_FLAG_MALLOCED;
      }

      if (inc) {
        if (error->data != NULL) {
          free(state->to_free);
          state->to_free = error->data;
        }
        error->data = NULL;
      }
    }
  }

  if (inc) {
    assert(!top);
    err_clear(error);
    state->bottom = i;
  }

  return ret;
}

uint32_t ERR_get_error(void) {
  return get_error_values(1          , 0             , NULL, NULL, NULL, NULL);
}

uint32_t ERR_get_error_line(const char **file, int *line) {
  return get_error_values(1          , 0             , file, line, NULL, NULL);
}

uint32_t ERR_get_error_line_data(const char **file, int *line,
                                 const char **data, int *flags) {
  return get_error_values(1          , 0             , file, line, data, flags);
}

uint32_t ERR_peek_error(void) {
  return get_error_values(0           , 0             , NULL, NULL, NULL, NULL);
}

uint32_t ERR_peek_error_line(const char **file, int *line) {
  return get_error_values(0           , 0             , file, line, NULL, NULL);
}

uint32_t ERR_peek_error_line_data(const char **file, int *line,
                                  const char **data, int *flags) {
  return get_error_values(0           , 0             , file, line, data,
                          flags);
}

uint32_t ERR_peek_last_error(void) {
  return get_error_values(0           , 1          , NULL, NULL, NULL, NULL);
}

uint32_t ERR_peek_last_error_line(const char **file, int *line) {
  return get_error_values(0           , 1          , file, line, NULL, NULL);
}

uint32_t ERR_peek_last_error_line_data(const char **file, int *line,
                                       const char **data, int *flags) {
  return get_error_values(0           , 1          , file, line, data, flags);
}

void ERR_clear_error(void) {
  ERR_STATE *const state = err_get_state();
  unsigned i;

  if (state == NULL) {
    return;
  }

  for (i = 0; i < ERR_NUM_ERRORS; i++) {
    err_clear(&state->errors[i]);
  }
  free(state->to_free);
  state->to_free = NULL;

  state->top = state->bottom = 0;
}

void ERR_remove_thread_state(const CRYPTO_THREADID *tid) {
  if (tid != NULL) {
    assert(0);
    return;
  }

  ERR_clear_error();
}

int ERR_get_next_error_library(void) {
  int ret;

  CRYPTO_MUTEX_lock_write(&global_next_library_mutex);
  ret = global_next_library++;
  CRYPTO_MUTEX_unlock_write(&global_next_library_mutex);

  return ret;
}

void ERR_remove_state(unsigned long pid) { ERR_clear_error(); }

void ERR_clear_system_error(void) { errno = 0; }

static int err_string_cmp(const void *a, const void *b) {
  const uint32_t a_key = *((const uint32_t *)a) >> 15;
  const uint32_t b_key = *((const uint32_t *)b) >> 15;

  if (a_key < b_key) {
    return -1;
  } else if (a_key > b_key) {
    return 1;
  } else {
    return 0;
  }
}

static const char *err_string_lookup(uint32_t lib, uint32_t key,
                                     const uint32_t *values, size_t num_values,
                                     const char *string_data) {

  if (lib >= (1 << 6) || key >= (1 << 11)) {
    return NULL;
  }
  uint32_t search_key = lib << 26 | key << 15;
  const uint32_t *result = reinterpret_cast<const uint32_t *>(bsearch(
      &search_key, values, num_values, sizeof(uint32_t), err_string_cmp));
  if (result == NULL) {
    return NULL;
  }

  return &string_data[(*result) & 0x7fff];
}

namespace {
typedef struct library_name_st {
  const char *str;
  const char *symbol;
  const char *reason_symbol;
} LIBRARY_NAME;
}

static const LIBRARY_NAME kLibraryNames[ERR_NUM_LIBS] = {
    {"invalid library (0)", NULL, NULL},
    {"unknown library", "NONE", "NONE_LIB"},
    {"system library", "SYS", "SYS_LIB"},
    {"bignum routines", "BN", "BN_LIB"},
    {"RSA routines", "RSA", "RSA_LIB"},
    {"Diffie-Hellman routines", "DH", "DH_LIB"},
    {"public key routines", "EVP", "EVP_LIB"},
    {"memory buffer routines", "BUF", "BUF_LIB"},
    {"object identifier routines", "OBJ", "OBJ_LIB"},
    {"PEM routines", "PEM", "PEM_LIB"},
    {"DSA routines", "DSA", "DSA_LIB"},
    {"X.509 certificate routines", "X509", "X509_LIB"},
    {"ASN.1 encoding routines", "ASN1", "ASN1_LIB"},
    {"configuration file routines", "CONF", "CONF_LIB"},
    {"common libcrypto routines", "CRYPTO", "CRYPTO_LIB"},
    {"elliptic curve routines", "EC", "EC_LIB"},
    {"SSL routines", "SSL", "SSL_LIB"},
    {"BIO routines", "BIO", "BIO_LIB"},
    {"PKCS7 routines", "PKCS7", "PKCS7_LIB"},
    {"PKCS8 routines", "PKCS8", "PKCS8_LIB"},
    {"X509 V3 routines", "X509V3", "X509V3_LIB"},
    {"random number generator", "RAND", "RAND_LIB"},
    {"ENGINE routines", "ENGINE", "ENGINE_LIB"},
    {"OCSP routines", "OCSP", "OCSP_LIB"},
    {"UI routines", "UI", "UI_LIB"},
    {"COMP routines", "COMP", "COMP_LIB"},
    {"ECDSA routines", "ECDSA", "ECDSA_LIB"},
    {"ECDH routines", "ECDH", "ECDH_LIB"},
    {"HMAC routines", "HMAC", "HMAC_LIB"},
    {"Digest functions", "DIGEST", "DIGEST_LIB"},
    {"Cipher functions", "CIPHER", "CIPHER_LIB"},
    {"HKDF functions", "HKDF", "HKDF_LIB"},
    {"Trust Token functions", "TRUST_TOKEN", "TRUST_TOKEN_LIB"},
    {"User defined functions", "USER", "USER_LIB"},
};

static const char *err_lib_error_string(uint32_t packed_error) {
  const uint32_t lib = ERR_GET_LIB(packed_error);
  return lib >= ERR_NUM_LIBS ? NULL : kLibraryNames[lib].str;
}

const char *ERR_lib_error_string(uint32_t packed_error) {
  const char *ret = err_lib_error_string(packed_error);
  return ret == NULL ? "unknown library" : ret;
}

const char *ERR_lib_symbol_name(uint32_t packed_error) {
  const uint32_t lib = ERR_GET_LIB(packed_error);
  return lib >= ERR_NUM_LIBS ? NULL : kLibraryNames[lib].symbol;
}

const char *ERR_func_error_string(uint32_t packed_error) {
  return "OPENSSL_internal";
}

static const char *err_reason_error_string(uint32_t packed_error, int symbol) {
  const uint32_t lib = ERR_GET_LIB(packed_error);
  const uint32_t reason = ERR_GET_REASON(packed_error);

  if (lib == ERR_LIB_SYS) {
    if (!symbol && reason < 127) {
      return strerror(reason);
    }
    return NULL;
  }

  if (reason < ERR_NUM_LIBS) {
    return symbol ? kLibraryNames[reason].reason_symbol
                  : kLibraryNames[reason].str;
  }

  if (reason < 100) {

    switch (reason) {
      case ERR_R_MALLOC_FAILURE:
        return symbol ? "MALLOC_FAILURE" : "malloc failure";
      case ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED:
        return symbol ? "SHOULD_NOT_HAVE_BEEN_CALLED"
                      : "function should not have been called";
      case ERR_R_PASSED_NULL_PARAMETER:
        return symbol ? "PASSED_NULL_PARAMETER" : "passed a null parameter";
      case ERR_R_INTERNAL_ERROR:
        return symbol ? "INTERNAL_ERROR" : "internal error";
      case ERR_R_OVERFLOW:
        return symbol ? "OVERFLOW" : "overflow";
      default:
        return NULL;
    }
  }

  return err_string_lookup(lib, reason, kOpenSSLReasonValues,
                           kOpenSSLReasonValuesLen, kOpenSSLReasonStringData);
}

const char *ERR_reason_error_string(uint32_t packed_error) {
  const char *ret = err_reason_error_string(packed_error,            0);
  return ret == NULL ? "unknown error" : ret;
}

const char *ERR_reason_symbol_name(uint32_t packed_error) {
  return err_reason_error_string(packed_error,            1);
}

char *ERR_error_string(uint32_t packed_error, char *ret) {
  static char buf[ERR_ERROR_STRING_BUF_LEN];

  if (ret == NULL) {

    ret = buf;
  }

#if !defined(NDEBUG)

  OPENSSL_memset(ret, 0, ERR_ERROR_STRING_BUF_LEN);
#endif

  return ERR_error_string_n(packed_error, ret, ERR_ERROR_STRING_BUF_LEN);
}

char *ERR_error_string_n(uint32_t packed_error, char *buf, size_t len) {
  if (len == 0) {
    return NULL;
  }

  unsigned lib = ERR_GET_LIB(packed_error);
  unsigned reason = ERR_GET_REASON(packed_error);

  const char *lib_str = err_lib_error_string(packed_error);
  const char *reason_str = err_reason_error_string(packed_error,            0);

  char lib_buf[32], reason_buf[32];
  if (lib_str == NULL) {
    snprintf(lib_buf, sizeof(lib_buf), "lib(%u)", lib);
    lib_str = lib_buf;
  }

  if (reason_str == NULL) {
    snprintf(reason_buf, sizeof(reason_buf), "reason(%u)", reason);
    reason_str = reason_buf;
  }

  int ret = snprintf(buf, len, "error:%08" PRIx32 ":%s:OPENSSL_internal:%s",
                     packed_error, lib_str, reason_str);
  if (ret >= 0 && (size_t)ret >= len) {

    static const unsigned num_colons = 4;
    unsigned i;
    char *s = buf;

    if (len <= num_colons) {

      return buf;
    }

    for (i = 0; i < num_colons; i++) {
      char *colon = strchr(s, ':');
      char *last_pos = &buf[len - 1] - num_colons + i;

      if (colon == NULL || colon > last_pos) {

        OPENSSL_memset(last_pos, ':', num_colons - i);
        break;
      }

      s = colon + 1;
    }
  }

  return buf;
}

void ERR_print_errors_cb(ERR_print_errors_callback_t callback, void *ctx) {
  char buf[ERR_ERROR_STRING_BUF_LEN];
  char buf2[1024];
  const char *file, *data;
  int line, flags;
  uint32_t packed_error;

  const unsigned long thread_hash = (uintptr_t)err_get_state();

  for (;;) {
    packed_error = ERR_get_error_line_data(&file, &line, &data, &flags);
    if (packed_error == 0) {
      break;
    }

    ERR_error_string_n(packed_error, buf, sizeof(buf));
    snprintf(buf2, sizeof(buf2), "%lu:%s:%s:%d:%s\n", thread_hash, buf, file,
             line, (flags & ERR_FLAG_STRING) ? data : "");
    if (callback(buf2, strlen(buf2), ctx) <= 0) {
      break;
    }
  }
}

static int print_errors_to_file(const char *msg, size_t msg_len, void *ctx) {
  assert(msg[msg_len] == '\0');
  FILE *fp = reinterpret_cast<FILE *>(ctx);
  int res = fputs(msg, fp);
  return res < 0 ? 0 : 1;
}

void ERR_print_errors_fp(FILE *file) {
  ERR_print_errors_cb(print_errors_to_file, file);
}

static void err_set_error_data(char *data) {
  ERR_STATE *const state = err_get_state();
  struct err_error_st *error;

  if (state == NULL || state->top == state->bottom) {
    free(data);
    return;
  }

  error = &state->errors[state->top];

  free(error->data);
  error->data = data;
}

void ERR_put_error(int library, int unused, int reason, const char *file,
                   unsigned line) {
  ERR_STATE *const state = err_get_state();
  struct err_error_st *error;

  if (state == NULL) {
    return;
  }

  if (library == ERR_LIB_SYS && reason == 0) {
#if defined(OPENSSL_WINDOWS)
    reason = GetLastError();
#else
    reason = errno;
#endif
  }

  state->top = (state->top + 1) % ERR_NUM_ERRORS;
  if (state->top == state->bottom) {
    state->bottom = (state->bottom + 1) % ERR_NUM_ERRORS;
  }

  error = &state->errors[state->top];
  err_clear(error);
  error->file = file;
  error->line = line;
  error->packed = ERR_PACK(library, reason);
}

static void err_add_error_vdata(unsigned num, va_list args) {
  size_t total_size = 0;
  const char *substr;
  char *buf;

  va_list args_copy;
  va_copy(args_copy, args);
  for (size_t i = 0; i < num; i++) {
    substr = va_arg(args_copy, const char *);
    if (substr == NULL) {
      continue;
    }
    size_t substr_len = strlen(substr);
    if (SIZE_MAX - total_size < substr_len) {
      return;
    }
    total_size += substr_len;
  }
  va_end(args_copy);
  if (total_size == SIZE_MAX) {
    return;
  }
  total_size += 1;
  if ((buf = reinterpret_cast<char *>(malloc(total_size))) == NULL) {
    return;
  }
  buf[0] = '\0';
  for (size_t i = 0; i < num; i++) {
    substr = va_arg(args, const char *);
    if (substr == NULL) {
      continue;
    }
    if (OPENSSL_strlcat(buf, substr, total_size) >= total_size) {
      assert(0);
    }
  }
  err_set_error_data(buf);
}

void ERR_add_error_data(unsigned count, ...) {
  va_list args;
  va_start(args, count);
  err_add_error_vdata(count, args);
  va_end(args);
}

void ERR_add_error_dataf(const char *format, ...) {
  char *buf = NULL;
  va_list ap;

  va_start(ap, format);
  if (OPENSSL_vasprintf_internal(&buf, format, ap,                   1) == -1) {
    return;
  }
  va_end(ap);

  err_set_error_data(buf);
}

void ERR_set_error_data(char *data, int flags) {
  if (!(flags & ERR_FLAG_STRING)) {

    assert(0);
    return;
  }

  char *copy = strdup_libc_malloc(data);
  if (copy != NULL) {
    err_set_error_data(copy);
  }
  if (flags & ERR_FLAG_MALLOCED) {

    OPENSSL_free(data);
  }
}

int ERR_set_mark(void) {
  ERR_STATE *const state = err_get_state();

  if (state == NULL || state->bottom == state->top) {
    return 0;
  }
  state->errors[state->top].mark = 1;
  return 1;
}

int ERR_pop_to_mark(void) {
  ERR_STATE *const state = err_get_state();

  if (state == NULL) {
    return 0;
  }

  while (state->bottom != state->top) {
    struct err_error_st *error = &state->errors[state->top];

    if (error->mark) {
      error->mark = 0;
      return 1;
    }

    err_clear(error);
    if (state->top == 0) {
      state->top = ERR_NUM_ERRORS - 1;
    } else {
      state->top--;
    }
  }

  return 0;
}

void ERR_load_crypto_strings(void) {}

void ERR_free_strings(void) {}

void ERR_load_BIO_strings(void) {}

void ERR_load_ERR_strings(void) {}

void ERR_load_RAND_strings(void) {}

struct err_save_state_st {
  struct err_error_st *errors;
  size_t num_errors;
};

void ERR_SAVE_STATE_free(ERR_SAVE_STATE *state) {
  if (state == NULL) {
    return;
  }
  for (size_t i = 0; i < state->num_errors; i++) {
    err_clear(&state->errors[i]);
  }
  free(state->errors);
  free(state);
}

ERR_SAVE_STATE *ERR_save_state(void) {
  ERR_STATE *const state = err_get_state();
  if (state == NULL || state->top == state->bottom) {
    return NULL;
  }

  ERR_SAVE_STATE *ret =
      reinterpret_cast<ERR_SAVE_STATE *>(malloc(sizeof(ERR_SAVE_STATE)));
  if (ret == NULL) {
    return NULL;
  }

  size_t num_errors = state->top >= state->bottom
                          ? state->top - state->bottom
                          : ERR_NUM_ERRORS + state->top - state->bottom;
  assert(num_errors < ERR_NUM_ERRORS);
  ret->errors = reinterpret_cast<err_error_st *>(
      malloc(num_errors * sizeof(struct err_error_st)));
  if (ret->errors == NULL) {
    free(ret);
    return NULL;
  }
  OPENSSL_memset(ret->errors, 0, num_errors * sizeof(struct err_error_st));
  ret->num_errors = num_errors;

  for (size_t i = 0; i < num_errors; i++) {
    size_t j = (state->bottom + i + 1) % ERR_NUM_ERRORS;
    err_copy(&ret->errors[i], &state->errors[j]);
  }
  return ret;
}

void ERR_restore_state(const ERR_SAVE_STATE *state) {
  if (state == NULL || state->num_errors == 0) {
    ERR_clear_error();
    return;
  }

  if (state->num_errors >= ERR_NUM_ERRORS) {
    abort();
  }

  ERR_STATE *const dst = err_get_state();
  if (dst == NULL) {
    return;
  }

  for (size_t i = 0; i < state->num_errors; i++) {
    err_copy(&dst->errors[i], &state->errors[i]);
  }
  dst->top = (unsigned)(state->num_errors - 1);
  dst->bottom = ERR_NUM_ERRORS - 1;
}
