// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_SERVICE_INDICATOR_H
#define OPENSSL_HEADER_SERVICE_INDICATOR_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT uint64_t FIPS_service_indicator_before_call(void);
OPENSSL_EXPORT uint64_t FIPS_service_indicator_after_call(void);

#if defined(__cplusplus)
}

#if !defined(BORINGSSL_NO_CXX)

extern "C++" {

#define CALL_SERVICE_AND_CHECK_APPROVED(approved, func)         \
  [&] {                                                       \
    bssl::FIPSIndicatorHelper fips_indicator_helper(&approved); \
    return func;                                                \
  }()

BSSL_NAMESPACE_BEGIN

enum class FIPSStatus {
  NOT_APPROVED = 0,
  APPROVED = 1,
};

class FIPSIndicatorHelper {
 public:
  FIPSIndicatorHelper(FIPSStatus *result)
      : result_(result), before_(FIPS_service_indicator_before_call()) {
    *result_ = FIPSStatus::NOT_APPROVED;
  }

  ~FIPSIndicatorHelper() {
    uint64_t after = FIPS_service_indicator_after_call();
    if (after != before_) {
      *result_ = FIPSStatus::APPROVED;
    }
  }

  FIPSIndicatorHelper(const FIPSIndicatorHelper&) = delete;
  FIPSIndicatorHelper &operator=(const FIPSIndicatorHelper &) = delete;

 private:
  FIPSStatus *const result_;
  const uint64_t before_;
};

BSSL_NAMESPACE_END
}

#endif
#endif

#endif
