// Copyright 2024 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_SPAKE2PLUS_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_SPAKE2PLUS_INTERNAL_H

#include <openssl/base.h>

#include <sys/types.h>

#include <openssl/sha.h>
#include <openssl/span.h>

#include "../fipsmodule/ec/internal.h"

BSSL_NAMESPACE_BEGIN

namespace spake2plus {

constexpr size_t kShareSize = 65;

constexpr size_t kConfirmSize = 32;

constexpr size_t kVerifierSize = 32;

constexpr size_t kRegistrationRecordSize = 65;

constexpr size_t kSecretSize = 32;

[[nodiscard]] OPENSSL_EXPORT bool Register(
    Span<uint8_t> out_w0, Span<uint8_t> out_w1,
    Span<uint8_t> out_registration_record, Span<const uint8_t> password,
    Span<const uint8_t> id_prover, Span<const uint8_t> id_verifier);

class OPENSSL_EXPORT Prover {
 public:
  static constexpr bool kAllowUniquePtr = true;

  Prover();
  ~Prover();

  [[nodiscard]] bool Init(Span<const uint8_t> context,
                          Span<const uint8_t> id_prover,
                          Span<const uint8_t> id_verifier,
                          Span<const uint8_t> w0, Span<const uint8_t> w1,
                          Span<const uint8_t> x = Span<const uint8_t>());

  [[nodiscard]] bool GenerateShare(Span<uint8_t> out_share);

  [[nodiscard]] bool ComputeConfirmation(Span<uint8_t> out_confirm,
                                         Span<uint8_t> out_secret,
                                         Span<const uint8_t> peer_share,
                                         Span<const uint8_t> peer_confirm);

 private:
  enum class State {
    kInit,
    kShareGenerated,
    kConfirmGenerated,
    kDone,
  };

  State state_ = State::kInit;
  SHA256_CTX transcript_hash_;
  EC_SCALAR w0_;
  EC_SCALAR w1_;
  EC_SCALAR x_;
  EC_AFFINE X_;
  uint8_t share_[kShareSize];
};

class OPENSSL_EXPORT Verifier {
 public:
  static constexpr bool kAllowUniquePtr = true;

  Verifier();
  ~Verifier();

  [[nodiscard]] bool Init(Span<const uint8_t> context,
                          Span<const uint8_t> id_prover,
                          Span<const uint8_t> id_verifier,
                          Span<const uint8_t> w0,
                          Span<const uint8_t> registration_record,
                          Span<const uint8_t> y = Span<const uint8_t>());

  [[nodiscard]] bool ProcessProverShare(Span<uint8_t> out_share,
                                        Span<uint8_t> out_confirm,
                                        Span<uint8_t> out_secret,
                                        Span<const uint8_t> prover_share);

  [[nodiscard]] bool VerifyProverConfirmation(Span<const uint8_t> peer_confirm);

 private:
  enum class State {
    kInit,
    kProverShareSeen,
    kDone,
  };

  State state_ = State::kInit;
  SHA256_CTX transcript_hash_;
  EC_SCALAR w0_;
  EC_AFFINE L_;
  EC_SCALAR y_;
  uint8_t confirm_[kConfirmSize];
};

}

BSSL_NAMESPACE_END

#endif
