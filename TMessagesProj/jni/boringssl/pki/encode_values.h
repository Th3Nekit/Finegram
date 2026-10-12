// Copyright 2016 The Chromium Authors
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

#ifndef BSSL_DER_ENCODE_VALUES_H_
#define BSSL_DER_ENCODE_VALUES_H_

#include <stddef.h>
#include <stdint.h>

#include <openssl/base.h>

BSSL_NAMESPACE_BEGIN
namespace der {

struct GeneralizedTime;

OPENSSL_EXPORT bool EncodePosixTimeAsGeneralizedTime(
    int64_t posix_time, GeneralizedTime *generalized_time);

OPENSSL_EXPORT bool GeneralizedTimeToPosixTime(
    const der::GeneralizedTime &generalized, int64_t *result);

static const size_t kGeneralizedTimeLength = 15;

OPENSSL_EXPORT bool EncodeGeneralizedTime(const GeneralizedTime &time,
                                          uint8_t out[kGeneralizedTimeLength]);

static const size_t kUTCTimeLength = 13;

OPENSSL_EXPORT bool EncodeUTCTime(const GeneralizedTime &time,
                                  uint8_t out[kUTCTimeLength]);

}
BSSL_NAMESPACE_END

#endif
