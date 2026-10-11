// Copyright 2020 The BoringSSL Authors
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

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "../bcm_support.h"
#include "../internal.h"
#include "internal.h"

#if defined(OPENSSL_FORK_DETECTION_MADVISE)
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#if defined(MADV_WIPEONFORK)
static_assert(MADV_WIPEONFORK == 18, "MADV_WIPEONFORK is not 18");
#else
#define MADV_WIPEONFORK 18
#endif
#elif defined(OPENSSL_FORK_DETECTION_PTHREAD_ATFORK)
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#if defined(OPENSSL_FORK_DETECTION_MADVISE)
static int g_force_madv_wipeonfork;
static int g_force_madv_wipeonfork_enabled;
static CRYPTO_once_t g_fork_detect_once = CRYPTO_ONCE_INIT;
static CRYPTO_MUTEX g_fork_detect_lock = CRYPTO_MUTEX_INIT;
static CRYPTO_atomic_u32 *g_fork_detect_addr;
static uint64_t g_fork_generation;

static void init_fork_detect(void) {
  if (g_force_madv_wipeonfork) {
    return;
  }

  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) {
    return;
  }

  void *addr = mmap(NULL, (size_t)page_size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (addr == MAP_FAILED) {
    return;
  }

  if (madvise(addr, (size_t)page_size, -1) == 0 ||
      madvise(addr, (size_t)page_size, MADV_WIPEONFORK) != 0) {
    munmap(addr, (size_t)page_size);
    return;
  }

  CRYPTO_atomic_u32 *const atomic = reinterpret_cast<CRYPTO_atomic_u32 *>(addr);
  CRYPTO_atomic_store_u32(atomic, 1);
  g_fork_detect_addr = atomic;
  g_fork_generation = 1;
}

uint64_t CRYPTO_get_fork_generation(void) {
  CRYPTO_once(&g_fork_detect_once, init_fork_detect);

  CRYPTO_atomic_u32 *const flag_ptr = g_fork_detect_addr;
  if (flag_ptr == NULL) {

    if (g_force_madv_wipeonfork && g_force_madv_wipeonfork_enabled) {

      return 42;
    }

    return 0;
  }

  uint64_t *const generation_ptr = &g_fork_generation;
  if (CRYPTO_atomic_load_u32(flag_ptr) != 0) {

    return *generation_ptr;
  }

  CRYPTO_MUTEX *const lock = &g_fork_detect_lock;
  CRYPTO_MUTEX_lock_write(lock);
  uint64_t current_generation = *generation_ptr;
  if (CRYPTO_atomic_load_u32(flag_ptr) == 0) {

    current_generation++;
    if (current_generation == 0) {

      current_generation = 1;
    }

    *generation_ptr = current_generation;
    CRYPTO_atomic_store_u32(flag_ptr, 1);
  }
  CRYPTO_MUTEX_unlock_write(lock);

  return current_generation;
}

void CRYPTO_fork_detect_force_madv_wipeonfork_for_testing(int on) {
  g_force_madv_wipeonfork = 1;
  g_force_madv_wipeonfork_enabled = on;
}

#elif defined(OPENSSL_FORK_DETECTION_PTHREAD_ATFORK)

static CRYPTO_once_t g_pthread_fork_detection_once = CRYPTO_ONCE_INIT;
static uint64_t g_atfork_fork_generation;

static void we_are_forked(void) {

  uint64_t value = g_atfork_fork_generation + 1;
  if (value == 0) {
    value = 1;
  }
  g_atfork_fork_generation = value;
}

static void init_pthread_fork_detection(void) {
  if (pthread_atfork(NULL, NULL, we_are_forked) != 0) {
    abort();
  }
  g_atfork_fork_generation = 1;
}

uint64_t CRYPTO_get_fork_generation(void) {
  CRYPTO_once(&g_pthread_fork_detection_once, init_pthread_fork_detection);

  return g_atfork_fork_generation;
}

#elif defined(OPENSSL_DOES_NOT_FORK)

uint64_t CRYPTO_get_fork_generation(void) { return 0xc0ffee; }

#else

uint64_t CRYPTO_get_fork_generation(void) { return 0; }

#endif
