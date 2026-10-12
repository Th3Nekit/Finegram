// Copyright 2015 The Chromium Authors
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

#ifndef BSSL_PKI_EXTENDED_KEY_USAGE_H_
#define BSSL_PKI_EXTENDED_KEY_USAGE_H_

#include <vector>

#include <openssl/base.h>

#include "input.h"

BSSL_NAMESPACE_BEGIN

inline constexpr uint8_t kAnyEKU[] = {0x55, 0x1d, 0x25, 0x00};

inline constexpr uint8_t kServerAuth[] = {0x2b, 0x06, 0x01, 0x05,
                                          0x05, 0x07, 0x03, 0x01};

inline constexpr uint8_t kClientAuth[] = {0x2b, 0x06, 0x01, 0x05,
                                          0x05, 0x07, 0x03, 0x02};

inline constexpr uint8_t kCodeSigning[] = {0x2b, 0x06, 0x01, 0x05,
                                           0x05, 0x07, 0x03, 0x03};

inline constexpr uint8_t kEmailProtection[] = {0x2b, 0x06, 0x01, 0x05,
                                               0x05, 0x07, 0x03, 0x04};

inline constexpr uint8_t kTimeStamping[] = {0x2b, 0x06, 0x01, 0x05,
                                            0x05, 0x07, 0x03, 0x08};

inline constexpr uint8_t kOCSPSigning[] = {0x2b, 0x06, 0x01, 0x05,
                                           0x05, 0x07, 0x03, 0x09};

inline constexpr uint8_t kRcsMlsClient[] = {0x67, 0x81, 0x12, 0x02, 0x01, 0x03};

OPENSSL_EXPORT bool ParseEKUExtension(der::Input extension_value,
                                      std::vector<der::Input> *eku_oids);

BSSL_NAMESPACE_END

#endif
