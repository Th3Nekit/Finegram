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

#include <openssl/ssl.h>

#include <assert.h>
#include <string.h>

#include <algorithm>
#include <utility>

#include <openssl/aead.h>
#include <openssl/bytestring.h>
#include <openssl/curve25519.h>
#include <openssl/err.h>
#include <openssl/hkdf.h>
#include <openssl/hpke.h>
#include <openssl/rand.h>

#include "../crypto/internal.h"
#include "internal.h"

BSSL_NAMESPACE_BEGIN

static constexpr uint16_t kECHConfigVersion =
    TLSEXT_TYPE_encrypted_client_hello;

static const decltype(&EVP_hpke_aes_128_gcm) kSupportedAEADs[] = {
    &EVP_hpke_aes_128_gcm,
    &EVP_hpke_aes_256_gcm,
    &EVP_hpke_chacha20_poly1305,
};

static const EVP_HPKE_AEAD *get_ech_aead(uint16_t aead_id) {
  for (const auto aead_func : kSupportedAEADs) {
    const EVP_HPKE_AEAD *aead = aead_func();
    if (aead_id == EVP_HPKE_AEAD_id(aead)) {
      return aead;
    }
  }
  return nullptr;
}

static bool ssl_client_hello_write_without_extensions(
    const SSL_CLIENT_HELLO *client_hello, CBB *out) {
  CBB cbb;
  if (!CBB_add_u16(out, client_hello->version) ||
      !CBB_add_bytes(out, client_hello->random, client_hello->random_len) ||
      !CBB_add_u8_length_prefixed(out, &cbb) ||
      !CBB_add_bytes(&cbb, client_hello->session_id,
                     client_hello->session_id_len)) {
    return false;
  }
  if (SSL_is_dtls(client_hello->ssl)) {
    if (!CBB_add_u8_length_prefixed(out, &cbb) ||
        !CBB_add_bytes(&cbb, client_hello->dtls_cookie,
                       client_hello->dtls_cookie_len)) {
      return false;
    }
  }
  if (!CBB_add_u16_length_prefixed(out, &cbb) ||
      !CBB_add_bytes(&cbb, client_hello->cipher_suites,
                     client_hello->cipher_suites_len) ||
      !CBB_add_u8_length_prefixed(out, &cbb) ||
      !CBB_add_bytes(&cbb, client_hello->compression_methods,
                     client_hello->compression_methods_len) ||
      !CBB_flush(out)) {
    return false;
  }
  return true;
}

static bool is_valid_client_hello_inner(SSL *ssl, uint8_t *out_alert,
                                        Span<const uint8_t> body) {

  SSL_CLIENT_HELLO client_hello;
  CBS extension;
  if (!SSL_parse_client_hello(ssl, &client_hello, body.data(), body.size()) ||
      !ssl_client_hello_get_extension(&client_hello, &extension,
                                      TLSEXT_TYPE_encrypted_client_hello) ||
      CBS_len(&extension) != 1 ||
      CBS_data(&extension)[0] != ECH_CLIENT_INNER ||
      !ssl_client_hello_get_extension(&client_hello, &extension,
                                      TLSEXT_TYPE_supported_versions)) {
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_CLIENT_HELLO_INNER);
    return false;
  }

  CBS versions;
  if (!CBS_get_u8_length_prefixed(&extension, &versions) ||
      CBS_len(&extension) != 0 ||
      CBS_len(&versions) == 0) {
    *out_alert = SSL_AD_DECODE_ERROR;
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }
  while (CBS_len(&versions) != 0) {
    uint16_t version;
    if (!CBS_get_u16(&versions, &version)) {
      *out_alert = SSL_AD_DECODE_ERROR;
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      return false;
    }
    if (version == SSL3_VERSION || version == TLS1_VERSION ||
        version == TLS1_1_VERSION || version == TLS1_2_VERSION ||
        version == DTLS1_VERSION || version == DTLS1_2_VERSION) {
      *out_alert = SSL_AD_ILLEGAL_PARAMETER;
      OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_CLIENT_HELLO_INNER);
      return false;
    }
  }
  return true;
}

bool ssl_decode_client_hello_inner(
    SSL *ssl, uint8_t *out_alert, Array<uint8_t> *out_client_hello_inner,
    Span<const uint8_t> encoded_client_hello_inner,
    const SSL_CLIENT_HELLO *client_hello_outer) {
  SSL_CLIENT_HELLO client_hello_inner;
  CBS cbs = encoded_client_hello_inner;
  if (!ssl_parse_client_hello_with_trailing_data(ssl, &cbs,
                                                 &client_hello_inner)) {
    return false;
  }

  uint8_t padding;
  while (CBS_get_u8(&cbs, &padding)) {
    if (padding != 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      *out_alert = SSL_AD_ILLEGAL_PARAMETER;
      return false;
    }
  }

  if (client_hello_inner.extensions_len == 0 ||
      client_hello_inner.session_id_len != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }
  client_hello_inner.session_id = client_hello_outer->session_id;
  client_hello_inner.session_id_len = client_hello_outer->session_id_len;

  ScopedCBB cbb;
  CBB body, extensions_cbb;
  if (!ssl->method->init_message(ssl, cbb.get(), &body, SSL3_MT_CLIENT_HELLO) ||
      !ssl_client_hello_write_without_extensions(&client_hello_inner, &body) ||
      !CBB_add_u16_length_prefixed(&body, &extensions_cbb)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  auto inner_extensions =
      Span(client_hello_inner.extensions, client_hello_inner.extensions_len);
  CBS ext_list_wrapper;
  if (!ssl_client_hello_get_extension(&client_hello_inner, &ext_list_wrapper,
                                      TLSEXT_TYPE_ech_outer_extensions)) {

    if (!CBB_add_bytes(&extensions_cbb, inner_extensions.data(),
                       inner_extensions.size())) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return false;
    }
  } else {
    const size_t offset = CBS_data(&ext_list_wrapper) - inner_extensions.data();
    auto inner_extensions_before =
        inner_extensions.subspan(0, offset - 4                       );
    auto inner_extensions_after =
        inner_extensions.subspan(offset + CBS_len(&ext_list_wrapper));
    if (!CBB_add_bytes(&extensions_cbb, inner_extensions_before.data(),
                       inner_extensions_before.size())) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return false;
    }

    CBS ext_list;
    if (!CBS_get_u8_length_prefixed(&ext_list_wrapper, &ext_list) ||
        CBS_len(&ext_list) == 0 || CBS_len(&ext_list_wrapper) != 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      return false;
    }
    CBS outer_extensions;
    CBS_init(&outer_extensions, client_hello_outer->extensions,
             client_hello_outer->extensions_len);
    while (CBS_len(&ext_list) != 0) {

      uint16_t want;
      if (!CBS_get_u16(&ext_list, &want)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
        return false;
      }

      if (want == TLSEXT_TYPE_encrypted_client_hello) {
        *out_alert = SSL_AD_ILLEGAL_PARAMETER;
        OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_OUTER_EXTENSION);
        return false;
      }

      uint16_t found;
      CBS ext_body;
      do {
        if (CBS_len(&outer_extensions) == 0) {
          *out_alert = SSL_AD_ILLEGAL_PARAMETER;
          OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_OUTER_EXTENSION);
          return false;
        }
        if (!CBS_get_u16(&outer_extensions, &found) ||
            !CBS_get_u16_length_prefixed(&outer_extensions, &ext_body)) {
          OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
          return false;
        }
      } while (found != want);

      if (!CBB_add_u16(&extensions_cbb, found) ||
          !CBB_add_u16(&extensions_cbb, CBS_len(&ext_body)) ||
          !CBB_add_bytes(&extensions_cbb, CBS_data(&ext_body),
                         CBS_len(&ext_body))) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
        return false;
      }
    }

    if (!CBB_add_bytes(&extensions_cbb, inner_extensions_after.data(),
                       inner_extensions_after.size())) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return false;
    }
  }
  if (!CBB_flush(&body)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  if (!is_valid_client_hello_inner(ssl, out_alert,
                                   Span(CBB_data(&body), CBB_len(&body)))) {
    return false;
  }

  if (!ssl->method->finish_message(ssl, cbb.get(), out_client_hello_inner)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }
  return true;
}

bool ssl_client_hello_decrypt(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                              bool *out_is_decrypt_error, Array<uint8_t> *out,
                              const SSL_CLIENT_HELLO *client_hello_outer,
                              Span<const uint8_t> payload) {
  *out_is_decrypt_error = false;

  Array<uint8_t> aad;
  if (!aad.CopyFrom(Span(client_hello_outer->client_hello,
                         client_hello_outer->client_hello_len))) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return false;
  }

  assert(reinterpret_cast<uintptr_t>(client_hello_outer->extensions) <=
         reinterpret_cast<uintptr_t>(payload.data()));
  assert(reinterpret_cast<uintptr_t>(client_hello_outer->extensions +
                                     client_hello_outer->extensions_len) >=
         reinterpret_cast<uintptr_t>(payload.data() + payload.size()));
  Span<uint8_t> payload_aad = Span(aad).subspan(
      payload.data() - client_hello_outer->client_hello, payload.size());
  OPENSSL_memset(payload_aad.data(), 0, payload_aad.size());

  Array<uint8_t> encoded;
  if (CRYPTO_fuzzer_mode_enabled()) {

    const uint8_t kBadPayload[] = {0xff};
    if (payload == kBadPayload) {
      *out_alert = SSL_AD_DECRYPT_ERROR;
      *out_is_decrypt_error = true;
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECRYPTION_FAILED);
      return false;
    }
    if (!encoded.CopyFrom(payload)) {
      *out_alert = SSL_AD_INTERNAL_ERROR;
      return false;
    }
  } else {
    if (!encoded.InitForOverwrite(payload.size())) {
      *out_alert = SSL_AD_INTERNAL_ERROR;
      return false;
    }
    size_t len;
    if (!EVP_HPKE_CTX_open(hs->ech_hpke_ctx.get(), encoded.data(), &len,
                           encoded.size(), payload.data(), payload.size(),
                           aad.data(), aad.size())) {
      *out_alert = SSL_AD_DECRYPT_ERROR;
      *out_is_decrypt_error = true;
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECRYPTION_FAILED);
      return false;
    }
    encoded.Shrink(len);
  }

  if (!ssl_decode_client_hello_inner(hs->ssl, out_alert, out, encoded,
                                     client_hello_outer)) {
    return false;
  }

  ssl_do_msg_callback(hs->ssl,              0, SSL3_RT_CLIENT_HELLO_INNER,
                      *out);
  return true;
}

static bool is_hex_component(Span<const uint8_t> in) {
  if (in.size() < 2 || in[0] != '0' || (in[1] != 'x' && in[1] != 'X')) {
    return false;
  }
  for (uint8_t b : in.subspan(2)) {
    if (!OPENSSL_isxdigit(b)) {
      return false;
    }
  }
  return true;
}

static bool is_decimal_component(Span<const uint8_t> in) {
  if (in.empty()) {
    return false;
  }
  for (uint8_t b : in) {
    if (!('0' <= b && b <= '9')) {
      return false;
    }
  }
  return true;
}

bool ssl_is_valid_ech_public_name(Span<const uint8_t> public_name) {

  auto remaining = public_name;
  if (remaining.empty()) {
    return false;
  }
  Span<const uint8_t> last;
  while (!remaining.empty()) {

    auto dot = std::find(remaining.begin(), remaining.end(), '.');
    Span<const uint8_t> component;
    if (dot == remaining.end()) {
      component = remaining;
      last = component;
      remaining = Span<const uint8_t>();
    } else {
      component = remaining.subspan(0, dot - remaining.begin());

      remaining = remaining.subspan(dot - remaining.begin() + 1);
      if (remaining.empty()) {

        return false;
      }
    }

    if (component.empty() || component.size() > 63 ||
        component.front() == '-' || component.back() == '-') {
      return false;
    }
    for (uint8_t c : component) {
      if (!OPENSSL_isalnum(c) && c != '-') {
        return false;
      }
    }
  }

  return !is_hex_component(last) && !is_decimal_component(last);
}

static bool parse_ech_config(CBS *cbs, ECHConfig *out, bool *out_supported,
                             bool all_extensions_mandatory) {
  uint16_t version;
  CBS orig = *cbs;
  CBS contents;
  if (!CBS_get_u16(cbs, &version) ||
      !CBS_get_u16_length_prefixed(cbs, &contents)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }

  if (version != kECHConfigVersion) {
    *out_supported = false;
    return true;
  }

  if (!out->raw.CopyFrom(
          Span(CBS_data(&orig), CBS_len(&orig) - CBS_len(cbs)))) {
    return false;
  }

  CBS ech_config(out->raw);
  CBS public_name, public_key, cipher_suites, extensions;
  if (!CBS_skip(&ech_config, 2) ||
      !CBS_get_u16_length_prefixed(&ech_config, &contents) ||
      !CBS_get_u8(&contents, &out->config_id) ||
      !CBS_get_u16(&contents, &out->kem_id) ||
      !CBS_get_u16_length_prefixed(&contents, &public_key) ||
      CBS_len(&public_key) == 0 ||
      !CBS_get_u16_length_prefixed(&contents, &cipher_suites) ||
      CBS_len(&cipher_suites) == 0 || CBS_len(&cipher_suites) % 4 != 0 ||
      !CBS_get_u8(&contents, &out->maximum_name_length) ||
      !CBS_get_u8_length_prefixed(&contents, &public_name) ||
      CBS_len(&public_name) == 0 ||
      !CBS_get_u16_length_prefixed(&contents, &extensions) ||
      CBS_len(&contents) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }

  if (!ssl_is_valid_ech_public_name(public_name)) {

    *out_supported = false;
    return true;
  }

  out->public_key = public_key;
  out->public_name = public_name;

  out->cipher_suites = cipher_suites;

  bool has_unknown_mandatory_extension = false;
  while (CBS_len(&extensions) != 0) {
    uint16_t type;
    CBS body;
    if (!CBS_get_u16(&extensions, &type) ||
        !CBS_get_u16_length_prefixed(&extensions, &body)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      return false;
    }

    if (type & 0x8000 || all_extensions_mandatory) {

      has_unknown_mandatory_extension = true;
    }
  }

  *out_supported = !has_unknown_mandatory_extension;
  return true;
}

bool ECHServerConfig::Init(Span<const uint8_t> ech_config,
                           const EVP_HPKE_KEY *key, bool is_retry_config) {
  is_retry_config_ = is_retry_config;

  CBS cbs = ech_config;
  bool supported;
  if (!parse_ech_config(&cbs, &ech_config_, &supported,
                                                     true)) {
    return false;
  }
  if (CBS_len(&cbs) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }
  if (!supported) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNSUPPORTED_ECH_SERVER_CONFIG);
    return false;
  }

  CBS cipher_suites = ech_config_.cipher_suites;
  while (CBS_len(&cipher_suites) > 0) {
    uint16_t kdf_id, aead_id;
    if (!CBS_get_u16(&cipher_suites, &kdf_id) ||
        !CBS_get_u16(&cipher_suites, &aead_id)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      return false;
    }

    if (kdf_id != EVP_HPKE_HKDF_SHA256 || get_ech_aead(aead_id) == nullptr) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_UNSUPPORTED_ECH_SERVER_CONFIG);
      return false;
    }
  }

  uint8_t expected_public_key[EVP_HPKE_MAX_PUBLIC_KEY_LENGTH];
  size_t expected_public_key_len;
  if (!EVP_HPKE_KEY_public_key(key, expected_public_key,
                               &expected_public_key_len,
                               sizeof(expected_public_key))) {
    return false;
  }
  if (ech_config_.kem_id != EVP_HPKE_KEM_id(EVP_HPKE_KEY_kem(key)) ||
      Span(expected_public_key, expected_public_key_len) !=
          ech_config_.public_key) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_ECH_SERVER_CONFIG_AND_PRIVATE_KEY_MISMATCH);
    return false;
  }

  if (!EVP_HPKE_KEY_copy(key_.get(), key)) {
    return false;
  }

  return true;
}

bool ECHServerConfig::SetupContext(EVP_HPKE_CTX *ctx, uint16_t kdf_id,
                                   uint16_t aead_id,
                                   Span<const uint8_t> enc) const {

  CBS cbs(ech_config_.cipher_suites);
  bool cipher_ok = false;
  while (CBS_len(&cbs) != 0) {
    uint16_t supported_kdf_id, supported_aead_id;
    if (!CBS_get_u16(&cbs, &supported_kdf_id) ||
        !CBS_get_u16(&cbs, &supported_aead_id)) {
      return false;
    }
    if (kdf_id == supported_kdf_id && aead_id == supported_aead_id) {
      cipher_ok = true;
      break;
    }
  }
  if (!cipher_ok) {
    return false;
  }

  static const uint8_t kInfoLabel[] = "tls ech";
  ScopedCBB info_cbb;
  if (!CBB_init(info_cbb.get(), sizeof(kInfoLabel) + ech_config_.raw.size()) ||
      !CBB_add_bytes(info_cbb.get(), kInfoLabel,
                     sizeof(kInfoLabel)                            ) ||
      !CBB_add_bytes(info_cbb.get(), ech_config_.raw.data(),
                     ech_config_.raw.size())) {
    return false;
  }

  assert(kdf_id == EVP_HPKE_HKDF_SHA256);
  assert(get_ech_aead(aead_id) != NULL);
  return EVP_HPKE_CTX_setup_recipient(ctx, key_.get(), EVP_hpke_hkdf_sha256(),
                                      get_ech_aead(aead_id), enc.data(),
                                      enc.size(), CBB_data(info_cbb.get()),
                                      CBB_len(info_cbb.get()));
}

bool ssl_is_valid_ech_config_list(Span<const uint8_t> ech_config_list) {
  CBS cbs = ech_config_list, child;
  if (!CBS_get_u16_length_prefixed(&cbs, &child) ||
      CBS_len(&child) == 0 ||
      CBS_len(&cbs) > 0) {
    return false;
  }
  while (CBS_len(&child) > 0) {
    ECHConfig ech_config;
    bool supported;
    if (!parse_ech_config(&child, &ech_config, &supported,
                                                       false)) {
      return false;
    }
  }
  return true;
}

static bool select_ech_cipher_suite(const EVP_HPKE_KDF **out_kdf,
                                    const EVP_HPKE_AEAD **out_aead,
                                    Span<const uint8_t> cipher_suites,
                                    const bool has_aes_hardware) {
  const EVP_HPKE_AEAD *aead = nullptr;
  CBS cbs = cipher_suites;
  while (CBS_len(&cbs) != 0) {
    uint16_t kdf_id, aead_id;
    if (!CBS_get_u16(&cbs, &kdf_id) ||
        !CBS_get_u16(&cbs, &aead_id)) {
      return false;
    }

    const EVP_HPKE_AEAD *candidate = get_ech_aead(aead_id);
    if (kdf_id != EVP_HPKE_HKDF_SHA256 || candidate == nullptr) {
      continue;
    }
    if (aead == nullptr ||
        (!has_aes_hardware && aead_id == EVP_HPKE_CHACHA20_POLY1305)) {
      aead = candidate;
    }
  }
  if (aead == nullptr) {
    return false;
  }

  *out_kdf = EVP_hpke_hkdf_sha256();
  *out_aead = aead;
  return true;
}

bool ssl_select_ech_config(SSL_HANDSHAKE *hs, Span<uint8_t> out_enc,
                           size_t *out_enc_len) {
  *out_enc_len = 0;
  if (hs->max_version < TLS1_3_VERSION) {

    return true;
  }

  if (!hs->config->client_ech_config_list.empty()) {
    CBS cbs = CBS(hs->config->client_ech_config_list);
    CBS child;
    if (!CBS_get_u16_length_prefixed(&cbs, &child) ||
        CBS_len(&child) == 0 ||
        CBS_len(&cbs) > 0) {
      return false;
    }

    while (CBS_len(&child) > 0) {
      ECHConfig ech_config;
      bool supported;
      if (!parse_ech_config(&child, &ech_config, &supported,
                                                         false)) {
        return false;
      }
      const EVP_HPKE_KEM *kem = EVP_hpke_x25519_hkdf_sha256();
      const EVP_HPKE_KDF *kdf;
      const EVP_HPKE_AEAD *aead;
      if (supported &&
          ech_config.kem_id == EVP_HPKE_DHKEM_X25519_HKDF_SHA256 &&
          select_ech_cipher_suite(&kdf, &aead, ech_config.cipher_suites,
                                  hs->ssl->config->aes_hw_override
                                      ? hs->ssl->config->aes_hw_override_value
                                      : EVP_has_aes_hardware())) {
        ScopedCBB info;
        static const uint8_t kInfoLabel[] = "tls ech";
        if (!CBB_init(info.get(), sizeof(kInfoLabel) + ech_config.raw.size()) ||
            !CBB_add_bytes(info.get(), kInfoLabel, sizeof(kInfoLabel)) ||
            !CBB_add_bytes(info.get(), ech_config.raw.data(),
                           ech_config.raw.size())) {
          return false;
        }

        if (!EVP_HPKE_CTX_setup_sender(
                hs->ech_hpke_ctx.get(), out_enc.data(), out_enc_len,
                out_enc.size(), kem, kdf, aead, ech_config.public_key.data(),
                ech_config.public_key.size(), CBB_data(info.get()),
                CBB_len(info.get())) ||
            !hs->inner_transcript.Init()) {
          return false;
        }

        hs->selected_ech_config = MakeUnique<ECHConfig>(std::move(ech_config));
        return hs->selected_ech_config != nullptr;
      }
    }
  }

  return true;
}

static size_t aead_overhead(const EVP_HPKE_AEAD *aead) {
  if (CRYPTO_fuzzer_mode_enabled()) {

    return 0;
  }
  return EVP_AEAD_max_overhead(EVP_HPKE_AEAD_aead(aead));
}

static size_t random_size(size_t min, size_t max) {
  assert(min < max);
  size_t value;
  RAND_bytes(reinterpret_cast<uint8_t *>(&value), sizeof(value));
  return value % (max - min + 1) + min;
}

static bool setup_ech_grease(SSL_HANDSHAKE *hs) {
  assert(!hs->selected_ech_config);
  if (hs->max_version < TLS1_3_VERSION || !hs->config->ech_grease_enabled) {
    return true;
  }

  const uint16_t kdf_id = EVP_HPKE_HKDF_SHA256;
  const bool has_aes_hw = hs->ssl->config->aes_hw_override
                              ? hs->ssl->config->aes_hw_override_value
                              : EVP_has_aes_hardware();
  const EVP_HPKE_AEAD *aead =
      has_aes_hw ? EVP_hpke_aes_128_gcm() : EVP_hpke_chacha20_poly1305();
  static_assert(ssl_grease_ech_config_id < sizeof(hs->grease_seed),
                "hs->grease_seed is too small");
  uint8_t config_id = hs->grease_seed[ssl_grease_ech_config_id];

  uint8_t enc[X25519_PUBLIC_VALUE_LEN];
  uint8_t private_key_unused[X25519_PRIVATE_KEY_LEN];
  X25519_keypair(enc, private_key_unused);

  const size_t payload_len =
      32 * random_size(128 / 32, 224 / 32) + aead_overhead(aead);
  bssl::ScopedCBB cbb;
  CBB enc_cbb, payload_cbb;
  uint8_t *payload;
  if (!CBB_init(cbb.get(), 256) || !CBB_add_u16(cbb.get(), kdf_id) ||
      !CBB_add_u16(cbb.get(), EVP_HPKE_AEAD_id(aead)) ||
      !CBB_add_u8(cbb.get(), config_id) ||
      !CBB_add_u16_length_prefixed(cbb.get(), &enc_cbb) ||
      !CBB_add_bytes(&enc_cbb, enc, sizeof(enc)) ||
      !CBB_add_u16_length_prefixed(cbb.get(), &payload_cbb) ||
      !CBB_add_space(&payload_cbb, &payload, payload_len) ||
      !RAND_bytes(payload, payload_len) ||
      !CBBFinishArray(cbb.get(), &hs->ech_client_outer)) {
    return false;
  }
  return true;
}

bool ssl_encrypt_client_hello(SSL_HANDSHAKE *hs, Span<const uint8_t> enc) {
  SSL *const ssl = hs->ssl;
  if (!hs->selected_ech_config) {
    return setup_ech_grease(hs);
  }

  ScopedCBB cbb, encoded_cbb;
  CBB body;
  bool needs_psk_binder;
  Array<uint8_t> hello_inner;
  if (!ssl->method->init_message(ssl, cbb.get(), &body, SSL3_MT_CLIENT_HELLO) ||
      !CBB_init(encoded_cbb.get(), 256) ||
      !ssl_write_client_hello_without_extensions(hs, &body,
                                                 ssl_client_hello_inner,
                                                                      false) ||
      !ssl_write_client_hello_without_extensions(hs, encoded_cbb.get(),
                                                 ssl_client_hello_inner,
                                                                      true) ||
      !ssl_add_clienthello_tlsext(hs, &body, encoded_cbb.get(),
                                  &needs_psk_binder, ssl_client_hello_inner,
                                  CBB_len(&body)) ||
      !ssl->method->finish_message(ssl, cbb.get(), &hello_inner)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  if (needs_psk_binder) {
    size_t binder_len;
    if (!tls13_write_psk_binder(hs, hs->inner_transcript, Span(hello_inner),
                                &binder_len)) {
      return false;
    }

    auto encoded_binder =
        Span(const_cast<uint8_t *>(CBB_data(encoded_cbb.get())),
             CBB_len(encoded_cbb.get()))
            .last(binder_len);
    auto hello_inner_binder = Span(hello_inner).last(binder_len);
    OPENSSL_memcpy(encoded_binder.data(), hello_inner_binder.data(),
                   binder_len);
  }

  ssl_do_msg_callback(ssl,              1, SSL3_RT_CLIENT_HELLO_INNER,
                      hello_inner);
  if (!hs->inner_transcript.Update(hello_inner)) {
    return false;
  }

  size_t padding_len = 0;
  size_t maximum_name_length = hs->selected_ech_config->maximum_name_length;
  if (ssl->hostname) {
    size_t hostname_len = strlen(ssl->hostname.get());
    if (hostname_len <= maximum_name_length) {
      padding_len = maximum_name_length - hostname_len;
    }
  } else {

    padding_len = 9 + maximum_name_length;
  }

  padding_len += 31 - ((CBB_len(encoded_cbb.get()) + padding_len - 1) % 32);
  Array<uint8_t> encoded;
  if (!CBB_add_zeros(encoded_cbb.get(), padding_len) ||
      !CBBFinishArray(encoded_cbb.get(), &encoded)) {
    return false;
  }

  const EVP_HPKE_KDF *kdf = EVP_HPKE_CTX_kdf(hs->ech_hpke_ctx.get());
  const EVP_HPKE_AEAD *aead = EVP_HPKE_CTX_aead(hs->ech_hpke_ctx.get());
  size_t payload_len = encoded.size() + aead_overhead(aead);
  CBB enc_cbb, payload_cbb;
  if (!CBB_init(cbb.get(), 256) ||
      !CBB_add_u16(cbb.get(), EVP_HPKE_KDF_id(kdf)) ||
      !CBB_add_u16(cbb.get(), EVP_HPKE_AEAD_id(aead)) ||
      !CBB_add_u8(cbb.get(), hs->selected_ech_config->config_id) ||
      !CBB_add_u16_length_prefixed(cbb.get(), &enc_cbb) ||
      !CBB_add_bytes(&enc_cbb, enc.data(), enc.size()) ||
      !CBB_add_u16_length_prefixed(cbb.get(), &payload_cbb) ||
      !CBB_add_zeros(&payload_cbb, payload_len) ||
      !CBBFinishArray(cbb.get(), &hs->ech_client_outer)) {
    return false;
  }

  bssl::ScopedCBB aad;
  if (!CBB_init(aad.get(), 256) ||
      !ssl_write_client_hello_without_extensions(hs, aad.get(),
                                                 ssl_client_hello_outer,
                                                                      false) ||
      !ssl_add_clienthello_tlsext(hs, aad.get(),                 nullptr,
                                  &needs_psk_binder, ssl_client_hello_outer,
                                  CBB_len(aad.get()))) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  assert(!needs_psk_binder);

  auto payload_span = Span(hs->ech_client_outer).last(payload_len);
  if (CRYPTO_fuzzer_mode_enabled()) {

    assert(payload_span.size() == encoded.size());
    OPENSSL_memcpy(payload_span.data(), encoded.data(), encoded.size());
  } else {
    if (!EVP_HPKE_CTX_seal(hs->ech_hpke_ctx.get(), payload_span.data(),
                           &payload_len, payload_span.size(), encoded.data(),
                           encoded.size(), CBB_data(aad.get()),
                           CBB_len(aad.get())) ||
        payload_len != payload_span.size()) {
      return false;
    }
  }

  return true;
}

BSSL_NAMESPACE_END

using namespace bssl;

void SSL_set_enable_ech_grease(SSL *ssl, int enable) {
  if (!ssl->config) {
    return;
  }
  ssl->config->ech_grease_enabled = !!enable;
}

int SSL_set1_ech_config_list(SSL *ssl, const uint8_t *ech_config_list,
                             size_t ech_config_list_len) {
  if (!ssl->config) {
    return 0;
  }

  auto span = Span(ech_config_list, ech_config_list_len);
  if (!ssl_is_valid_ech_config_list(span)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_ECH_CONFIG_LIST);
    return 0;
  }
  return ssl->config->client_ech_config_list.CopyFrom(span);
}

void SSL_get0_ech_name_override(const SSL *ssl, const char **out_name,
                                size_t *out_name_len) {

  const SSL_HANDSHAKE *hs = ssl->s3->hs.get();
  if (!ssl->server && hs && ssl->s3->ech_status == ssl_ech_rejected) {
    *out_name = reinterpret_cast<const char *>(
        hs->selected_ech_config->public_name.data());
    *out_name_len = hs->selected_ech_config->public_name.size();
  } else {
    *out_name = nullptr;
    *out_name_len = 0;
  }
}

void SSL_get0_ech_retry_configs(const SSL *ssl,
                                const uint8_t **out_retry_configs,
                                size_t *out_retry_configs_len) {
  const SSL_HANDSHAKE *hs = ssl->s3->hs.get();
  if (!hs || !hs->ech_authenticated_reject) {

    assert(0);
    static const uint8_t kPlaceholder[] = {
        kECHConfigVersion >> 8, kECHConfigVersion & 0xff, 0xff, 0xff, 0xff};
    *out_retry_configs = kPlaceholder;
    *out_retry_configs_len = sizeof(kPlaceholder);
    return;
  }

  *out_retry_configs = hs->ech_retry_configs.data();
  *out_retry_configs_len = hs->ech_retry_configs.size();
}

int SSL_marshal_ech_config(uint8_t **out, size_t *out_len, uint8_t config_id,
                           const EVP_HPKE_KEY *key, const char *public_name,
                           size_t max_name_len) {
  Span<const uint8_t> public_name_u8 = StringAsBytes(public_name);
  if (!ssl_is_valid_ech_public_name(public_name_u8)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_ECH_PUBLIC_NAME);
    return 0;
  }

  if (max_name_len > 0xff) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_LENGTH);
    return 0;
  }

  ScopedCBB cbb;
  CBB contents, child;
  uint8_t *public_key;
  size_t public_key_len;
  if (!CBB_init(cbb.get(), 128) ||
      !CBB_add_u16(cbb.get(), kECHConfigVersion) ||
      !CBB_add_u16_length_prefixed(cbb.get(), &contents) ||
      !CBB_add_u8(&contents, config_id) ||
      !CBB_add_u16(&contents, EVP_HPKE_KEM_id(EVP_HPKE_KEY_kem(key))) ||
      !CBB_add_u16_length_prefixed(&contents, &child) ||
      !CBB_reserve(&child, &public_key, EVP_HPKE_MAX_PUBLIC_KEY_LENGTH) ||
      !EVP_HPKE_KEY_public_key(key, public_key, &public_key_len,
                               EVP_HPKE_MAX_PUBLIC_KEY_LENGTH) ||
      !CBB_did_write(&child, public_key_len) ||
      !CBB_add_u16_length_prefixed(&contents, &child) ||

      !CBB_add_u16(&child, EVP_HPKE_HKDF_SHA256) ||
      !CBB_add_u16(&child, EVP_HPKE_AES_128_GCM) ||
      !CBB_add_u16(&child, EVP_HPKE_HKDF_SHA256) ||
      !CBB_add_u16(&child, EVP_HPKE_CHACHA20_POLY1305) ||
      !CBB_add_u8(&contents, max_name_len) ||
      !CBB_add_u8_length_prefixed(&contents, &child) ||
      !CBB_add_bytes(&child, public_name_u8.data(), public_name_u8.size()) ||

      !CBB_add_u16(&contents, 0                    ) ||
      !CBB_finish(cbb.get(), out, out_len)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }
  return 1;
}

SSL_ECH_KEYS *SSL_ECH_KEYS_new() { return New<SSL_ECH_KEYS>(); }

void SSL_ECH_KEYS_up_ref(SSL_ECH_KEYS *keys) { keys->UpRefInternal(); }

void SSL_ECH_KEYS_free(SSL_ECH_KEYS *keys) {
  if (keys != nullptr) {
    keys->DecRefInternal();
  }
}

int SSL_ECH_KEYS_add(SSL_ECH_KEYS *configs, int is_retry_config,
                     const uint8_t *ech_config, size_t ech_config_len,
                     const EVP_HPKE_KEY *key) {
  UniquePtr<ECHServerConfig> parsed_config = MakeUnique<ECHServerConfig>();
  if (!parsed_config) {
    return 0;
  }
  if (!parsed_config->Init(Span(ech_config, ech_config_len), key,
                           !!is_retry_config)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return 0;
  }
  if (!configs->configs.Push(std::move(parsed_config))) {
    return 0;
  }
  return 1;
}

int SSL_ECH_KEYS_has_duplicate_config_id(const SSL_ECH_KEYS *keys) {
  bool seen[256] = {false};
  for (const auto &config : keys->configs) {
    if (seen[config->ech_config().config_id]) {
      return 1;
    }
    seen[config->ech_config().config_id] = true;
  }
  return 0;
}

int SSL_ECH_KEYS_marshal_retry_configs(const SSL_ECH_KEYS *keys, uint8_t **out,
                                       size_t *out_len) {
  ScopedCBB cbb;
  CBB child;
  if (!CBB_init(cbb.get(), 128) ||
      !CBB_add_u16_length_prefixed(cbb.get(), &child)) {
    return false;
  }
  for (const auto &config : keys->configs) {
    if (config->is_retry_config() &&
        !CBB_add_bytes(&child, config->ech_config().raw.data(),
                       config->ech_config().raw.size())) {
      return false;
    }
  }
  return CBB_finish(cbb.get(), out, out_len);
}

int SSL_CTX_set1_ech_keys(SSL_CTX *ctx, SSL_ECH_KEYS *keys) {
  bool has_retry_config = false;
  for (const auto &config : keys->configs) {
    if (config->is_retry_config()) {
      has_retry_config = true;
      break;
    }
  }
  if (!has_retry_config) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_ECH_SERVER_WOULD_HAVE_NO_RETRY_CONFIGS);
    return 0;
  }
  UniquePtr<SSL_ECH_KEYS> owned_keys = UpRef(keys);
  MutexWriteLock lock(&ctx->lock);
  ctx->ech_keys.swap(owned_keys);
  return 1;
}

int SSL_ech_accepted(const SSL *ssl) {
  if (SSL_in_early_data(ssl) && !ssl->server) {

    return ssl->s3->hs->selected_ech_config != nullptr;
  }

  return ssl->s3->ech_status == ssl_ech_accepted;
}
