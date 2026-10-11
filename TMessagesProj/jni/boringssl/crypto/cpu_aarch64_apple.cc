// Copyright 2021 The BoringSSL Authors
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

#include "internal.h"

#if defined(OPENSSL_AARCH64) && defined(OPENSSL_APPLE) && \
    !defined(OPENSSL_STATIC_ARMCAP) && !defined(OPENSSL_NO_ASM)

#include <sys/sysctl.h>
#include <sys/types.h>

static int has_hw_feature(const char *name) {
  int value;
  size_t len = sizeof(value);
  if (sysctlbyname(name, &value, &len, NULL, 0) != 0) {
    return 0;
  }
  if (len != sizeof(int)) {

    assert(0);
    return 0;
  }

  return value != 0;
}

void OPENSSL_cpuid_setup(void) {

#if !defined(__ARM_NEON) || !defined(__ARM_FEATURE_AES) || \
    !defined(__ARM_FEATURE_SHA2)
#error "NEON and crypto extensions should be statically available."
#endif
  OPENSSL_armcap_P =
      ARMV7_NEON | ARMV8_AES | ARMV8_PMULL | ARMV8_SHA1 | ARMV8_SHA256;

  if (has_hw_feature("hw.optional.arm.FEAT_SHA512") ||
      has_hw_feature("hw.optional.armv8_2_sha512")) {
    OPENSSL_armcap_P |= ARMV8_SHA512;
  }
}

#endif
