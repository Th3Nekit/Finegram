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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_BCM_INTERFACE_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_BCM_INTERFACE_H

#include <openssl/aes.h>
#include <openssl/bcm_public.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum class bcm_infallible_t {
  approved,
  not_approved,
};

enum class bcm_status_t {
  approved,
  not_approved,
  failure,
};
typedef enum bcm_status_t bcm_status;
typedef enum bcm_infallible_t bcm_infallible;

inline int bcm_success(bcm_status status) {
  return status == bcm_status::approved || status == bcm_status::not_approved;
}

inline bcm_status_t bcm_as_approved_status(int result) {
  return result ? bcm_status::approved : bcm_status::failure;
}

inline bcm_status_t bcm_as_not_approved_status(int result) {
  return result ? bcm_status::not_approved : bcm_status::failure;
}

#if defined(BORINGSSL_FIPS)

#define BORINGSSL_FIPS_OVERREAD 10

#endif

bcm_infallible BCM_rand_load_entropy(const uint8_t *entropy, size_t entropy_len,
                                     int want_additional_input);

OPENSSL_EXPORT bcm_infallible BCM_rand_bytes(uint8_t *out, size_t out_len);

bcm_status BCM_rand_bytes_hwrng(uint8_t *out, size_t len);

bcm_infallible BCM_rand_bytes_with_additional_data(
    uint8_t *out, size_t out_len, const uint8_t user_additional_data[32]);

#define BCM_SHA_DIGEST_LENGTH 20

bcm_infallible BCM_sha1_init(SHA_CTX *sha);

bcm_infallible BCM_sha1_transform(SHA_CTX *c,
                                  const uint8_t data[BCM_SHA_CBLOCK]);

bcm_infallible BCM_sha1_update(SHA_CTX *c, const void *data, size_t len);

bcm_infallible BCM_sha1_final(uint8_t out[BCM_SHA_DIGEST_LENGTH], SHA_CTX *c);

bcm_infallible BCM_fips_186_2_prf(uint8_t *out, size_t out_len,
                                  const uint8_t xkey[BCM_SHA_DIGEST_LENGTH]);

#define BCM_SHA224_DIGEST_LENGTH 28

bcm_infallible BCM_sha224_init(SHA256_CTX *sha);

bcm_infallible BCM_sha224_update(SHA256_CTX *sha, const void *data, size_t len);

bcm_infallible BCM_sha224_final(uint8_t out[BCM_SHA224_DIGEST_LENGTH],
                                SHA256_CTX *sha);

#define BCM_SHA256_DIGEST_LENGTH 32

bcm_infallible BCM_sha256_init(SHA256_CTX *sha);

bcm_infallible BCM_sha256_update(SHA256_CTX *sha, const void *data, size_t len);

bcm_infallible BCM_sha256_final(uint8_t out[BCM_SHA256_DIGEST_LENGTH],
                                SHA256_CTX *sha);

bcm_infallible BCM_sha256_transform(SHA256_CTX *sha,
                                    const uint8_t block[BCM_SHA256_CBLOCK]);

bcm_infallible BCM_sha256_transform_blocks(uint32_t state[8],
                                           const uint8_t *data,
                                           size_t num_blocks);

#define BCM_SHA384_DIGEST_LENGTH 48

bcm_infallible BCM_sha384_init(SHA512_CTX *sha);

bcm_infallible BCM_sha384_update(SHA512_CTX *sha, const void *data, size_t len);

bcm_infallible BCM_sha384_final(uint8_t out[BCM_SHA384_DIGEST_LENGTH],
                                SHA512_CTX *sha);

#define BCM_SHA512_DIGEST_LENGTH 64

bcm_infallible BCM_sha512_init(SHA512_CTX *sha);

bcm_infallible BCM_sha512_update(SHA512_CTX *sha, const void *data, size_t len);

bcm_infallible BCM_sha512_final(uint8_t out[BCM_SHA512_DIGEST_LENGTH],
                                SHA512_CTX *sha);

bcm_infallible BCM_sha512_transform(SHA512_CTX *sha,
                                    const uint8_t block[BCM_SHA512_CBLOCK]);

#define BCM_SHA512_256_DIGEST_LENGTH 32

bcm_infallible BCM_sha512_256_init(SHA512_CTX *sha);

bcm_infallible BCM_sha512_256_update(SHA512_CTX *sha, const void *data,
                                     size_t len);

bcm_infallible BCM_sha512_256_final(uint8_t out[BCM_SHA512_256_DIGEST_LENGTH],
                                    SHA512_CTX *sha);

#define BCM_MLDSA_SIGNATURE_RANDOMIZER_BYTES 32

#define BCM_MLDSA_SEED_BYTES 32

#define BCM_MLDSA65_PRIVATE_KEY_BYTES 4032

#define BCM_MLDSA65_PUBLIC_KEY_BYTES 1952

#define BCM_MLDSA65_SIGNATURE_BYTES 3309

struct BCM_mldsa65_private_key {
  union {
    uint8_t bytes[32 + 32 + 64 + 256 * 4 * (5 + 6 + 6)];
    uint32_t alignment;
  } opaque;
};

struct BCM_mldsa65_public_key {
  union {
    uint8_t bytes[32 + 64 + 256 * 4 * 6];
    uint32_t alignment;
  } opaque;
};

OPENSSL_EXPORT bcm_status BCM_mldsa65_generate_key(
    uint8_t out_encoded_public_key[BCM_MLDSA65_PUBLIC_KEY_BYTES],
    uint8_t out_seed[BCM_MLDSA_SEED_BYTES],
    struct BCM_mldsa65_private_key *out_private_key);

OPENSSL_EXPORT bcm_status BCM_mldsa65_private_key_from_seed(
    struct BCM_mldsa65_private_key *out_private_key,
    const uint8_t seed[BCM_MLDSA_SEED_BYTES]);

OPENSSL_EXPORT bcm_status BCM_mldsa65_public_from_private(
    struct BCM_mldsa65_public_key *out_public_key,
    const struct BCM_mldsa65_private_key *private_key);

OPENSSL_EXPORT bcm_status
BCM_mldsa65_check_key_fips(struct BCM_mldsa65_private_key *private_key);

OPENSSL_EXPORT bcm_status BCM_mldsa65_generate_key_fips(
    uint8_t out_encoded_public_key[BCM_MLDSA65_PUBLIC_KEY_BYTES],
    uint8_t out_seed[BCM_MLDSA_SEED_BYTES],
    struct BCM_mldsa65_private_key *out_private_key);

OPENSSL_EXPORT bcm_status BCM_mldsa65_private_key_from_seed_fips(
    struct BCM_mldsa65_private_key *out_private_key,
    const uint8_t seed[BCM_MLDSA_SEED_BYTES]);

OPENSSL_EXPORT bcm_status BCM_mldsa65_sign(
    uint8_t out_encoded_signature[BCM_MLDSA65_SIGNATURE_BYTES],
    const struct BCM_mldsa65_private_key *private_key, const uint8_t *msg,
    size_t msg_len, const uint8_t *context, size_t context_len);

OPENSSL_EXPORT bcm_status BCM_mldsa65_verify(
    const struct BCM_mldsa65_public_key *public_key,
    const uint8_t signature[BCM_MLDSA65_SIGNATURE_BYTES], const uint8_t *msg,
    size_t msg_len, const uint8_t *context, size_t context_len);

OPENSSL_EXPORT bcm_status BCM_mldsa65_marshal_public_key(
    CBB *out, const struct BCM_mldsa65_public_key *public_key);

OPENSSL_EXPORT bcm_status BCM_mldsa65_parse_public_key(
    struct BCM_mldsa65_public_key *public_key, CBS *in);

OPENSSL_EXPORT bcm_status BCM_mldsa65_parse_private_key(
    struct BCM_mldsa65_private_key *private_key, CBS *in);

OPENSSL_EXPORT bcm_status BCM_mldsa65_generate_key_external_entropy(
    uint8_t out_encoded_public_key[BCM_MLDSA65_PUBLIC_KEY_BYTES],
    struct BCM_mldsa65_private_key *out_private_key,
    const uint8_t entropy[BCM_MLDSA_SEED_BYTES]);

OPENSSL_EXPORT bcm_status BCM_mldsa65_generate_key_external_entropy_fips(
    uint8_t out_encoded_public_key[BCM_MLDSA65_PUBLIC_KEY_BYTES],
    struct BCM_mldsa65_private_key *out_private_key,
    const uint8_t entropy[BCM_MLDSA_SEED_BYTES]);

OPENSSL_EXPORT bcm_status BCM_mldsa65_sign_internal(
    uint8_t out_encoded_signature[BCM_MLDSA65_SIGNATURE_BYTES],
    const struct BCM_mldsa65_private_key *private_key, const uint8_t *msg,
    size_t msg_len, const uint8_t *context_prefix, size_t context_prefix_len,
    const uint8_t *context, size_t context_len,
    const uint8_t randomizer[BCM_MLDSA_SIGNATURE_RANDOMIZER_BYTES]);

OPENSSL_EXPORT bcm_status BCM_mldsa65_verify_internal(
    const struct BCM_mldsa65_public_key *public_key,
    const uint8_t encoded_signature[BCM_MLDSA65_SIGNATURE_BYTES],
    const uint8_t *msg, size_t msg_len, const uint8_t *context_prefix,
    size_t context_prefix_len, const uint8_t *context, size_t context_len);

OPENSSL_EXPORT bcm_status BCM_mldsa65_marshal_private_key(
    CBB *out, const struct BCM_mldsa65_private_key *private_key);

#define BCM_MLDSA87_PRIVATE_KEY_BYTES 4896

#define BCM_MLDSA87_PUBLIC_KEY_BYTES 2592

#define BCM_MLDSA87_SIGNATURE_BYTES 4627

struct BCM_mldsa87_private_key {
  union {
    uint8_t bytes[32 + 32 + 64 + 256 * 4 * (7 + 8 + 8)];
    uint32_t alignment;
  } opaque;
};

struct BCM_mldsa87_public_key {
  union {
    uint8_t bytes[32 + 64 + 256 * 4 * 8];
    uint32_t alignment;
  } opaque;
};

OPENSSL_EXPORT bcm_status BCM_mldsa87_generate_key(
    uint8_t out_encoded_public_key[BCM_MLDSA87_PUBLIC_KEY_BYTES],
    uint8_t out_seed[BCM_MLDSA_SEED_BYTES],
    struct BCM_mldsa87_private_key *out_private_key);

OPENSSL_EXPORT bcm_status BCM_mldsa87_private_key_from_seed(
    struct BCM_mldsa87_private_key *out_private_key,
    const uint8_t seed[BCM_MLDSA_SEED_BYTES]);

OPENSSL_EXPORT bcm_status BCM_mldsa87_public_from_private(
    struct BCM_mldsa87_public_key *out_public_key,
    const struct BCM_mldsa87_private_key *private_key);

OPENSSL_EXPORT bcm_status
BCM_mldsa87_check_key_fips(struct BCM_mldsa87_private_key *private_key);

OPENSSL_EXPORT bcm_status BCM_mldsa87_generate_key_fips(
    uint8_t out_encoded_public_key[BCM_MLDSA87_PUBLIC_KEY_BYTES],
    uint8_t out_seed[BCM_MLDSA_SEED_BYTES],
    struct BCM_mldsa87_private_key *out_private_key);

OPENSSL_EXPORT bcm_status BCM_mldsa87_private_key_from_seed_fips(
    struct BCM_mldsa87_private_key *out_private_key,
    const uint8_t seed[BCM_MLDSA_SEED_BYTES]);

OPENSSL_EXPORT bcm_status BCM_mldsa87_sign(
    uint8_t out_encoded_signature[BCM_MLDSA87_SIGNATURE_BYTES],
    const struct BCM_mldsa87_private_key *private_key, const uint8_t *msg,
    size_t msg_len, const uint8_t *context, size_t context_len);

OPENSSL_EXPORT bcm_status
BCM_mldsa87_verify(const struct BCM_mldsa87_public_key *public_key,
                   const uint8_t *signature, const uint8_t *msg, size_t msg_len,
                   const uint8_t *context, size_t context_len);

OPENSSL_EXPORT bcm_status BCM_mldsa87_marshal_public_key(
    CBB *out, const struct BCM_mldsa87_public_key *public_key);

OPENSSL_EXPORT bcm_status BCM_mldsa87_parse_public_key(
    struct BCM_mldsa87_public_key *public_key, CBS *in);

OPENSSL_EXPORT bcm_status BCM_mldsa87_parse_private_key(
    struct BCM_mldsa87_private_key *private_key, CBS *in);

OPENSSL_EXPORT bcm_status BCM_mldsa87_generate_key_external_entropy(
    uint8_t out_encoded_public_key[BCM_MLDSA87_PUBLIC_KEY_BYTES],
    struct BCM_mldsa87_private_key *out_private_key,
    const uint8_t entropy[BCM_MLDSA_SEED_BYTES]);

OPENSSL_EXPORT bcm_status BCM_mldsa87_generate_key_external_entropy_fips(
    uint8_t out_encoded_public_key[BCM_MLDSA87_PUBLIC_KEY_BYTES],
    struct BCM_mldsa87_private_key *out_private_key,
    const uint8_t entropy[BCM_MLDSA_SEED_BYTES]);

OPENSSL_EXPORT bcm_status BCM_mldsa87_sign_internal(
    uint8_t out_encoded_signature[BCM_MLDSA87_SIGNATURE_BYTES],
    const struct BCM_mldsa87_private_key *private_key, const uint8_t *msg,
    size_t msg_len, const uint8_t *context_prefix, size_t context_prefix_len,
    const uint8_t *context, size_t context_len,
    const uint8_t randomizer[BCM_MLDSA_SIGNATURE_RANDOMIZER_BYTES]);

OPENSSL_EXPORT bcm_status BCM_mldsa87_verify_internal(
    const struct BCM_mldsa87_public_key *public_key,
    const uint8_t encoded_signature[BCM_MLDSA87_SIGNATURE_BYTES],
    const uint8_t *msg, size_t msg_len, const uint8_t *context_prefix,
    size_t context_prefix_len, const uint8_t *context, size_t context_len);

OPENSSL_EXPORT bcm_status BCM_mldsa87_marshal_private_key(
    CBB *out, const struct BCM_mldsa87_private_key *private_key);

#define BCM_MLKEM_ENCAP_ENTROPY 32

#define BCM_MLKEM768_PUBLIC_KEY_BYTES 1184

#define BCM_MLKEM1024_PUBLIC_KEY_BYTES 1568

#define BCM_MLKEM768_CIPHERTEXT_BYTES 1088

#define BCM_MLKEM1024_CIPHERTEXT_BYTES 1568

#define BCM_MLKEM768_PRIVATE_KEY_BYTES 2400

#define BCM_MLKEM1024_PRIVATE_KEY_BYTES 3168

#define BCM_MLKEM_SEED_BYTES 64

#define BCM_MLKEM_SHARED_SECRET_BYTES 32

struct BCM_mlkem768_public_key {
  union {
    uint8_t bytes[512 * (3 + 9) + 32 + 32];
    uint16_t alignment;
  } opaque;
};

struct BCM_mlkem768_private_key {
  union {
    uint8_t bytes[512 * (3 + 3 + 9) + 32 + 32 + 32];
    uint16_t alignment;
  } opaque;
};

OPENSSL_EXPORT bcm_infallible BCM_mlkem768_generate_key(
    uint8_t out_encoded_public_key[BCM_MLKEM768_PUBLIC_KEY_BYTES],
    uint8_t optional_out_seed[BCM_MLKEM_SEED_BYTES],
    struct BCM_mlkem768_private_key *out_private_key);

OPENSSL_EXPORT bcm_status BCM_mlkem768_private_key_from_seed(
    struct BCM_mlkem768_private_key *out_private_key, const uint8_t *seed,
    size_t seed_len);

OPENSSL_EXPORT bcm_status BCM_mlkem768_generate_key_fips(
    uint8_t out_encoded_public_key[BCM_MLKEM768_PUBLIC_KEY_BYTES],
    uint8_t optional_out_seed[BCM_MLKEM_SEED_BYTES],
    struct BCM_mlkem768_private_key *out_private_key);

OPENSSL_EXPORT bcm_status
BCM_mlkem768_check_fips(const struct BCM_mlkem768_private_key *private_key);

OPENSSL_EXPORT bcm_infallible BCM_mlkem768_public_from_private(
    struct BCM_mlkem768_public_key *out_public_key,
    const struct BCM_mlkem768_private_key *private_key);

OPENSSL_EXPORT bcm_infallible
BCM_mlkem768_encap(uint8_t out_ciphertext[BCM_MLKEM768_CIPHERTEXT_BYTES],
                   uint8_t out_shared_secret[BCM_MLKEM_SHARED_SECRET_BYTES],
                   const struct BCM_mlkem768_public_key *public_key);

OPENSSL_EXPORT bcm_status
BCM_mlkem768_decap(uint8_t out_shared_secret[BCM_MLKEM_SHARED_SECRET_BYTES],
                   const uint8_t *ciphertext, size_t ciphertext_len,
                   const struct BCM_mlkem768_private_key *private_key);

OPENSSL_EXPORT bcm_status BCM_mlkem768_marshal_public_key(
    CBB *out, const struct BCM_mlkem768_public_key *public_key);

OPENSSL_EXPORT bcm_status BCM_mlkem768_parse_public_key(
    struct BCM_mlkem768_public_key *out_public_key, CBS *in);

OPENSSL_EXPORT bcm_status BCM_mlkem768_parse_private_key(
    struct BCM_mlkem768_private_key *out_private_key, CBS *in);

OPENSSL_EXPORT bcm_infallible BCM_mlkem768_generate_key_external_seed(
    uint8_t out_encoded_public_key[BCM_MLKEM768_PUBLIC_KEY_BYTES],
    struct BCM_mlkem768_private_key *out_private_key,
    const uint8_t seed[BCM_MLKEM_SEED_BYTES]);

OPENSSL_EXPORT bcm_infallible BCM_mlkem768_encap_external_entropy(
    uint8_t out_ciphertext[BCM_MLKEM768_CIPHERTEXT_BYTES],
    uint8_t out_shared_secret[BCM_MLKEM_SHARED_SECRET_BYTES],
    const struct BCM_mlkem768_public_key *public_key,
    const uint8_t entropy[BCM_MLKEM_ENCAP_ENTROPY]);

OPENSSL_EXPORT bcm_status BCM_mlkem768_marshal_private_key(
    CBB *out, const struct BCM_mlkem768_private_key *private_key);

struct BCM_mlkem1024_public_key {
  union {
    uint8_t bytes[512 * (4 + 16) + 32 + 32];
    uint16_t alignment;
  } opaque;
};

struct BCM_mlkem1024_private_key {
  union {
    uint8_t bytes[512 * (4 + 4 + 16) + 32 + 32 + 32];
    uint16_t alignment;
  } opaque;
};

OPENSSL_EXPORT bcm_infallible BCM_mlkem1024_generate_key(
    uint8_t out_encoded_public_key[BCM_MLKEM1024_PUBLIC_KEY_BYTES],
    uint8_t optional_out_seed[BCM_MLKEM_SEED_BYTES],
    struct BCM_mlkem1024_private_key *out_private_key);

OPENSSL_EXPORT bcm_status BCM_mlkem1024_generate_key_fips(
    uint8_t out_encoded_public_key[BCM_MLKEM1024_PUBLIC_KEY_BYTES],
    uint8_t optional_out_seed[BCM_MLKEM_SEED_BYTES],
    struct BCM_mlkem1024_private_key *out_private_key);

OPENSSL_EXPORT bcm_status
BCM_mlkem1024_check_fips(const struct BCM_mlkem1024_private_key *private_key);

OPENSSL_EXPORT bcm_status BCM_mlkem1024_private_key_from_seed(
    struct BCM_mlkem1024_private_key *out_private_key, const uint8_t *seed,
    size_t seed_len);

OPENSSL_EXPORT bcm_infallible BCM_mlkem1024_public_from_private(
    struct BCM_mlkem1024_public_key *out_public_key,
    const struct BCM_mlkem1024_private_key *private_key);

OPENSSL_EXPORT bcm_infallible
BCM_mlkem1024_encap(uint8_t out_ciphertext[BCM_MLKEM1024_CIPHERTEXT_BYTES],
                    uint8_t out_shared_secret[BCM_MLKEM_SHARED_SECRET_BYTES],
                    const struct BCM_mlkem1024_public_key *public_key);

OPENSSL_EXPORT bcm_status
BCM_mlkem1024_decap(uint8_t out_shared_secret[BCM_MLKEM_SHARED_SECRET_BYTES],
                    const uint8_t *ciphertext, size_t ciphertext_len,
                    const struct BCM_mlkem1024_private_key *private_key);

OPENSSL_EXPORT bcm_status BCM_mlkem1024_marshal_public_key(
    CBB *out, const struct BCM_mlkem1024_public_key *public_key);

OPENSSL_EXPORT bcm_status BCM_mlkem1024_parse_public_key(
    struct BCM_mlkem1024_public_key *out_public_key, CBS *in);

OPENSSL_EXPORT bcm_status BCM_mlkem1024_parse_private_key(
    struct BCM_mlkem1024_private_key *out_private_key, CBS *in);

OPENSSL_EXPORT bcm_infallible BCM_mlkem1024_generate_key_external_seed(
    uint8_t out_encoded_public_key[BCM_MLKEM1024_PUBLIC_KEY_BYTES],
    struct BCM_mlkem1024_private_key *out_private_key,
    const uint8_t seed[BCM_MLKEM_SEED_BYTES]);

OPENSSL_EXPORT bcm_infallible BCM_mlkem1024_encap_external_entropy(
    uint8_t out_ciphertext[BCM_MLKEM1024_CIPHERTEXT_BYTES],
    uint8_t out_shared_secret[BCM_MLKEM_SHARED_SECRET_BYTES],
    const struct BCM_mlkem1024_public_key *public_key,
    const uint8_t entropy[BCM_MLKEM_ENCAP_ENTROPY]);

OPENSSL_EXPORT bcm_status BCM_mlkem1024_marshal_private_key(
    CBB *out, const struct BCM_mlkem1024_private_key *private_key);

#define BCM_SLHDSA_SHA2_128S_N 16

#define BCM_SLHDSA_M_PRIME_HEADER_LEN 2

#define BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES 32

#define BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES 64

#define BCM_SLHDSA_SHA2_128S_SIGNATURE_BYTES 7856

OPENSSL_EXPORT bcm_infallible BCM_slhdsa_sha2_128s_generate_key_from_seed(
    uint8_t out_public_key[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    uint8_t out_secret_key[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES],
    const uint8_t seed[3 * BCM_SLHDSA_SHA2_128S_N]);

OPENSSL_EXPORT bcm_status BCM_slhdsa_sha2_128s_generate_key_from_seed_fips(
    uint8_t out_public_key[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    uint8_t out_secret_key[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES],
    const uint8_t seed[3 * BCM_SLHDSA_SHA2_128S_N]);

OPENSSL_EXPORT bcm_infallible BCM_slhdsa_sha2_128s_sign_internal(
    uint8_t out_signature[BCM_SLHDSA_SHA2_128S_SIGNATURE_BYTES],
    const uint8_t secret_key[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES],
    const uint8_t header[BCM_SLHDSA_M_PRIME_HEADER_LEN], const uint8_t *context,
    size_t context_len, const uint8_t *msg, size_t msg_len,
    const uint8_t entropy[BCM_SLHDSA_SHA2_128S_N]);

OPENSSL_EXPORT bcm_status BCM_slhdsa_sha2_128s_verify_internal(
    const uint8_t *signature, size_t signature_len,
    const uint8_t public_key[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    const uint8_t header[BCM_SLHDSA_M_PRIME_HEADER_LEN], const uint8_t *context,
    size_t context_len, const uint8_t *msg, size_t msg_len);

OPENSSL_EXPORT bcm_infallible BCM_slhdsa_sha2_128s_generate_key(
    uint8_t out_public_key[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    uint8_t out_private_key[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES]);

OPENSSL_EXPORT bcm_status BCM_slhdsa_sha2_128s_generate_key_fips(
    uint8_t out_public_key[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    uint8_t out_private_key[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES]);

OPENSSL_EXPORT bcm_infallible BCM_slhdsa_sha2_128s_public_from_private(
    uint8_t out_public_key[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    const uint8_t private_key[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES]);

OPENSSL_EXPORT bcm_status BCM_slhdsa_sha2_128s_sign(
    uint8_t out_signature[BCM_SLHDSA_SHA2_128S_SIGNATURE_BYTES],
    const uint8_t private_key[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES],
    const uint8_t *msg, size_t msg_len, const uint8_t *context,
    size_t context_len);

OPENSSL_EXPORT bcm_status BCM_slhdsa_sha2_128s_verify(
    const uint8_t *signature, size_t signature_len,
    const uint8_t public_key[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    const uint8_t *msg, size_t msg_len, const uint8_t *context,
    size_t context_len);

OPENSSL_EXPORT bcm_status BCM_slhdsa_sha2_128s_prehash_sign(
    uint8_t out_signature[BCM_SLHDSA_SHA2_128S_SIGNATURE_BYTES],
    const uint8_t private_key[BCM_SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES],
    const uint8_t *hashed_msg, size_t hashed_msg_len, int hash_nid,
    const uint8_t *context, size_t context_len);

OPENSSL_EXPORT bcm_status BCM_slhdsa_sha2_128s_prehash_verify(
    const uint8_t *signature, size_t signature_len,
    const uint8_t public_key[BCM_SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    const uint8_t *hashed_msg, size_t hashed_msg_len, int hash_nid,
    const uint8_t *context, size_t context_len);

OPENSSL_EXPORT bcm_infallible BCM_aes_encrypt(const uint8_t *in, uint8_t *out,
                                              const AES_KEY *key);

OPENSSL_EXPORT bcm_infallible BCM_aes_decrypt(const uint8_t *in, uint8_t *out,
                                              const AES_KEY *key);

OPENSSL_EXPORT bcm_status BCM_aes_set_encrypt_key(const uint8_t *key,
                                                  unsigned bits,
                                                  AES_KEY *aeskey);

OPENSSL_EXPORT bcm_status BCM_aes_set_decrypt_key(const uint8_t *key,
                                                  unsigned bits,
                                                  AES_KEY *aeskey);

#if defined(__cplusplus)
}
#endif

#endif
