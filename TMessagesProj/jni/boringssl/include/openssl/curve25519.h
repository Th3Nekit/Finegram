// Copyright 2015 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CURVE25519_H
#define OPENSSL_HEADER_CURVE25519_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define X25519_PRIVATE_KEY_LEN 32
#define X25519_PUBLIC_VALUE_LEN 32
#define X25519_SHARED_KEY_LEN 32

OPENSSL_EXPORT void X25519_keypair(uint8_t out_public_value[32],
                                   uint8_t out_private_key[32]);

OPENSSL_EXPORT int X25519(uint8_t out_shared_key[32],
                          const uint8_t private_key[32],
                          const uint8_t peer_public_value[32]);

OPENSSL_EXPORT void X25519_public_from_private(uint8_t out_public_value[32],
                                               const uint8_t private_key[32]);

#define ED25519_PRIVATE_KEY_LEN 64
#define ED25519_PUBLIC_KEY_LEN 32
#define ED25519_SIGNATURE_LEN 64

OPENSSL_EXPORT void ED25519_keypair(uint8_t out_public_key[32],
                                    uint8_t out_private_key[64]);

OPENSSL_EXPORT int ED25519_sign(uint8_t out_sig[64], const uint8_t *message,
                                size_t message_len,
                                const uint8_t private_key[64]);

OPENSSL_EXPORT int ED25519_verify(const uint8_t *message, size_t message_len,
                                  const uint8_t signature[64],
                                  const uint8_t public_key[32]);

OPENSSL_EXPORT void ED25519_keypair_from_seed(uint8_t out_public_key[32],
                                              uint8_t out_private_key[64],
                                              const uint8_t seed[32]);

enum spake2_role_t {
  spake2_role_alice,
  spake2_role_bob,
};

OPENSSL_EXPORT SPAKE2_CTX *SPAKE2_CTX_new(
    enum spake2_role_t my_role,
    const uint8_t *my_name, size_t my_name_len,
    const uint8_t *their_name, size_t their_name_len);

OPENSSL_EXPORT void SPAKE2_CTX_free(SPAKE2_CTX *ctx);

#define SPAKE2_MAX_MSG_SIZE 32

OPENSSL_EXPORT int SPAKE2_generate_msg(SPAKE2_CTX *ctx, uint8_t *out,
                                       size_t *out_len, size_t max_out_len,
                                       const uint8_t *password,
                                       size_t password_len);

#define SPAKE2_MAX_KEY_SIZE 64

OPENSSL_EXPORT int SPAKE2_process_msg(SPAKE2_CTX *ctx, uint8_t *out_key,
                                      size_t *out_key_len,
                                      size_t max_out_key_len,
                                      const uint8_t *their_msg,
                                      size_t their_msg_len);

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(SPAKE2_CTX, SPAKE2_CTX_free)

BSSL_NAMESPACE_END

}

#endif

#endif
