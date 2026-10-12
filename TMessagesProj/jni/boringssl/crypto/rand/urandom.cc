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

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <openssl/rand.h>

#include "../bcm_support.h"
#include "internal.h"

#if defined(OPENSSL_RAND_URANDOM)

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if defined(OPENSSL_LINUX)
#if defined(BORINGSSL_FIPS)
#include <linux/random.h>
#include <sys/ioctl.h>
#endif
#include <sys/syscall.h>

#if defined(OPENSSL_ANDROID)
#include <sys/system_properties.h>
#endif

#if !defined(OPENSSL_ANDROID)
#define OPENSSL_HAS_GETAUXVAL
#endif

#if defined(__GLIBC_PREREQ)
#if !__GLIBC_PREREQ(2, 16)
#undef OPENSSL_HAS_GETAUXVAL
#endif
#endif
#if defined(OPENSSL_HAS_GETAUXVAL)
#include <sys/auxv.h>
#endif
#endif

#include <openssl/mem.h>
#include <openssl/thread.h>

#include "../internal.h"
#include "getrandom_fillin.h"

#if defined(USE_NR_getrandom)

#if defined(OPENSSL_MSAN)
extern "C" {
void __msan_unpoison(void *, size_t);
}
#endif

static ssize_t boringssl_getrandom(void *buf, size_t buf_len, unsigned flags) {
  ssize_t ret;
  do {
    ret = syscall(__NR_getrandom, buf, buf_len, flags);
  } while (ret == -1 && errno == EINTR);

#if defined(OPENSSL_MSAN)
  if (ret > 0) {

    __msan_unpoison(buf, ret);
  }
#endif

  return ret;
}

#endif

static const int kHaveGetrandom = -3;

static int urandom_fd;

#if defined(USE_NR_getrandom)

static int getrandom_ready;

static int extra_getrandom_flags_for_seed;

static void maybe_set_extra_getrandom_flags(void) {
#if defined(BORINGSSL_FIPS) && defined(OPENSSL_ANDROID)
  char value[PROP_VALUE_MAX + 1];
  int length = __system_property_get("ro.boringcrypto.hwrand", value);
  if (length < 0 || length > PROP_VALUE_MAX) {
    return;
  }

  value[length] = 0;
  if (OPENSSL_strcasecmp(value, "true") == 0) {
    extra_getrandom_flags_for_seed = GRND_RANDOM;
  }
#endif
}

#endif

static CRYPTO_once_t rand_once = CRYPTO_ONCE_INIT;

static void init_once(void) {
#if defined(USE_NR_getrandom)
  int have_getrandom;
  uint8_t dummy;
  ssize_t getrandom_ret =
      boringssl_getrandom(&dummy, sizeof(dummy), GRND_NONBLOCK);
  if (getrandom_ret == 1) {
    getrandom_ready = 1;
    have_getrandom = 1;
  } else if (getrandom_ret == -1 && errno == EAGAIN) {

    have_getrandom = 1;
  } else if (getrandom_ret == -1 && errno == ENOSYS) {

    have_getrandom = 0;
  } else {

    perror("getrandom");
    abort();
  }

  if (have_getrandom) {
    urandom_fd = kHaveGetrandom;
    maybe_set_extra_getrandom_flags();
    return;
  }
#endif

#if defined(BORINGSSL_FIPS)
  perror("getrandom not found");
  abort();
#endif

  int fd;
  do {
    fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
  } while (fd == -1 && errno == EINTR);

  if (fd < 0) {
    perror("failed to open /dev/urandom");
    abort();
  }

  urandom_fd = fd;
}

static CRYPTO_once_t wait_for_entropy_once = CRYPTO_ONCE_INIT;

static void wait_for_entropy(void) {
  int fd = urandom_fd;
  if (fd == kHaveGetrandom) {

#if defined(USE_NR_getrandom)
    if (getrandom_ready) {

      return;
    }

    uint8_t dummy;
    ssize_t getrandom_ret =
        boringssl_getrandom(&dummy, sizeof(dummy), GRND_NONBLOCK);
    if (getrandom_ret == -1 && errno == EAGAIN) {

      const char *current_process = "<unknown>";
#if defined(OPENSSL_HAS_GETAUXVAL)
      const unsigned long getauxval_ret = getauxval(AT_EXECFN);
      if (getauxval_ret != 0) {
        current_process = (const char *)getauxval_ret;
      }
#endif

      fprintf(
          stderr,
          "%s: getrandom indicates that the entropy pool has not been "
          "initialized. Rather than continue with poor entropy, this process "
          "will block until entropy is available.\n",
          current_process);

      getrandom_ret =
          boringssl_getrandom(&dummy, sizeof(dummy), 0               );
    }

    if (getrandom_ret != 1) {
      perror("getrandom");
      abort();
    }
#endif
    return;
  }
}

static int fill_with_entropy(uint8_t *out, size_t len, int block, int seed) {
  if (len == 0) {
    return 1;
  }

#if defined(USE_NR_getrandom) || defined(FREEBSD_GETRANDOM)
  int getrandom_flags = 0;
  if (!block) {
    getrandom_flags |= GRND_NONBLOCK;
  }
#endif

#if defined(USE_NR_getrandom)
  if (seed) {
    getrandom_flags |= extra_getrandom_flags_for_seed;
  }
#endif

  CRYPTO_init_sysrand();
  if (block) {
    CRYPTO_once(&wait_for_entropy_once, wait_for_entropy);
  }

  errno = 0;
  while (len > 0) {
    ssize_t r;

    if (urandom_fd == kHaveGetrandom) {
#if defined(USE_NR_getrandom)
      r = boringssl_getrandom(out, len, getrandom_flags);
#else
      fprintf(stderr, "urandom fd corrupt.\n");
      abort();
#endif
    } else {
      do {
        r = read(urandom_fd, out, len);
      } while (r == -1 && errno == EINTR);
    }

    if (r <= 0) {
      return 0;
    }
    out += r;
    len -= r;
  }

  return 1;
}

void CRYPTO_init_sysrand(void) { CRYPTO_once(&rand_once, init_once); }

void CRYPTO_sysrand(uint8_t *out, size_t requested) {
  if (!fill_with_entropy(out, requested,           1,          0)) {
    perror("entropy fill failed");
    abort();
  }
}

void CRYPTO_sysrand_for_seed(uint8_t *out, size_t requested) {
  if (!fill_with_entropy(out, requested,           1,          1)) {
    perror("entropy fill failed");
    abort();
  }
}

int CRYPTO_sysrand_if_available(uint8_t *out, size_t requested) {
  if (fill_with_entropy(out, requested,           0,          0)) {
    return 1;
  } else if (errno == EAGAIN) {
    OPENSSL_memset(out, 0, requested);
    return 0;
  } else {
    perror("opportunistic entropy fill failed");
    abort();
  }
}

#endif
