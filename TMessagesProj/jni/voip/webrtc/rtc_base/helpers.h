/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_HELPERS_H_
#define RTC_BASE_HELPERS_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "rtc_base/system/rtc_export.h"

namespace rtc {

class RandomGenerator {
 public:
  virtual ~RandomGenerator() {}
  virtual bool Init(const void* seed, size_t len) = 0;
  virtual bool Generate(void* buf, size_t len) = 0;
};

void SetDefaultRandomGenerator();

void SetRandomGenerator(std::unique_ptr<RandomGenerator> generator);

void SetRandomTestMode(bool test);

bool InitRandom(int seed);
bool InitRandom(const char* seed, size_t len);

RTC_EXPORT std::string CreateRandomString(size_t length);

RTC_EXPORT bool CreateRandomString(size_t length, std::string* str);

RTC_EXPORT bool CreateRandomString(size_t length,
                                   absl::string_view table,
                                   std::string* str);

bool CreateRandomData(size_t length, std::string* data);

std::string CreateRandomUuid();

uint32_t CreateRandomId();

RTC_EXPORT uint64_t CreateRandomId64();

uint32_t CreateRandomNonZeroId();

double CreateRandomDouble();

double GetNextMovingAverage(double prev_average, double cur, double ratio);

}

#endif
