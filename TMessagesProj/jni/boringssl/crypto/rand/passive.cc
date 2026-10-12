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

#include <openssl/ctrdrbg.h>

#include "../bcm_support.h"
#include "../fipsmodule/bcm_interface.h"
#include "../internal.h"

#if defined(BORINGSSL_FIPS)

#include <atomic>

static void passive_get_seed_entropy(uint8_t *out_entropy,
                                     size_t out_entropy_len,
                                     int *out_want_additional_input) {
  *out_want_additional_input = 0;
  if (bcm_success(BCM_rand_bytes_hwrng(out_entropy, out_entropy_len))) {
    *out_want_additional_input = 1;
  } else {
    CRYPTO_sysrand_for_seed(out_entropy, out_entropy_len);
  }
}

#define ENTROPY_READ_LEN \
  (                      16 + CTR_DRBG_ENTROPY_LEN * BORINGSSL_FIPS_OVERREAD)

#if defined(OPENSSL_ANDROID)

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

enum class socket_history_t {

  socket_not_yet_attempted = 0,

  socket_success,

  socket_failed,
};

static std::atomic<socket_history_t> g_socket_history{
    socket_history_t::socket_not_yet_attempted};

#define DAEMON_RESPONSE_LEN 496

static_assert(ENTROPY_READ_LEN == DAEMON_RESPONSE_LEN,
              "entropy daemon response length mismatch");

static int get_seed_from_daemon(uint8_t *out_entropy, size_t out_entropy_len) {

  if (out_entropy_len > DAEMON_RESPONSE_LEN) {
    abort();
  }

  const socket_history_t socket_history =
      g_socket_history.load(std::memory_order_acquire);
  if (socket_history == socket_history_t::socket_failed) {
    return 0;
  }

  int ret = 0;
  static const char kSocketPath[] = "/dev/socket/prng_seeder";
  struct sockaddr_un sun;
  uint8_t buffer[DAEMON_RESPONSE_LEN];
  size_t done = 0;
  const int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0) {
    goto out;
  }

  memset(&sun, 0, sizeof(sun));
  sun.sun_family = AF_UNIX;
  static_assert(sizeof(kSocketPath) <= UNIX_PATH_MAX, "kSocketPath too long");
  OPENSSL_memcpy(sun.sun_path, kSocketPath, sizeof(kSocketPath));

  if (connect(sock, (struct sockaddr *)&sun, sizeof(sun))) {
    goto out;
  }

  while (done < sizeof(buffer)) {
    ssize_t n;
    do {
      n = read(sock, buffer + done, sizeof(buffer) - done);
    } while (n == -1 && errno == EINTR);

    if (n < 1) {
      goto out;
    }
    done += n;
  }

  if (done != DAEMON_RESPONSE_LEN) {

    goto out;
  }

  assert(out_entropy_len <= DAEMON_RESPONSE_LEN);
  OPENSSL_memcpy(out_entropy, buffer, out_entropy_len);
  ret = 1;

out:
  if (socket_history == socket_history_t::socket_not_yet_attempted) {
    socket_history_t expected = socket_history_t::socket_not_yet_attempted;

    g_socket_history.compare_exchange_strong(
        expected,
        (ret == 0) ? socket_history_t::socket_failed
                   : socket_history_t::socket_success,
        std::memory_order_release, std::memory_order_relaxed);
  }

  close(sock);
  return ret;
}

#else

static int get_seed_from_daemon(uint8_t *out_entropy, size_t out_entropy_len) {
  return 0;
}

#endif

void RAND_need_entropy(size_t bytes_needed) {
  uint8_t buf[ENTROPY_READ_LEN];
  size_t todo = sizeof(buf);
  if (todo > bytes_needed) {
    todo = bytes_needed;
  }

  int want_additional_input;
  if (get_seed_from_daemon(buf, todo)) {
    want_additional_input = 1;
  } else {
    passive_get_seed_entropy(buf, todo, &want_additional_input);
  }

  if (boringssl_fips_break_test("CRNG")) {

    OPENSSL_memset(buf, 0, todo);
  }

  BCM_rand_load_entropy(buf, todo, want_additional_input);
}

#endif
