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

// Ensure we can't call OPENSSL_malloc circularly.
#define _BORINGSSL_PROHIBIT_OPENSSL_MALLOC
#include "internal.h"

#if defined(OPENSSL_WINDOWS_THREADS)

#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static BOOL CALLBACK call_once_init(INIT_ONCE *once, void *arg, void **out) {
  void (**init)(void) = (void (**)(void))arg;
  (**init)();
  return TRUE;
}

void CRYPTO_once(CRYPTO_once_t *once, void (*init)(void)) {
  if (!InitOnceExecuteOnce(once, call_once_init, &init, NULL)) {
    abort();
  }
}

void CRYPTO_MUTEX_init(CRYPTO_MUTEX *lock) { InitializeSRWLock(lock); }

void CRYPTO_MUTEX_lock_read(CRYPTO_MUTEX *lock) { AcquireSRWLockShared(lock); }

void CRYPTO_MUTEX_lock_write(CRYPTO_MUTEX *lock) {
  AcquireSRWLockExclusive(lock);
}

void CRYPTO_MUTEX_unlock_read(CRYPTO_MUTEX *lock) {
  ReleaseSRWLockShared(lock);
}

void CRYPTO_MUTEX_unlock_write(CRYPTO_MUTEX *lock) {
  ReleaseSRWLockExclusive(lock);
}

void CRYPTO_MUTEX_cleanup(CRYPTO_MUTEX *lock) {

}

static SRWLOCK g_destructors_lock = SRWLOCK_INIT;
static thread_local_destructor_t g_destructors[NUM_OPENSSL_THREAD_LOCALS];

static CRYPTO_once_t g_thread_local_init_once = CRYPTO_ONCE_INIT;
static DWORD g_thread_local_key;
static int g_thread_local_failed;

static void thread_local_init(void) {
  g_thread_local_key = TlsAlloc();
  g_thread_local_failed = (g_thread_local_key == TLS_OUT_OF_INDEXES);
}

static void NTAPI thread_local_destructor(PVOID module, DWORD reason,
                                          PVOID reserved) {

  if (reason != DLL_THREAD_DETACH) {
    return;
  }

  CRYPTO_once(&g_thread_local_init_once, thread_local_init);
  if (g_thread_local_failed) {
    return;
  }

  void **pointers = (void **)TlsGetValue(g_thread_local_key);
  if (pointers == NULL) {
    return;
  }

  thread_local_destructor_t destructors[NUM_OPENSSL_THREAD_LOCALS];

  AcquireSRWLockExclusive(&g_destructors_lock);
  OPENSSL_memcpy(destructors, g_destructors, sizeof(destructors));
  ReleaseSRWLockExclusive(&g_destructors_lock);

  for (unsigned i = 0; i < NUM_OPENSSL_THREAD_LOCALS; i++) {
    if (destructors[i] != NULL) {
      destructors[i](pointers[i]);
    }
  }

  free(pointers);
}

#define STRINGIFY(x) #x
#define EXPAND_AND_STRINGIFY(x) STRINGIFY(x)
#ifdef _WIN64
__pragma(comment(linker, "/INCLUDE:_tls_used")) __pragma(comment(
    linker, "/INCLUDE:" EXPAND_AND_STRINGIFY(p_thread_callback_boringssl)))
#else
__pragma(comment(linker, "/INCLUDE:__tls_used")) __pragma(comment(
    linker, "/INCLUDE:_" EXPAND_AND_STRINGIFY(p_thread_callback_boringssl)))
#endif

#ifdef _WIN64

#pragma const_seg(".CRT$XLC")

extern "C" {
  extern const PIMAGE_TLS_CALLBACK p_thread_callback_boringssl;
}

const PIMAGE_TLS_CALLBACK p_thread_callback_boringssl = thread_local_destructor;

#pragma const_seg()

#else

#pragma data_seg(".CRT$XLC")

extern "C" {
  extern PIMAGE_TLS_CALLBACK p_thread_callback_boringssl;
}

PIMAGE_TLS_CALLBACK p_thread_callback_boringssl = thread_local_destructor;

#pragma data_seg()

#endif

static void **get_thread_locals(void) {

  DWORD last_error = GetLastError();
  void **ret = reinterpret_cast<void **>(TlsGetValue(g_thread_local_key));
  SetLastError(last_error);
  return ret;
}

void *CRYPTO_get_thread_local(thread_local_data_t index) {
  CRYPTO_once(&g_thread_local_init_once, thread_local_init);
  if (g_thread_local_failed) {
    return NULL;
  }

  void **pointers = get_thread_locals();
  if (pointers == NULL) {
    return NULL;
  }
  return pointers[index];
}

int CRYPTO_set_thread_local(thread_local_data_t index, void *value,
                            thread_local_destructor_t destructor) {
  CRYPTO_once(&g_thread_local_init_once, thread_local_init);
  if (g_thread_local_failed) {
    destructor(value);
    return 0;
  }

  void **pointers = get_thread_locals();
  if (pointers == NULL) {
    pointers = reinterpret_cast<void **>(
        malloc(sizeof(void *) * NUM_OPENSSL_THREAD_LOCALS));
    if (pointers == NULL) {
      destructor(value);
      return 0;
    }
    OPENSSL_memset(pointers, 0, sizeof(void *) * NUM_OPENSSL_THREAD_LOCALS);
    if (TlsSetValue(g_thread_local_key, pointers) == 0) {
      free(pointers);
      destructor(value);
      return 0;
    }
  }

  AcquireSRWLockExclusive(&g_destructors_lock);
  g_destructors[index] = destructor;
  ReleaseSRWLockExclusive(&g_destructors_lock);

  pointers[index] = value;
  return 1;
}

#endif
