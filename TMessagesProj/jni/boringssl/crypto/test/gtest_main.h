// Copyright 2017 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_TEST_GTEST_MAIN_H
#define OPENSSL_HEADER_CRYPTO_TEST_GTEST_MAIN_H

#include <stdio.h>
#include <stdlib.h>

#include <gtest/gtest.h>

#include <openssl/crypto.h>
#include <openssl/err.h>

#if defined(OPENSSL_WINDOWS)
#include <winsock2.h>
#else
#include <signal.h>
#endif

#include "../internal.h"

BSSL_NAMESPACE_BEGIN

class TestEventListener : public testing::EmptyTestEventListener {
 public:
  TestEventListener() {}
  ~TestEventListener() override {}

  void OnTestEnd(const testing::TestInfo &test_info) override {
    if (test_info.result()->Failed()) {

      ERR_print_errors_fp(stdout);
    } else {

      ERR_clear_error();
    }

    OPENSSL_reset_malloc_counter_for_testing();
  }
};

inline void SetupGoogleTest() {
#if defined(OPENSSL_WINDOWS)

  WORD wsa_version = MAKEWORD(2, 2);
  WSADATA wsa_data;
  int wsa_err = WSAStartup(wsa_version, &wsa_data);
  if (wsa_err != 0) {
    fprintf(stderr, "WSAStartup failed: %d\n", wsa_err);
    exit(1);
  }
  if (wsa_data.wVersion != wsa_version) {
    fprintf(stderr, "Didn't get expected version: %x\n", wsa_data.wVersion);
    exit(1);
  }
#else

  signal(SIGPIPE, SIG_IGN);
#endif

  testing::UnitTest::GetInstance()->listeners().Append(new TestEventListener);
}

BSSL_NAMESPACE_END

#endif
