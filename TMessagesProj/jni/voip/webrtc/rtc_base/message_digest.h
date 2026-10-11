/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_MESSAGE_DIGEST_H_
#define RTC_BASE_MESSAGE_DIGEST_H_

#include <stddef.h>

#include <string>

#include "absl/strings/string_view.h"

namespace rtc {

extern const char DIGEST_MD5[];
extern const char DIGEST_SHA_1[];
extern const char DIGEST_SHA_224[];
extern const char DIGEST_SHA_256[];
extern const char DIGEST_SHA_384[];
extern const char DIGEST_SHA_512[];

class MessageDigest {
 public:
  enum { kMaxSize = 64 };
  virtual ~MessageDigest() {}

  virtual size_t Size() const = 0;

  virtual void Update(const void* buf, size_t len) = 0;

  virtual size_t Finish(void* buf, size_t len) = 0;
};

class MessageDigestFactory {
 public:
  static MessageDigest* Create(absl::string_view alg);
};

bool IsFips180DigestAlgorithm(absl::string_view alg);

size_t ComputeDigest(MessageDigest* digest,
                     const void* input,
                     size_t in_len,
                     void* output,
                     size_t out_len);

size_t ComputeDigest(absl::string_view alg,
                     const void* input,
                     size_t in_len,
                     void* output,
                     size_t out_len);

std::string ComputeDigest(MessageDigest* digest, absl::string_view input);

std::string ComputeDigest(absl::string_view alg, absl::string_view input);

bool ComputeDigest(absl::string_view alg,
                   absl::string_view input,
                   std::string* output);

inline std::string MD5(absl::string_view input) {
  return ComputeDigest(DIGEST_MD5, input);
}

size_t ComputeHmac(MessageDigest* digest,
                   const void* key,
                   size_t key_len,
                   const void* input,
                   size_t in_len,
                   void* output,
                   size_t out_len);

size_t ComputeHmac(absl::string_view alg,
                   const void* key,
                   size_t key_len,
                   const void* input,
                   size_t in_len,
                   void* output,
                   size_t out_len);

std::string ComputeHmac(MessageDigest* digest,
                        absl::string_view key,
                        absl::string_view input);

std::string ComputeHmac(absl::string_view alg,
                        absl::string_view key,
                        absl::string_view input);

bool ComputeHmac(absl::string_view alg,
                 absl::string_view key,
                 absl::string_view input,
                 std::string* output);

}

#endif
