// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
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
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <utility>

#include <openssl/aead.h>
#include <openssl/bytestring.h>
#include <openssl/chacha.h>
#include <openssl/curve25519.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/hpke.h>
#include <openssl/mem.h>
#include <openssl/nid.h>
#include <openssl/rand.h>

#include "../crypto/internal.h"
#include "../crypto/spake2plus/internal.h"
#include "internal.h"

BSSL_NAMESPACE_BEGIN

static bool ssl_check_clienthello_tlsext(SSL_HANDSHAKE *hs);
static bool ssl_check_serverhello_tlsext(SSL_HANDSHAKE *hs);

static int compare_uint16_t(const void *p1, const void *p2) {
  uint16_t u1 = *((const uint16_t *)p1);
  uint16_t u2 = *((const uint16_t *)p2);
  if (u1 < u2) {
    return -1;
  } else if (u1 > u2) {
    return 1;
  } else {
    return 0;
  }
}

static bool tls1_check_duplicate_extensions(const CBS *cbs) {

  size_t num_extensions = 0;
  CBS extensions = *cbs;
  while (CBS_len(&extensions) > 0) {
    uint16_t type;
    CBS extension;

    if (!CBS_get_u16(&extensions, &type) ||
        !CBS_get_u16_length_prefixed(&extensions, &extension)) {
      return false;
    }

    num_extensions++;
  }

  if (num_extensions == 0) {
    return true;
  }

  Array<uint16_t> extension_types;
  if (!extension_types.InitForOverwrite(num_extensions)) {
    return false;
  }

  extensions = *cbs;
  for (size_t i = 0; i < extension_types.size(); i++) {
    CBS extension;

    if (!CBS_get_u16(&extensions, &extension_types[i]) ||
        !CBS_get_u16_length_prefixed(&extensions, &extension)) {

      return false;
    }
  }
  assert(CBS_len(&extensions) == 0);

  qsort(extension_types.data(), extension_types.size(), sizeof(uint16_t),
        compare_uint16_t);
  for (size_t i = 1; i < num_extensions; i++) {
    if (extension_types[i - 1] == extension_types[i]) {
      return false;
    }
  }

  return true;
}

static bool is_post_quantum_group(uint16_t id) {
  switch (id) {
    case SSL_GROUP_X25519_KYBER768_DRAFT00:
    case SSL_GROUP_X25519_MLKEM768:
      return true;
    default:
      return false;
  }
}

bool ssl_parse_client_hello_with_trailing_data(const SSL *ssl, CBS *cbs,
                                               SSL_CLIENT_HELLO *out) {
  OPENSSL_memset(out, 0, sizeof(*out));
  out->ssl = const_cast<SSL *>(ssl);

  CBS copy = *cbs;
  CBS random, session_id;
  if (!CBS_get_u16(cbs, &out->version) ||
      !CBS_get_bytes(cbs, &random, SSL3_RANDOM_SIZE) ||
      !CBS_get_u8_length_prefixed(cbs, &session_id) ||
      CBS_len(&session_id) > SSL_MAX_SSL_SESSION_ID_LENGTH) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_PARSE_FAILED);
    return false;
  }

  out->random = CBS_data(&random);
  out->random_len = CBS_len(&random);
  out->session_id = CBS_data(&session_id);
  out->session_id_len = CBS_len(&session_id);

  if (SSL_is_dtls(out->ssl)) {
    CBS cookie;
    if (!CBS_get_u8_length_prefixed(cbs, &cookie)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_PARSE_FAILED);
      return false;
    }
    out->dtls_cookie = CBS_data(&cookie);
    out->dtls_cookie_len = CBS_len(&cookie);
  } else {
    out->dtls_cookie = nullptr;
    out->dtls_cookie_len = 0;
  }

  CBS cipher_suites, compression_methods;
  if (!CBS_get_u16_length_prefixed(cbs, &cipher_suites) ||
      CBS_len(&cipher_suites) < 2 || (CBS_len(&cipher_suites) & 1) != 0 ||
      !CBS_get_u8_length_prefixed(cbs, &compression_methods) ||
      CBS_len(&compression_methods) < 1) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_PARSE_FAILED);
    return false;
  }

  out->cipher_suites = CBS_data(&cipher_suites);
  out->cipher_suites_len = CBS_len(&cipher_suites);
  out->compression_methods = CBS_data(&compression_methods);
  out->compression_methods_len = CBS_len(&compression_methods);

  if (CBS_len(cbs) == 0) {
    out->extensions = nullptr;
    out->extensions_len = 0;
  } else {

    CBS extensions;
    if (!CBS_get_u16_length_prefixed(cbs, &extensions) ||
        !tls1_check_duplicate_extensions(&extensions)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_PARSE_FAILED);
      return false;
    }
    out->extensions = CBS_data(&extensions);
    out->extensions_len = CBS_len(&extensions);
  }

  out->client_hello = CBS_data(&copy);
  out->client_hello_len = CBS_len(&copy) - CBS_len(cbs);
  return true;
}

bool ssl_client_hello_get_extension(const SSL_CLIENT_HELLO *client_hello,
                                    CBS *out, uint16_t extension_type) {
  CBS extensions;
  CBS_init(&extensions, client_hello->extensions, client_hello->extensions_len);
  while (CBS_len(&extensions) != 0) {

    uint16_t type;
    CBS extension;
    if (!CBS_get_u16(&extensions, &type) ||
        !CBS_get_u16_length_prefixed(&extensions, &extension)) {
      return false;
    }

    if (type == extension_type) {
      *out = extension;
      return true;
    }
  }

  return false;
}

static const uint16_t kDefaultGroups[] = {
    SSL_GROUP_X25519,
    SSL_GROUP_SECP256R1,
    SSL_GROUP_SECP384R1,
};

Span<const uint16_t> tls1_get_grouplist(const SSL_HANDSHAKE *hs) {
  if (!hs->config->supported_group_list.empty()) {
    return hs->config->supported_group_list;
  }
  return Span<const uint16_t>(kDefaultGroups);
}

bool tls1_get_shared_group(SSL_HANDSHAKE *hs, uint16_t *out_group_id) {
  SSL *const ssl = hs->ssl;
  assert(ssl->server);

  Span<const uint16_t> groups = tls1_get_grouplist(hs);
  Span<const uint16_t> pref, supp;
  if (ssl->options & SSL_OP_CIPHER_SERVER_PREFERENCE) {
    pref = groups;
    supp = hs->peer_supported_group_list;
  } else {
    pref = hs->peer_supported_group_list;
    supp = groups;
  }

  for (uint16_t pref_group : pref) {
    for (uint16_t supp_group : supp) {
      if (pref_group == supp_group &&

          (ssl_protocol_version(ssl) >= TLS1_3_VERSION ||
           !is_post_quantum_group(pref_group))) {
        *out_group_id = pref_group;
        return true;
      }
    }
  }

  return false;
}

bool tls1_check_group_id(const SSL_HANDSHAKE *hs, uint16_t group_id) {
  if (is_post_quantum_group(group_id) &&
      ssl_protocol_version(hs->ssl) < TLS1_3_VERSION) {

    return false;
  }

  if (group_id == 0) {
    return false;
  }

  for (uint16_t supported : tls1_get_grouplist(hs)) {
    if (supported == group_id) {
      return true;
    }
  }

  return false;
}

static const uint16_t kVerifySignatureAlgorithms[] = {

    SSL_SIGN_ECDSA_SECP256R1_SHA256,
    SSL_SIGN_RSA_PSS_RSAE_SHA256,
    SSL_SIGN_RSA_PKCS1_SHA256,

    SSL_SIGN_ECDSA_SECP384R1_SHA384,
    SSL_SIGN_RSA_PSS_RSAE_SHA384,
    SSL_SIGN_RSA_PKCS1_SHA384,

    SSL_SIGN_RSA_PSS_RSAE_SHA512,
    SSL_SIGN_RSA_PKCS1_SHA512,

    SSL_SIGN_RSA_PKCS1_SHA1,
};

static const uint16_t kSignSignatureAlgorithms[] = {

    SSL_SIGN_ED25519,
    SSL_SIGN_ECDSA_SECP256R1_SHA256,
    SSL_SIGN_RSA_PSS_RSAE_SHA256,
    SSL_SIGN_RSA_PKCS1_SHA256,

    SSL_SIGN_ECDSA_SECP384R1_SHA384,
    SSL_SIGN_RSA_PSS_RSAE_SHA384,
    SSL_SIGN_RSA_PKCS1_SHA384,

    SSL_SIGN_ECDSA_SECP521R1_SHA512,
    SSL_SIGN_RSA_PSS_RSAE_SHA512,
    SSL_SIGN_RSA_PKCS1_SHA512,

    SSL_SIGN_ECDSA_SHA1,
    SSL_SIGN_RSA_PKCS1_SHA1,
};

static Span<const uint16_t> tls12_get_verify_sigalgs(const SSL_HANDSHAKE *hs) {
  if (hs->config->verify_sigalgs.empty()) {
    return Span<const uint16_t>(kVerifySignatureAlgorithms);
  }
  return hs->config->verify_sigalgs;
}

bool tls12_add_verify_sigalgs(const SSL_HANDSHAKE *hs, CBB *out) {
  for (uint16_t sigalg : tls12_get_verify_sigalgs(hs)) {
    if (!CBB_add_u16(out, sigalg)) {
      return false;
    }
  }
  return true;
}

bool tls12_check_peer_sigalg(const SSL_HANDSHAKE *hs, uint8_t *out_alert,
                             uint16_t sigalg, EVP_PKEY *pkey) {

  Span<const uint16_t> sigalgs = tls12_get_verify_sigalgs(hs);
  if (std::find(sigalgs.begin(), sigalgs.end(), sigalg) == sigalgs.end() ||
      !ssl_pkey_supports_algorithm(hs->ssl, pkey, sigalg,               true)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_SIGNATURE_TYPE);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return false;
  }

  return true;
}

bool ssl_add_flags_extension(CBB *cbb, SSLFlags flags) {
  if (flags == 0) {
    return true;
  }

  CBB body, child;
  if (!CBB_add_u16(cbb, TLSEXT_TYPE_tls_flags) ||
      !CBB_add_u16_length_prefixed(cbb, &body) ||
      !CBB_add_u8_length_prefixed(&body, &child)) {
    return false;
  }

  while (flags != 0) {
    if (!CBB_add_u8(&child, static_cast<uint8_t>(flags))) {
      return false;
    }
    flags >>= 8;
  }

  return CBB_flush(cbb);
}

static bool ssl_parse_flags_extension(const CBS *cbs, SSLFlags *out,
                                      uint8_t *out_alert, bool allow_unknown) {
  CBS copy = *cbs, flags;
  if (!CBS_get_u8_length_prefixed(&copy, &flags) ||
      CBS_len(&copy) != 0 ||
      CBS_len(&flags) == 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    *out_alert = SSL_AD_DECODE_ERROR;
    return false;
  }

  if (CBS_data(&flags)[CBS_len(&flags) - 1] == 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return false;
  }

  if (!allow_unknown && CBS_len(&flags) > sizeof(SSLFlags)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return false;
  }

  uint8_t padded[sizeof(SSLFlags)] = {0};
  OPENSSL_memcpy(padded, CBS_data(&flags),
                 std::min(CBS_len(&flags), size_t{4}));
  static_assert(sizeof(SSLFlags) == sizeof(uint32_t),
                "We currently assume SSLFlags is 32-bit");
  *out = CRYPTO_load_u32_le(padded);
  return true;
}

bool ssl_parse_flags_extension_request(const CBS *cbs, SSLFlags *out,
                                       uint8_t *out_alert) {

  return ssl_parse_flags_extension(cbs, out, out_alert,
                                                     true);
}

bool ssl_parse_flags_extension_response(const CBS *cbs, SSLFlags *out,
                                        uint8_t *out_alert,
                                        SSLFlags allowed_flags) {

  if (!ssl_parse_flags_extension(cbs, out, out_alert,
                                                   false)) {
    return false;
  }

  if ((*out & allowed_flags) != *out) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return false;
  }

  return true;
}

struct tls_extension {
  uint16_t value;

  bool (*add_clienthello)(const SSL_HANDSHAKE *hs, CBB *out,
                          CBB *out_compressible, ssl_client_hello_type_t type);
  bool (*parse_serverhello)(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                            CBS *contents);

  bool (*parse_clienthello)(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                            CBS *contents);
  bool (*add_serverhello)(SSL_HANDSHAKE *hs, CBB *out);
};

static bool forbid_parse_serverhello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                     CBS *contents) {
  if (contents != NULL) {

    *out_alert = SSL_AD_UNSUPPORTED_EXTENSION;
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
    return false;
  }

  return true;
}

static bool ignore_parse_clienthello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                     CBS *contents) {

  return true;
}

static bool dont_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) { return true; }

static bool ext_sni_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                    CBB *out_compressible,
                                    ssl_client_hello_type_t type) {
  const SSL *const ssl = hs->ssl;

  Span<const uint8_t> hostname;
  if (type == ssl_client_hello_outer) {
    hostname = hs->selected_ech_config->public_name;
  } else {
    if (ssl->hostname == nullptr) {
      return true;
    }
    hostname = StringAsBytes(ssl->hostname.get());
  }

  CBB contents, server_name_list, name;
  if (!CBB_add_u16(out, TLSEXT_TYPE_server_name) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &server_name_list) ||
      !CBB_add_u8(&server_name_list, TLSEXT_NAMETYPE_host_name) ||
      !CBB_add_u16_length_prefixed(&server_name_list, &name) ||
      !CBB_add_bytes(&name, hostname.data(), hostname.size()) ||
      !CBB_flush(out)) {
    return false;
  }

  return true;
}

static bool ext_sni_parse_serverhello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                      CBS *contents) {

  return contents == NULL || CBS_len(contents) == 0;
}

static bool ext_sni_parse_clienthello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                      CBS *contents) {

  return true;
}

static bool ext_sni_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  if (hs->ssl->s3->session_reused ||
      !hs->should_ack_sni) {
    return true;
  }

  if (!CBB_add_u16(out, TLSEXT_TYPE_server_name) ||
      !CBB_add_u16(out, 0             )) {
    return false;
  }

  return true;
}

static bool ext_ech_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                    CBB *out_compressible,
                                    ssl_client_hello_type_t type) {
  if (type == ssl_client_hello_inner) {
    if (!CBB_add_u16(out, TLSEXT_TYPE_encrypted_client_hello) ||
        !CBB_add_u16(out,              1) ||
        !CBB_add_u8(out, ECH_CLIENT_INNER)) {
      return false;
    }
    return true;
  }

  if (hs->ech_client_outer.empty()) {
    return true;
  }

  CBB ech_body;
  if (!CBB_add_u16(out, TLSEXT_TYPE_encrypted_client_hello) ||
      !CBB_add_u16_length_prefixed(out, &ech_body) ||
      !CBB_add_u8(&ech_body, ECH_CLIENT_OUTER) ||
      !CBB_add_bytes(&ech_body, hs->ech_client_outer.data(),
                     hs->ech_client_outer.size()) ||
      !CBB_flush(out)) {
    return false;
  }
  return true;
}

static bool ext_ech_parse_serverhello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                      CBS *contents) {
  SSL *const ssl = hs->ssl;
  if (contents == NULL) {
    return true;
  }

  if (ssl_protocol_version(ssl) < TLS1_3_VERSION ||
      ssl->s3->ech_status == ssl_ech_accepted) {
    *out_alert = SSL_AD_UNSUPPORTED_EXTENSION;
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
    return false;
  }

  if (!ssl_is_valid_ech_config_list(*contents)) {
    *out_alert = SSL_AD_DECODE_ERROR;
    return false;
  }

  if (ssl->s3->ech_status == ssl_ech_rejected &&
      !hs->ech_retry_configs.CopyFrom(*contents)) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return false;
  }

  return true;
}

static bool ext_ech_parse_clienthello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                      CBS *contents) {
  if (contents == nullptr) {
    return true;
  }

  uint8_t type;
  if (!CBS_get_u8(contents, &type)) {
    return false;
  }
  if (type == ECH_CLIENT_OUTER) {

    return true;
  }
  if (type != ECH_CLIENT_INNER || CBS_len(contents) != 0) {
    return false;
  }

  hs->ech_is_inner = true;
  return true;
}

static bool ext_ech_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  SSL *const ssl = hs->ssl;
  if (ssl_protocol_version(ssl) < TLS1_3_VERSION ||
      ssl->s3->ech_status == ssl_ech_accepted ||
      hs->ech_keys == nullptr) {
    return true;
  }

  CBB body, retry_configs;
  if (!CBB_add_u16(out, TLSEXT_TYPE_encrypted_client_hello) ||
      !CBB_add_u16_length_prefixed(out, &body) ||
      !CBB_add_u16_length_prefixed(&body, &retry_configs)) {
    return false;
  }
  for (const auto &config : hs->ech_keys->configs) {
    if (!config->is_retry_config()) {
      continue;
    }
    if (!CBB_add_bytes(&retry_configs, config->ech_config().raw.data(),
                       config->ech_config().raw.size())) {
      return false;
    }
  }
  return CBB_flush(out);
}

static bool ext_ri_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                   CBB *out_compressible,
                                   ssl_client_hello_type_t type) {
  const SSL *const ssl = hs->ssl;

  if (hs->min_version >= TLS1_3_VERSION ||
      type == ssl_client_hello_inner) {
    return true;
  }

  assert(ssl->s3->initial_handshake_complete ==
         !ssl->s3->previous_client_finished.empty());

  CBB contents, prev_finished;
  if (!CBB_add_u16(out, TLSEXT_TYPE_renegotiate) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u8_length_prefixed(&contents, &prev_finished) ||
      !CBB_add_bytes(&prev_finished, ssl->s3->previous_client_finished.data(),
                     ssl->s3->previous_client_finished.size()) ||
      !CBB_flush(out)) {
    return false;
  }

  return true;
}

static bool ext_ri_parse_serverhello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                     CBS *contents) {
  SSL *const ssl = hs->ssl;
  if (contents != NULL && ssl_protocol_version(ssl) >= TLS1_3_VERSION) {
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return false;
  }

  if (ssl->s3->initial_handshake_complete &&
      (contents != NULL) != ssl->s3->send_connection_binding) {
    *out_alert = SSL_AD_HANDSHAKE_FAILURE;
    OPENSSL_PUT_ERROR(SSL, SSL_R_RENEGOTIATION_MISMATCH);
    return false;
  }

  if (contents == NULL) {

    return true;
  }

  assert(ssl->s3->previous_client_finished.size() ==
         ssl->s3->previous_server_finished.size());
  assert(ssl->s3->initial_handshake_complete ==
         !ssl->s3->previous_client_finished.empty());

  CBS renegotiated_connection;
  if (!CBS_get_u8_length_prefixed(contents, &renegotiated_connection) ||
      CBS_len(contents) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_RENEGOTIATION_ENCODING_ERR);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return false;
  }

  CBS client_verify, server_verify;
  if (!CBS_get_bytes(&renegotiated_connection, &client_verify,
                     ssl->s3->previous_client_finished.size()) ||
      !CBS_get_bytes(&renegotiated_connection, &server_verify,
                     ssl->s3->previous_server_finished.size()) ||
      CBS_len(&renegotiated_connection) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_RENEGOTIATION_MISMATCH);
    *out_alert = SSL_AD_HANDSHAKE_FAILURE;
    return false;
  }

  bool ok =
      CBS_mem_equal(&client_verify, ssl->s3->previous_client_finished.data(),
                    ssl->s3->previous_client_finished.size()) &&
      CBS_mem_equal(&server_verify, ssl->s3->previous_server_finished.data(),
                    ssl->s3->previous_server_finished.size());
  if (CRYPTO_fuzzer_mode_enabled()) {
    ok = true;
  }
  if (!ok) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_RENEGOTIATION_MISMATCH);
    *out_alert = SSL_AD_HANDSHAKE_FAILURE;
    return false;
  }

  ssl->s3->send_connection_binding = true;
  return true;
}

static bool ext_ri_parse_clienthello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                     CBS *contents) {
  SSL *const ssl = hs->ssl;

  assert(!ssl->s3->initial_handshake_complete);

  if (ssl_protocol_version(ssl) >= TLS1_3_VERSION) {
    return true;
  }

  if (contents == NULL) {
    return true;
  }

  CBS renegotiated_connection;
  if (!CBS_get_u8_length_prefixed(contents, &renegotiated_connection) ||
      CBS_len(contents) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_RENEGOTIATION_ENCODING_ERR);
    return false;
  }

  if (CBS_len(&renegotiated_connection) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_RENEGOTIATION_MISMATCH);
    *out_alert = SSL_AD_HANDSHAKE_FAILURE;
    return false;
  }

  ssl->s3->send_connection_binding = true;

  return true;
}

static bool ext_ri_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  SSL *const ssl = hs->ssl;

  assert(!ssl->s3->initial_handshake_complete);

  if (ssl_protocol_version(ssl) >= TLS1_3_VERSION) {
    return true;
  }

  if (!CBB_add_u16(out, TLSEXT_TYPE_renegotiate) ||
      !CBB_add_u16(out, 1             ) ||
      !CBB_add_u8(out, 0                               )) {
    return false;
  }

  return true;
}

static bool ext_ems_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                    CBB *out_compressible,
                                    ssl_client_hello_type_t type) {

  if (hs->min_version >= TLS1_3_VERSION || type == ssl_client_hello_inner) {
    return true;
  }

  if (!CBB_add_u16(out, TLSEXT_TYPE_extended_master_secret) ||
      !CBB_add_u16(out, 0             )) {
    return false;
  }

  return true;
}

static bool ext_ems_parse_serverhello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                      CBS *contents) {
  SSL *const ssl = hs->ssl;

  if (contents != NULL) {
    if (ssl_protocol_version(ssl) >= TLS1_3_VERSION ||
        CBS_len(contents) != 0) {
      return false;
    }

    hs->extended_master_secret = true;
  }

  if (ssl->s3->established_session != nullptr &&
      hs->extended_master_secret !=
          !!ssl->s3->established_session->extended_master_secret) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_RENEGOTIATION_EMS_MISMATCH);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return false;
  }

  return true;
}

static bool ext_ems_parse_clienthello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                      CBS *contents) {
  if (ssl_protocol_version(hs->ssl) >= TLS1_3_VERSION) {
    return true;
  }

  if (contents == NULL) {
    return true;
  }

  if (CBS_len(contents) != 0) {
    return false;
  }

  hs->extended_master_secret = true;
  return true;
}

static bool ext_ems_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  if (!hs->extended_master_secret) {
    return true;
  }

  if (!CBB_add_u16(out, TLSEXT_TYPE_extended_master_secret) ||
      !CBB_add_u16(out, 0             )) {
    return false;
  }

  return true;
}

static bool ext_ticket_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                       CBB *out_compressible,
                                       ssl_client_hello_type_t type) {
  const SSL *const ssl = hs->ssl;

  if (hs->min_version >= TLS1_3_VERSION || type == ssl_client_hello_inner ||
      SSL_get_options(ssl) & SSL_OP_NO_TICKET) {
    return true;
  }

  Span<const uint8_t> ticket;
  if (!ssl->s3->initial_handshake_complete &&
      ssl->session != nullptr &&
      ssl_session_get_type(ssl->session.get()) == SSLSessionType::kTicket) {
    ticket = ssl->session->ticket;
  }

  CBB ticket_cbb;
  if (!CBB_add_u16(out, TLSEXT_TYPE_session_ticket) ||
      !CBB_add_u16_length_prefixed(out, &ticket_cbb) ||
      !CBB_add_bytes(&ticket_cbb, ticket.data(), ticket.size()) ||
      !CBB_flush(out)) {
    return false;
  }

  return true;
}

static bool ext_ticket_parse_serverhello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                         CBS *contents) {
  SSL *const ssl = hs->ssl;
  if (contents == NULL) {
    return true;
  }

  if (ssl_protocol_version(ssl) >= TLS1_3_VERSION) {
    return false;
  }

  assert((SSL_get_options(ssl) & SSL_OP_NO_TICKET) == 0);

  if (CBS_len(contents) != 0) {
    return false;
  }

  hs->ticket_expected = true;
  return true;
}

static bool ext_ticket_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  if (!hs->ticket_expected) {
    return true;
  }

  assert((SSL_get_options(hs->ssl) & SSL_OP_NO_TICKET) == 0);

  if (!CBB_add_u16(out, TLSEXT_TYPE_session_ticket) ||
      !CBB_add_u16(out, 0             )) {
    return false;
  }

  return true;
}

static bool ext_sigalgs_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                        CBB *out_compressible,
                                        ssl_client_hello_type_t type) {
  if (hs->max_version < TLS1_2_VERSION) {
    return true;
  }

  if (hs->pake_prover != nullptr) {
    return true;
  }

  CBB contents, sigalgs_cbb;
  if (!CBB_add_u16(out_compressible, TLSEXT_TYPE_signature_algorithms) ||
      !CBB_add_u16_length_prefixed(out_compressible, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &sigalgs_cbb) ||
      !tls12_add_verify_sigalgs(hs, &sigalgs_cbb) ||
      !CBB_flush(out_compressible)) {
    return false;
  }

  return true;
}

static bool ext_sigalgs_parse_clienthello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                          CBS *contents) {
  hs->peer_sigalgs.Reset();
  if (contents == NULL) {
    return true;
  }

  CBS supported_signature_algorithms;
  if (!CBS_get_u16_length_prefixed(contents, &supported_signature_algorithms) ||
      CBS_len(contents) != 0 ||
      !tls1_parse_peer_sigalgs(hs, &supported_signature_algorithms)) {
    return false;
  }

  return true;
}

static bool ext_ocsp_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                     CBB *out_compressible,
                                     ssl_client_hello_type_t type) {
  if (!hs->config->ocsp_stapling_enabled) {
    return true;
  }

  CBB contents;
  if (!CBB_add_u16(out_compressible, TLSEXT_TYPE_status_request) ||
      !CBB_add_u16_length_prefixed(out_compressible, &contents) ||
      !CBB_add_u8(&contents, TLSEXT_STATUSTYPE_ocsp) ||
      !CBB_add_u16(&contents, 0                              ) ||
      !CBB_add_u16(&contents, 0                               ) ||
      !CBB_flush(out_compressible)) {
    return false;
  }

  return true;
}

static bool ext_ocsp_parse_serverhello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                       CBS *contents) {
  SSL *const ssl = hs->ssl;
  if (contents == NULL) {
    return true;
  }

  if (ssl_protocol_version(ssl) >= TLS1_3_VERSION) {
    return false;
  }

  if (CBS_len(contents) != 0 ||
      !ssl_cipher_uses_certificate_auth(hs->new_cipher)) {
    return false;
  }

  hs->certificate_status_expected = true;
  return true;
}

static bool ext_ocsp_parse_clienthello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                       CBS *contents) {
  if (contents == NULL) {
    return true;
  }

  uint8_t status_type;
  if (!CBS_get_u8(contents, &status_type)) {
    return false;
  }

  hs->ocsp_stapling_requested = status_type == TLSEXT_STATUSTYPE_ocsp;

  return true;
}

static bool ext_ocsp_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  SSL *const ssl = hs->ssl;
  if (ssl_protocol_version(ssl) >= TLS1_3_VERSION ||
      !hs->ocsp_stapling_requested || ssl->s3->session_reused ||
      !ssl_cipher_uses_certificate_auth(hs->new_cipher) ||
      hs->credential->ocsp_response == nullptr) {
    return true;
  }

  hs->certificate_status_expected = true;

  return CBB_add_u16(out, TLSEXT_TYPE_status_request) &&
         CBB_add_u16(out, 0             );
}

static bool ext_npn_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                    CBB *out_compressible,
                                    ssl_client_hello_type_t type) {
  const SSL *const ssl = hs->ssl;
  if (ssl->ctx->next_proto_select_cb == NULL ||

      ssl->s3->initial_handshake_complete ||

      SSL_is_dtls(ssl) || hs->min_version >= TLS1_3_VERSION ||
      type == ssl_client_hello_inner) {
    return true;
  }

  if (!CBB_add_u16(out, TLSEXT_TYPE_next_proto_neg) ||
      !CBB_add_u16(out, 0             )) {
    return false;
  }

  return true;
}

static bool ext_npn_parse_serverhello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                      CBS *contents) {
  SSL *const ssl = hs->ssl;
  if (contents == NULL) {
    return true;
  }

  if (ssl_protocol_version(ssl) >= TLS1_3_VERSION) {
    return false;
  }

  assert(!ssl->s3->initial_handshake_complete);
  assert(!SSL_is_dtls(ssl));
  assert(ssl->ctx->next_proto_select_cb != NULL);

  if (!ssl->s3->alpn_selected.empty()) {

    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    OPENSSL_PUT_ERROR(SSL, SSL_R_NEGOTIATED_BOTH_NPN_AND_ALPN);
    return false;
  }

  const uint8_t *const orig_contents = CBS_data(contents);
  const size_t orig_len = CBS_len(contents);

  while (CBS_len(contents) != 0) {
    CBS proto;
    if (!CBS_get_u8_length_prefixed(contents, &proto) ||
        CBS_len(&proto) == 0) {
      return false;
    }
  }

  uint8_t *selected;
  uint8_t selected_len;
  if (ssl->ctx->next_proto_select_cb(
          ssl, &selected, &selected_len, orig_contents,
          static_cast<unsigned>(orig_len),
          ssl->ctx->next_proto_select_cb_arg) != SSL_TLSEXT_ERR_OK ||
      !ssl->s3->next_proto_negotiated.CopyFrom(Span(selected, selected_len))) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return false;
  }

  hs->next_proto_neg_seen = true;
  return true;
}

static bool ext_npn_parse_clienthello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                      CBS *contents) {
  SSL *const ssl = hs->ssl;
  if (ssl_protocol_version(ssl) >= TLS1_3_VERSION) {
    return true;
  }

  if (contents != NULL && CBS_len(contents) != 0) {
    return false;
  }

  if (contents == NULL ||
      ssl->s3->initial_handshake_complete ||
      ssl->ctx->next_protos_advertised_cb == NULL ||
      SSL_is_dtls(ssl)) {
    return true;
  }

  hs->next_proto_neg_seen = true;
  return true;
}

static bool ext_npn_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  SSL *const ssl = hs->ssl;

  if (!hs->next_proto_neg_seen) {
    return true;
  }

  const uint8_t *npa;
  unsigned npa_len;

  if (ssl->ctx->next_protos_advertised_cb(
          ssl, &npa, &npa_len, ssl->ctx->next_protos_advertised_cb_arg) !=
      SSL_TLSEXT_ERR_OK) {
    hs->next_proto_neg_seen = false;
    return true;
  }

  CBB contents;
  if (!CBB_add_u16(out, TLSEXT_TYPE_next_proto_neg) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_bytes(&contents, npa, npa_len) ||
      !CBB_flush(out)) {
    return false;
  }

  return true;
}

static bool ext_sct_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                    CBB *out_compressible,
                                    ssl_client_hello_type_t type) {
  if (!hs->config->signed_cert_timestamps_enabled) {
    return true;
  }

  if (!CBB_add_u16(out_compressible, TLSEXT_TYPE_certificate_timestamp) ||
      !CBB_add_u16(out_compressible, 0             )) {
    return false;
  }

  return true;
}

static bool ext_sct_parse_serverhello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                      CBS *contents) {
  SSL *const ssl = hs->ssl;
  if (contents == NULL) {
    return true;
  }

  if (ssl_protocol_version(ssl) >= TLS1_3_VERSION) {
    *out_alert = SSL_AD_DECODE_ERROR;
    return false;
  }

  assert(hs->config->signed_cert_timestamps_enabled);

  if (!ssl_is_sct_list_valid(contents)) {
    *out_alert = SSL_AD_DECODE_ERROR;
    return false;
  }

  if (!ssl->s3->session_reused) {
    hs->new_session->signed_cert_timestamp_list.reset(
        CRYPTO_BUFFER_new_from_CBS(contents, ssl->ctx->pool));
    if (hs->new_session->signed_cert_timestamp_list == nullptr) {
      *out_alert = SSL_AD_INTERNAL_ERROR;
      return false;
    }
  }

  return true;
}

static bool ext_sct_parse_clienthello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                      CBS *contents) {
  if (contents == NULL) {
    return true;
  }

  if (CBS_len(contents) != 0) {
    return false;
  }

  hs->scts_requested = true;
  return true;
}

static bool ext_sct_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  SSL *const ssl = hs->ssl;
  assert(hs->scts_requested);

  if (ssl_protocol_version(ssl) >= TLS1_3_VERSION || ssl->s3->session_reused ||
      !ssl_cipher_uses_certificate_auth(hs->new_cipher) ||
      hs->credential->signed_cert_timestamp_list == nullptr) {
    return true;
  }

  CBB contents;
  return CBB_add_u16(out, TLSEXT_TYPE_certificate_timestamp) &&
         CBB_add_u16_length_prefixed(out, &contents) &&
         CBB_add_bytes(&contents,
                       CRYPTO_BUFFER_data(
                           hs->credential->signed_cert_timestamp_list.get()),
                       CRYPTO_BUFFER_len(
                           hs->credential->signed_cert_timestamp_list.get())) &&
         CBB_flush(out);
}

static bool ext_alpn_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                     CBB *out_compressible,
                                     ssl_client_hello_type_t type) {
  const SSL *const ssl = hs->ssl;
  if (hs->config->alpn_client_proto_list.empty() && SSL_is_quic(ssl)) {

    OPENSSL_PUT_ERROR(SSL, SSL_R_NO_APPLICATION_PROTOCOL);
    return false;
  }

  if (hs->config->alpn_client_proto_list.empty() ||
      ssl->s3->initial_handshake_complete) {
    return true;
  }

  CBB contents, proto_list;
  if (!CBB_add_u16(out_compressible,
                   TLSEXT_TYPE_application_layer_protocol_negotiation) ||
      !CBB_add_u16_length_prefixed(out_compressible, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &proto_list) ||
      !CBB_add_bytes(&proto_list, hs->config->alpn_client_proto_list.data(),
                     hs->config->alpn_client_proto_list.size()) ||
      !CBB_flush(out_compressible)) {
    return false;
  }

  return true;
}

static bool ext_alpn_parse_serverhello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                       CBS *contents) {
  SSL *const ssl = hs->ssl;
  if (contents == NULL) {
    if (SSL_is_quic(ssl)) {

      OPENSSL_PUT_ERROR(SSL, SSL_R_NO_APPLICATION_PROTOCOL);
      *out_alert = SSL_AD_NO_APPLICATION_PROTOCOL;
      return false;
    }
    return true;
  }

  assert(!ssl->s3->initial_handshake_complete);
  assert(!hs->config->alpn_client_proto_list.empty());

  if (hs->next_proto_neg_seen) {

    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    OPENSSL_PUT_ERROR(SSL, SSL_R_NEGOTIATED_BOTH_NPN_AND_ALPN);
    return false;
  }

  CBS protocol_name_list, protocol_name;
  if (!CBS_get_u16_length_prefixed(contents, &protocol_name_list) ||
      CBS_len(contents) != 0 ||
      !CBS_get_u8_length_prefixed(&protocol_name_list, &protocol_name) ||

      CBS_len(&protocol_name) == 0 ||
      CBS_len(&protocol_name_list) != 0) {
    return false;
  }

  if (!ssl_is_alpn_protocol_allowed(hs, protocol_name)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_ALPN_PROTOCOL);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return false;
  }

  if (!ssl->s3->alpn_selected.CopyFrom(protocol_name)) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return false;
  }

  return true;
}

bool ssl_is_valid_alpn_list(Span<const uint8_t> in) {
  CBS protocol_name_list = in;
  if (CBS_len(&protocol_name_list) == 0) {
    return false;
  }
  while (CBS_len(&protocol_name_list) > 0) {
    CBS protocol_name;
    if (!CBS_get_u8_length_prefixed(&protocol_name_list, &protocol_name) ||

        CBS_len(&protocol_name) == 0) {
      return false;
    }
  }
  return true;
}

bool ssl_is_alpn_protocol_allowed(const SSL_HANDSHAKE *hs,
                                  Span<const uint8_t> protocol) {
  if (hs->config->alpn_client_proto_list.empty()) {
    return false;
  }

  if (hs->ssl->ctx->allow_unknown_alpn_protos) {
    return true;
  }

  return ssl_alpn_list_contains_protocol(hs->config->alpn_client_proto_list,
                                         protocol);
}

bool ssl_alpn_list_contains_protocol(Span<const uint8_t> list,
                                     Span<const uint8_t> protocol) {
  CBS cbs = list, candidate;
  while (CBS_len(&cbs) > 0) {
    if (!CBS_get_u8_length_prefixed(&cbs, &candidate)) {
      return false;
    }

    if (candidate == protocol) {
      return true;
    }
  }

  return false;
}

bool ssl_negotiate_alpn(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                        const SSL_CLIENT_HELLO *client_hello) {
  SSL *const ssl = hs->ssl;
  CBS contents;
  if (ssl->ctx->alpn_select_cb == NULL ||
      !ssl_client_hello_get_extension(
          client_hello, &contents,
          TLSEXT_TYPE_application_layer_protocol_negotiation)) {
    if (SSL_is_quic(ssl)) {

      OPENSSL_PUT_ERROR(SSL, SSL_R_NO_APPLICATION_PROTOCOL);
      *out_alert = SSL_AD_NO_APPLICATION_PROTOCOL;
      return false;
    }

    return true;
  }

  hs->next_proto_neg_seen = false;

  CBS protocol_name_list;
  if (!CBS_get_u16_length_prefixed(&contents, &protocol_name_list) ||
      CBS_len(&contents) != 0 ||
      !ssl_is_valid_alpn_list(protocol_name_list)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_PARSE_TLSEXT);
    *out_alert = SSL_AD_DECODE_ERROR;
    return false;
  }

  const uint8_t *selected;
  uint8_t selected_len;
  int ret = ssl->ctx->alpn_select_cb(
      ssl, &selected, &selected_len, CBS_data(&protocol_name_list),
      static_cast<unsigned>(CBS_len(&protocol_name_list)),
      ssl->ctx->alpn_select_cb_arg);

  if (SSL_is_quic(ssl) &&
      (ret == SSL_TLSEXT_ERR_NOACK || ret == SSL_TLSEXT_ERR_ALERT_WARNING)) {
    ret = SSL_TLSEXT_ERR_ALERT_FATAL;
  }
  switch (ret) {
    case SSL_TLSEXT_ERR_OK:
      if (selected_len == 0) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_ALPN_PROTOCOL);
        *out_alert = SSL_AD_INTERNAL_ERROR;
        return false;
      }
      if (!ssl->s3->alpn_selected.CopyFrom(Span(selected, selected_len))) {
        *out_alert = SSL_AD_INTERNAL_ERROR;
        return false;
      }
      break;
    case SSL_TLSEXT_ERR_NOACK:
    case SSL_TLSEXT_ERR_ALERT_WARNING:
      break;
    case SSL_TLSEXT_ERR_ALERT_FATAL:
      *out_alert = SSL_AD_NO_APPLICATION_PROTOCOL;
      OPENSSL_PUT_ERROR(SSL, SSL_R_NO_APPLICATION_PROTOCOL);
      return false;
    default:

      *out_alert = SSL_AD_INTERNAL_ERROR;
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return false;
  }

  return true;
}

static bool ext_alpn_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  SSL *const ssl = hs->ssl;
  if (ssl->s3->alpn_selected.empty()) {
    return true;
  }

  CBB contents, proto_list, proto;
  if (!CBB_add_u16(out, TLSEXT_TYPE_application_layer_protocol_negotiation) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &proto_list) ||
      !CBB_add_u8_length_prefixed(&proto_list, &proto) ||
      !CBB_add_bytes(&proto, ssl->s3->alpn_selected.data(),
                     ssl->s3->alpn_selected.size()) ||
      !CBB_flush(out)) {
    return false;
  }

  return true;
}

static bool ext_channel_id_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                           CBB *out_compressible,
                                           ssl_client_hello_type_t type) {
  const SSL *const ssl = hs->ssl;
  if (!hs->config->channel_id_private || SSL_is_dtls(ssl) ||

      type == ssl_client_hello_outer) {
    return true;
  }

  if (!CBB_add_u16(out, TLSEXT_TYPE_channel_id) ||
      !CBB_add_u16(out, 0             )) {
    return false;
  }

  return true;
}

static bool ext_channel_id_parse_serverhello(SSL_HANDSHAKE *hs,
                                             uint8_t *out_alert,
                                             CBS *contents) {
  if (contents == NULL) {
    return true;
  }

  assert(!SSL_is_dtls(hs->ssl));
  assert(hs->config->channel_id_private);

  if (CBS_len(contents) != 0) {
    return false;
  }

  hs->channel_id_negotiated = true;
  return true;
}

static bool ext_channel_id_parse_clienthello(SSL_HANDSHAKE *hs,
                                             uint8_t *out_alert,
                                             CBS *contents) {
  SSL *const ssl = hs->ssl;
  if (contents == NULL || !hs->config->channel_id_enabled || SSL_is_dtls(ssl)) {
    return true;
  }

  if (CBS_len(contents) != 0) {
    return false;
  }

  hs->channel_id_negotiated = true;
  return true;
}

static bool ext_channel_id_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  if (!hs->channel_id_negotiated) {
    return true;
  }

  if (!CBB_add_u16(out, TLSEXT_TYPE_channel_id) ||
      !CBB_add_u16(out, 0             )) {
    return false;
  }

  return true;
}

static bool ext_srtp_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                     CBB *out_compressible,
                                     ssl_client_hello_type_t type) {
  const SSL *const ssl = hs->ssl;
  const STACK_OF(SRTP_PROTECTION_PROFILE) *profiles =
      SSL_get_srtp_profiles(ssl);
  if (profiles == NULL ||
      sk_SRTP_PROTECTION_PROFILE_num(profiles) == 0 ||
      !SSL_is_dtls(ssl)) {
    return true;
  }

  CBB contents, profile_ids;
  if (!CBB_add_u16(out_compressible, TLSEXT_TYPE_srtp) ||
      !CBB_add_u16_length_prefixed(out_compressible, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &profile_ids)) {
    return false;
  }

  for (const SRTP_PROTECTION_PROFILE *profile : profiles) {
    if (!CBB_add_u16(&profile_ids, profile->id)) {
      return false;
    }
  }

  if (!CBB_add_u8(&contents, 0                          ) ||
      !CBB_flush(out_compressible)) {
    return false;
  }

  return true;
}

static bool ext_srtp_parse_serverhello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                       CBS *contents) {
  SSL *const ssl = hs->ssl;
  if (contents == NULL) {
    return true;
  }

  assert(SSL_is_dtls(ssl));
  CBS profile_ids, srtp_mki;
  uint16_t profile_id;
  if (!CBS_get_u16_length_prefixed(contents, &profile_ids) ||
      !CBS_get_u16(&profile_ids, &profile_id) ||
      CBS_len(&profile_ids) != 0 ||
      !CBS_get_u8_length_prefixed(contents, &srtp_mki) ||
      CBS_len(contents) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
    return false;
  }

  if (CBS_len(&srtp_mki) != 0) {

    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_SRTP_MKI_VALUE);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return false;
  }

  for (const SRTP_PROTECTION_PROFILE *profile : SSL_get_srtp_profiles(ssl)) {
    if (profile->id == profile_id) {
      ssl->s3->srtp_profile = profile;
      return true;
    }
  }

  OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
  *out_alert = SSL_AD_ILLEGAL_PARAMETER;
  return false;
}

static bool ext_srtp_parse_clienthello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                       CBS *contents) {
  SSL *const ssl = hs->ssl;

  if (contents == NULL || !SSL_is_dtls(ssl)) {
    return true;
  }

  CBS profile_ids, srtp_mki;
  if (!CBS_get_u16_length_prefixed(contents, &profile_ids) ||
      CBS_len(&profile_ids) < 2 ||
      !CBS_get_u8_length_prefixed(contents, &srtp_mki) ||
      CBS_len(contents) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST);
    return false;
  }

  const STACK_OF(SRTP_PROTECTION_PROFILE) *server_profiles =
      SSL_get_srtp_profiles(ssl);

  for (const SRTP_PROTECTION_PROFILE *server_profile : server_profiles) {
    CBS profile_ids_tmp;
    CBS_init(&profile_ids_tmp, CBS_data(&profile_ids), CBS_len(&profile_ids));

    while (CBS_len(&profile_ids_tmp) > 0) {
      uint16_t profile_id;
      if (!CBS_get_u16(&profile_ids_tmp, &profile_id)) {
        return false;
      }

      if (server_profile->id == profile_id) {
        ssl->s3->srtp_profile = server_profile;
        return true;
      }
    }
  }

  return true;
}

static bool ext_srtp_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  SSL *const ssl = hs->ssl;
  if (ssl->s3->srtp_profile == NULL) {
    return true;
  }

  assert(SSL_is_dtls(ssl));
  CBB contents, profile_ids;
  if (!CBB_add_u16(out, TLSEXT_TYPE_srtp) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &profile_ids) ||
      !CBB_add_u16(&profile_ids, ssl->s3->srtp_profile->id) ||
      !CBB_add_u8(&contents, 0                ) || !CBB_flush(out)) {
    return false;
  }

  return true;
}

static bool ext_ec_point_add_extension(const SSL_HANDSHAKE *hs, CBB *out) {
  CBB contents, formats;
  if (!CBB_add_u16(out, TLSEXT_TYPE_ec_point_formats) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u8_length_prefixed(&contents, &formats) ||
      !CBB_add_u8(&formats, TLSEXT_ECPOINTFORMAT_uncompressed) ||
      !CBB_flush(out)) {
    return false;
  }

  return true;
}

static bool ext_ec_point_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                         CBB *out_compressible,
                                         ssl_client_hello_type_t type) {

  if (hs->min_version >= TLS1_3_VERSION || type == ssl_client_hello_inner) {
    return true;
  }

  return ext_ec_point_add_extension(hs, out);
}

static bool ext_ec_point_parse_serverhello(SSL_HANDSHAKE *hs,
                                           uint8_t *out_alert, CBS *contents) {
  if (contents == NULL) {
    return true;
  }

  if (ssl_protocol_version(hs->ssl) >= TLS1_3_VERSION) {
    return false;
  }

  CBS ec_point_format_list;
  if (!CBS_get_u8_length_prefixed(contents, &ec_point_format_list) ||
      CBS_len(contents) != 0) {
    return false;
  }

  if (OPENSSL_memchr(CBS_data(&ec_point_format_list),
                     TLSEXT_ECPOINTFORMAT_uncompressed,
                     CBS_len(&ec_point_format_list)) == NULL) {
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return false;
  }

  return true;
}

static bool ext_ec_point_parse_clienthello(SSL_HANDSHAKE *hs,
                                           uint8_t *out_alert, CBS *contents) {
  if (ssl_protocol_version(hs->ssl) >= TLS1_3_VERSION) {
    return true;
  }

  return ext_ec_point_parse_serverhello(hs, out_alert, contents);
}

static bool ext_ec_point_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  SSL *const ssl = hs->ssl;
  if (ssl_protocol_version(ssl) >= TLS1_3_VERSION) {
    return true;
  }

  const uint32_t alg_k = hs->new_cipher->algorithm_mkey;
  const uint32_t alg_a = hs->new_cipher->algorithm_auth;
  const bool using_ecc = (alg_k & SSL_kECDHE) || (alg_a & SSL_aECDSA);

  if (!using_ecc) {
    return true;
  }

  return ext_ec_point_add_extension(hs, out);
}

static bool should_offer_psk(const SSL_HANDSHAKE *hs,
                             ssl_client_hello_type_t type) {
  const SSL *const ssl = hs->ssl;
  if (hs->max_version < TLS1_3_VERSION || ssl->session == nullptr ||
      ssl_session_get_type(ssl->session.get()) !=
          SSLSessionType::kPreSharedKey ||

      type == ssl_client_hello_outer) {
    return false;
  }

  if (ssl->s3->used_hello_retry_request &&
      ssl->session->cipher->algorithm_prf != hs->new_cipher->algorithm_prf) {
    return false;
  }

  return true;
}

static size_t ext_pre_shared_key_clienthello_length(
    const SSL_HANDSHAKE *hs, ssl_client_hello_type_t type) {
  const SSL *const ssl = hs->ssl;
  if (!should_offer_psk(hs, type)) {
    return 0;
  }

  size_t binder_len = EVP_MD_size(ssl_session_get_digest(ssl->session.get()));
  return 15 + ssl->session->ticket.size() + binder_len;
}

static bool ext_pre_shared_key_add_clienthello(const SSL_HANDSHAKE *hs,
                                               CBB *out, bool *out_needs_binder,
                                               ssl_client_hello_type_t type) {
  const SSL *const ssl = hs->ssl;
  *out_needs_binder = false;
  if (!should_offer_psk(hs, type)) {
    return true;
  }

  OPENSSL_timeval now = ssl_ctx_get_current_time(ssl->ctx.get());
  uint32_t ticket_age = 1000 * (now.tv_sec - ssl->session->time);
  uint32_t obfuscated_ticket_age = ticket_age + ssl->session->ticket_age_add;

  size_t binder_len = EVP_MD_size(ssl_session_get_digest(ssl->session.get()));

  CBB contents, identity, ticket, binders, binder;
  if (!CBB_add_u16(out, TLSEXT_TYPE_pre_shared_key) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &identity) ||
      !CBB_add_u16_length_prefixed(&identity, &ticket) ||
      !CBB_add_bytes(&ticket, ssl->session->ticket.data(),
                     ssl->session->ticket.size()) ||
      !CBB_add_u32(&identity, obfuscated_ticket_age) ||
      !CBB_add_u16_length_prefixed(&contents, &binders) ||
      !CBB_add_u8_length_prefixed(&binders, &binder) ||
      !CBB_add_zeros(&binder, binder_len)) {
    return false;
  }

  *out_needs_binder = true;
  return CBB_flush(out);
}

bool ssl_ext_pre_shared_key_parse_serverhello(SSL_HANDSHAKE *hs,
                                              uint8_t *out_alert,
                                              CBS *contents) {
  uint16_t psk_id;
  if (!CBS_get_u16(contents, &psk_id) ||
      CBS_len(contents) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    *out_alert = SSL_AD_DECODE_ERROR;
    return false;
  }

  if (psk_id != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_PSK_IDENTITY_NOT_FOUND);
    *out_alert = SSL_AD_UNKNOWN_PSK_IDENTITY;
    return false;
  }

  return true;
}

bool ssl_ext_pre_shared_key_parse_clienthello(
    SSL_HANDSHAKE *hs, CBS *out_ticket, CBS *out_binders,
    uint32_t *out_obfuscated_ticket_age, uint8_t *out_alert,
    const SSL_CLIENT_HELLO *client_hello, CBS *contents) {

  if (CBS_data(contents) + CBS_len(contents) !=
      client_hello->extensions + client_hello->extensions_len) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_PRE_SHARED_KEY_MUST_BE_LAST);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return false;
  }

  CBS identities, binders;
  if (!CBS_get_u16_length_prefixed(contents, &identities) ||
      !CBS_get_u16_length_prefixed(&identities, out_ticket) ||
      !CBS_get_u32(&identities, out_obfuscated_ticket_age) ||
      !CBS_get_u16_length_prefixed(contents, &binders) ||
      CBS_len(&binders) == 0 ||
      CBS_len(contents) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    *out_alert = SSL_AD_DECODE_ERROR;
    return false;
  }

  *out_binders = binders;

  size_t num_identities = 1;
  while (CBS_len(&identities) != 0) {
    CBS unused_ticket;
    uint32_t unused_obfuscated_ticket_age;
    if (!CBS_get_u16_length_prefixed(&identities, &unused_ticket) ||
        !CBS_get_u32(&identities, &unused_obfuscated_ticket_age)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      *out_alert = SSL_AD_DECODE_ERROR;
      return false;
    }

    num_identities++;
  }

  size_t num_binders = 0;
  while (CBS_len(&binders) != 0) {
    CBS binder;
    if (!CBS_get_u8_length_prefixed(&binders, &binder)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      *out_alert = SSL_AD_DECODE_ERROR;
      return false;
    }

    num_binders++;
  }

  if (num_identities != num_binders) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_PSK_IDENTITY_BINDER_COUNT_MISMATCH);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return false;
  }

  return true;
}

bool ssl_ext_pre_shared_key_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  if (!hs->ssl->s3->session_reused) {
    return true;
  }

  CBB contents;
  if (!CBB_add_u16(out, TLSEXT_TYPE_pre_shared_key) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||

      !CBB_add_u16(&contents, 0) ||
      !CBB_flush(out)) {
    return false;
  }

  return true;
}

static bool ext_psk_key_exchange_modes_add_clienthello(
    const SSL_HANDSHAKE *hs, CBB *out, CBB *out_compressible,
    ssl_client_hello_type_t type) {
  if (hs->max_version < TLS1_3_VERSION) {
    return true;
  }

  if (hs->pake_prover != nullptr) {
    return true;
  }

  CBB contents, ke_modes;
  if (!CBB_add_u16(out_compressible, TLSEXT_TYPE_psk_key_exchange_modes) ||
      !CBB_add_u16_length_prefixed(out_compressible, &contents) ||
      !CBB_add_u8_length_prefixed(&contents, &ke_modes) ||
      !CBB_add_u8(&ke_modes, SSL_PSK_DHE_KE)) {
    return false;
  }

  return CBB_flush(out_compressible);
}

static bool ext_psk_key_exchange_modes_parse_clienthello(SSL_HANDSHAKE *hs,
                                                         uint8_t *out_alert,
                                                         CBS *contents) {
  if (contents == NULL) {
    return true;
  }

  CBS ke_modes;
  if (!CBS_get_u8_length_prefixed(contents, &ke_modes) ||
      CBS_len(&ke_modes) == 0 ||
      CBS_len(contents) != 0) {
    *out_alert = SSL_AD_DECODE_ERROR;
    return false;
  }

  hs->accept_psk_mode = OPENSSL_memchr(CBS_data(&ke_modes), SSL_PSK_DHE_KE,
                                       CBS_len(&ke_modes)) != NULL;

  return true;
}

static bool ext_early_data_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                           CBB *out_compressible,
                                           ssl_client_hello_type_t type) {
  const SSL *const ssl = hs->ssl;

  if (ssl->s3->used_hello_retry_request) {
    assert(ssl->s3->early_data_reason != ssl_early_data_unknown);
    return true;
  }

  if (!hs->early_data_offered) {
    return true;
  }

  if (!CBB_add_u16(out_compressible, TLSEXT_TYPE_early_data) ||
      !CBB_add_u16(out_compressible, 0) ||
      !CBB_flush(out_compressible)) {
    return false;
  }

  return true;
}

static bool ext_early_data_parse_serverhello(SSL_HANDSHAKE *hs,
                                             uint8_t *out_alert,
                                             CBS *contents) {
  SSL *const ssl = hs->ssl;
  if (contents == NULL) {
    if (hs->early_data_offered && !ssl->s3->used_hello_retry_request) {
      ssl->s3->early_data_reason = ssl->s3->session_reused
                                       ? ssl_early_data_peer_declined
                                       : ssl_early_data_session_not_resumed;
    } else {

      assert(ssl->s3->early_data_reason != ssl_early_data_unknown);
    }
    return true;
  }

  assert(!ssl->s3->used_hello_retry_request);

  if (CBS_len(contents) != 0) {
    *out_alert = SSL_AD_DECODE_ERROR;
    return false;
  }

  if (!ssl->s3->session_reused) {
    *out_alert = SSL_AD_UNSUPPORTED_EXTENSION;
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
    return false;
  }

  ssl->s3->early_data_reason = ssl_early_data_accepted;
  ssl->s3->early_data_accepted = true;
  return true;
}

static bool ext_early_data_parse_clienthello(SSL_HANDSHAKE *hs,
                                             uint8_t *out_alert,
                                             CBS *contents) {
  SSL *const ssl = hs->ssl;
  if (contents == NULL || ssl_protocol_version(ssl) < TLS1_3_VERSION) {
    return true;
  }

  if (CBS_len(contents) != 0) {
    *out_alert = SSL_AD_DECODE_ERROR;
    return false;
  }

  hs->early_data_offered = true;
  return true;
}

static bool ext_early_data_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  if (!hs->ssl->s3->early_data_accepted) {
    return true;
  }

  if (!CBB_add_u16(out, TLSEXT_TYPE_early_data) ||
      !CBB_add_u16(out, 0) ||
      !CBB_flush(out)) {
    return false;
  }

  return true;
}

bool ssl_setup_key_shares(SSL_HANDSHAKE *hs, uint16_t override_group_id) {
  SSL *const ssl = hs->ssl;
  hs->key_shares[0].reset();
  hs->key_shares[1].reset();
  hs->key_share_bytes.Reset();

  if (hs->max_version < TLS1_3_VERSION || hs->pake_prover) {
    return true;
  }

  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 64)) {
    return false;
  }

  if (override_group_id == 0 && ssl->ctx->grease_enabled) {

    if (!CBB_add_u16(cbb.get(), ssl_get_grease_value(hs, ssl_grease_group)) ||
        !CBB_add_u16(cbb.get(), 1             ) ||
        !CBB_add_u8(cbb.get(), 0                         )) {
      return false;
    }
  }

  uint16_t group_id = override_group_id;
  uint16_t second_group_id = 0;
  if (override_group_id == 0) {

    Span<const uint16_t> groups = tls1_get_grouplist(hs);
    if (groups.empty()) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_NO_GROUPS_SPECIFIED);
      return false;
    }

    group_id = groups[0];

    for (size_t i = 1; i < groups.size() && second_group_id == 0; i++) {
      if (is_post_quantum_group(group_id) != is_post_quantum_group(groups[i])) {
        second_group_id = groups[i];
        assert(second_group_id != group_id);
      }
    }
  }

  CBB key_exchange;
  hs->key_shares[0] = SSLKeyShare::Create(group_id);
  if (!hs->key_shares[0] ||
      !CBB_add_u16(cbb.get(), group_id) ||
      !CBB_add_u16_length_prefixed(cbb.get(), &key_exchange) ||
      !hs->key_shares[0]->Generate(&key_exchange)) {
    return false;
  }

  if (second_group_id != 0) {
    hs->key_shares[1] = SSLKeyShare::Create(second_group_id);
    if (!hs->key_shares[1] ||
        !CBB_add_u16(cbb.get(), second_group_id) ||
        !CBB_add_u16_length_prefixed(cbb.get(), &key_exchange) ||
        !hs->key_shares[1]->Generate(&key_exchange)) {
      return false;
    }
  }

  return CBBFinishArray(cbb.get(), &hs->key_share_bytes);
}

static bool ext_key_share_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                          CBB *out_compressible,
                                          ssl_client_hello_type_t type) {

  if (hs->max_version < TLS1_3_VERSION || hs->pake_prover) {
    return true;
  }

  assert(!hs->key_share_bytes.empty());
  CBB contents, kse_bytes;
  if (!CBB_add_u16(out_compressible, TLSEXT_TYPE_key_share) ||
      !CBB_add_u16_length_prefixed(out_compressible, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &kse_bytes) ||
      !CBB_add_bytes(&kse_bytes, hs->key_share_bytes.data(),
                     hs->key_share_bytes.size()) ||
      !CBB_flush(out_compressible)) {
    return false;
  }

  return true;
}

bool ssl_ext_key_share_parse_serverhello(SSL_HANDSHAKE *hs,
                                         Array<uint8_t> *out_secret,
                                         uint8_t *out_alert, CBS *contents) {
  if (hs->key_shares[0] == nullptr) {

    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return false;
  }

  CBS ciphertext;
  uint16_t group_id;
  if (!CBS_get_u16(contents, &group_id) ||
      !CBS_get_u16_length_prefixed(contents, &ciphertext) ||
      CBS_len(contents) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    *out_alert = SSL_AD_DECODE_ERROR;
    return false;
  }

  SSLKeyShare *key_share = hs->key_shares[0].get();
  if (key_share->GroupID() != group_id) {
    if (!hs->key_shares[1] || hs->key_shares[1]->GroupID() != group_id) {
      *out_alert = SSL_AD_ILLEGAL_PARAMETER;
      OPENSSL_PUT_ERROR(SSL, SSL_R_WRONG_CURVE);
      return false;
    }
    key_share = hs->key_shares[1].get();
  }

  if (!key_share->Decap(out_secret, out_alert, ciphertext)) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return false;
  }

  hs->new_session->group_id = group_id;
  hs->key_shares[0].reset();
  hs->key_shares[1].reset();
  return true;
}

bool ssl_ext_key_share_parse_clienthello(SSL_HANDSHAKE *hs, bool *out_found,
                                         Span<const uint8_t> *out_peer_key,
                                         uint8_t *out_alert,
                                         const SSL_CLIENT_HELLO *client_hello) {

  CBS contents;
  if (!ssl_client_hello_get_extension(client_hello, &contents,
                                      TLSEXT_TYPE_key_share)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_KEY_SHARE);
    *out_alert = SSL_AD_MISSING_EXTENSION;
    return false;
  }

  CBS key_shares;
  if (!CBS_get_u16_length_prefixed(&contents, &key_shares) ||
      CBS_len(&contents) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }

  const uint16_t group_id = hs->new_session->group_id;
  CBS peer_key;
  CBS_init(&peer_key, nullptr, 0);
  while (CBS_len(&key_shares) > 0) {
    uint16_t id;
    CBS peer_key_tmp;
    if (!CBS_get_u16(&key_shares, &id) ||
        !CBS_get_u16_length_prefixed(&key_shares, &peer_key_tmp) ||
        CBS_len(&peer_key_tmp) == 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      return false;
    }

    if (id == group_id) {
      if (CBS_len(&peer_key) != 0) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_DUPLICATE_KEY_SHARE);
        *out_alert = SSL_AD_ILLEGAL_PARAMETER;
        return false;
      }

      peer_key = peer_key_tmp;

    }
  }

  if (out_peer_key != nullptr) {
    *out_peer_key = peer_key;
  }
  *out_found = CBS_len(&peer_key) != 0;
  return true;
}

bool ssl_ext_pake_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  if (hs->pake_share_bytes.empty()) {
    return true;
  }

  CBB pake_ext, pake_msg;
  if (!CBB_add_u16(out, TLSEXT_TYPE_pake) ||
      !CBB_add_u16_length_prefixed(out, &pake_ext) ||
      !CBB_add_u16(&pake_ext, SSL_PAKE_SPAKE2PLUSV1) ||
      !CBB_add_u16_length_prefixed(&pake_ext, &pake_msg) ||
      !CBB_add_bytes(&pake_msg, hs->pake_share_bytes.data(),
                     hs->pake_share_bytes.size()) ||
      !CBB_flush(out)) {
    return false;
  }
  return true;
}

bool ssl_ext_key_share_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  if (hs->pake_verifier) {

    return true;
  }

  CBB entry, ciphertext;
  if (!CBB_add_u16(out, TLSEXT_TYPE_key_share) ||
      !CBB_add_u16_length_prefixed(out, &entry) ||
      !CBB_add_u16(&entry, hs->new_session->group_id) ||
      !CBB_add_u16_length_prefixed(&entry, &ciphertext) ||
      !CBB_add_bytes(&ciphertext, hs->key_share_ciphertext.data(),
                     hs->key_share_ciphertext.size()) ||
      !CBB_flush(out)) {
    return false;
  }
  return true;
}

static bool ext_supported_versions_add_clienthello(
    const SSL_HANDSHAKE *hs, CBB *out, CBB *out_compressible,
    ssl_client_hello_type_t type) {
  const SSL *const ssl = hs->ssl;
  if (hs->max_version <= TLS1_2_VERSION) {
    return true;
  }

  if (hs->min_version >= TLS1_3_VERSION) {
    out = out_compressible;
  }

  CBB contents, versions;
  if (!CBB_add_u16(out, TLSEXT_TYPE_supported_versions) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u8_length_prefixed(&contents, &versions)) {
    return false;
  }

  if (ssl->ctx->grease_enabled &&
      !CBB_add_u16(&versions, ssl_get_grease_value(hs, ssl_grease_version))) {
    return false;
  }

  uint16_t extra_min_version =
      type == ssl_client_hello_inner ? TLS1_3_VERSION : 0;
  if (!ssl_add_supported_versions(hs, &versions, extra_min_version) ||
      !CBB_flush(out)) {
    return false;
  }

  return true;
}

static bool ext_cookie_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                       CBB *out_compressible,
                                       ssl_client_hello_type_t type) {
  if (hs->cookie.empty()) {
    return true;
  }

  CBB contents, cookie;
  if (!CBB_add_u16(out_compressible, TLSEXT_TYPE_cookie) ||
      !CBB_add_u16_length_prefixed(out_compressible, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &cookie) ||
      !CBB_add_bytes(&cookie, hs->cookie.data(), hs->cookie.size()) ||
      !CBB_flush(out_compressible)) {
    return false;
  }

  return true;
}

static bool ext_supported_groups_add_clienthello(const SSL_HANDSHAKE *hs,
                                                 CBB *out,
                                                 CBB *out_compressible,
                                                 ssl_client_hello_type_t type) {
  const SSL *const ssl = hs->ssl;

  if (hs->pake_prover != nullptr) {
    return true;
  }

  CBB contents, groups_bytes;
  if (!CBB_add_u16(out_compressible, TLSEXT_TYPE_supported_groups) ||
      !CBB_add_u16_length_prefixed(out_compressible, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &groups_bytes)) {
    return false;
  }

  if (ssl->ctx->grease_enabled &&
      !CBB_add_u16(&groups_bytes, ssl_get_grease_value(hs, ssl_grease_group))) {
    return false;
  }

  for (uint16_t group : tls1_get_grouplist(hs)) {
    if (is_post_quantum_group(group) && hs->max_version < TLS1_3_VERSION) {
      continue;
    }
    if (!CBB_add_u16(&groups_bytes, group)) {
      return false;
    }
  }

  return CBB_flush(out_compressible);
}

static bool ext_supported_groups_parse_serverhello(SSL_HANDSHAKE *hs,
                                                   uint8_t *out_alert,
                                                   CBS *contents) {

  return true;
}

static bool parse_u16_array(const CBS *cbs, Array<uint16_t> *out) {
  CBS copy = *cbs;
  if ((CBS_len(&copy) & 1) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }

  Array<uint16_t> ret;
  if (!ret.InitForOverwrite(CBS_len(&copy) / 2)) {
    return false;
  }
  for (size_t i = 0; i < ret.size(); i++) {
    if (!CBS_get_u16(&copy, &ret[i])) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return false;
    }
  }

  assert(CBS_len(&copy) == 0);
  *out = std::move(ret);
  return true;
}

static bool ext_supported_groups_parse_clienthello(SSL_HANDSHAKE *hs,
                                                   uint8_t *out_alert,
                                                   CBS *contents) {
  if (contents == NULL) {
    return true;
  }

  CBS supported_group_list;
  if (!CBS_get_u16_length_prefixed(contents, &supported_group_list) ||
      CBS_len(&supported_group_list) == 0 ||
      CBS_len(contents) != 0 ||
      !parse_u16_array(&supported_group_list, &hs->peer_supported_group_list)) {
    return false;
  }

  return true;
}

static bool ext_certificate_authorities_add_clienthello(
    const SSL_HANDSHAKE *hs, CBB *out, CBB *out_compressible,
    ssl_client_hello_type_t type) {

  if (ssl_has_CA_names(hs->config)) {
    CBB ca_contents;
    if (!CBB_add_u16(out_compressible,
                     TLSEXT_TYPE_certificate_authorities) ||
        !CBB_add_u16_length_prefixed(out_compressible, &ca_contents) ||
        !ssl_add_CA_names(hs, &ca_contents) ||
        !CBB_flush(out_compressible)) {
      return false;
    }
  }
  return true;
}

static bool ext_certificate_authorities_parse_clienthello(SSL_HANDSHAKE *hs,
                                                          uint8_t *out_alert,
                                                          CBS *contents) {
  if (contents == NULL) {
    return true;
  }

  if (CBS_len(contents) == 0) {
    return false;
  }

  hs->ca_names = SSL_parse_CA_list(hs->ssl, out_alert, contents);
  if (!hs->ca_names) {
    return false;
  }

  return true;
}

bool ssl_is_valid_trust_anchor_list(Span<const uint8_t> in) {
  CBS ids = in;
  while (CBS_len(&ids) > 0) {
    CBS id;
    if (!CBS_get_u8_length_prefixed(&ids, &id) ||
        CBS_len(&id) == 0) {
      return false;
    }
  }
  return true;
}

static bool ext_trust_anchors_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                              CBB *out_compressible,
                                              ssl_client_hello_type_t type) {
  if (!hs->config->requested_trust_anchors.has_value()) {
    return true;
  }

  CBB contents, list;
  if (!CBB_add_u16(out_compressible, TLSEXT_TYPE_trust_anchors) ||
      !CBB_add_u16_length_prefixed(out_compressible, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &list) ||
      !CBB_add_bytes(&list, hs->config->requested_trust_anchors->data(),
                     hs->config->requested_trust_anchors->size()) ||
      !CBB_flush(out_compressible)) {
    return false;
  }
  return true;
}

static bool ext_trust_anchors_parse_clienthello(SSL_HANDSHAKE *hs,
                                                uint8_t *out_alert,
                                                CBS *contents) {
  if (contents == nullptr || ssl_protocol_version(hs->ssl) < TLS1_3_VERSION) {
    return true;
  }

  CBS child;
  if (!CBS_get_u16_length_prefixed(contents, &child) ||
      !ssl_is_valid_trust_anchor_list(child)) {
    *out_alert = SSL_AD_DECODE_ERROR;
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }

  hs->peer_requested_trust_anchors.emplace();
  if (!hs->peer_requested_trust_anchors->CopyFrom(child)) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return false;
  }
  return true;
}

static bool ext_trust_anchors_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  SSL *const ssl = hs->ssl;
  const auto &creds = hs->config->cert->credentials;
  if (ssl_protocol_version(ssl) < TLS1_3_VERSION ||

      std::none_of(creds.begin(), creds.end(), [](const auto &cred) {
        return !cred->trust_anchor_id.empty();
      })) {
    return true;
  }
  CBB contents, list;
  if (!CBB_add_u16(out, TLSEXT_TYPE_trust_anchors) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &list)) {
    return false;
  }
  for (const auto &cred : creds) {
    if (!cred->trust_anchor_id.empty()) {
      CBB child;
      if (!CBB_add_u8_length_prefixed(&list, &child) ||
          !CBB_add_bytes(&child, cred->trust_anchor_id.data(),
                         cred->trust_anchor_id.size()) ||
          !CBB_flush(&list)) {
        return false;
      }
    }
  }
  return CBB_flush(out);
}

static bool ext_trust_anchors_parse_serverhello(SSL_HANDSHAKE *hs,
                                                uint8_t *out_alert,
                                                CBS *contents) {
  if (contents == nullptr) {
    return true;
  }

  if (ssl_protocol_version(hs->ssl) < TLS1_3_VERSION) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
    *out_alert = SSL_AD_UNSUPPORTED_EXTENSION;
    return false;
  }

  CBS child;
  if (!CBS_get_u16_length_prefixed(contents, &child) ||

      CBS_len(&child) == 0 ||
      !ssl_is_valid_trust_anchor_list(child)) {
    *out_alert = SSL_AD_DECODE_ERROR;
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }

  if (!hs->peer_available_trust_anchors.CopyFrom(child)) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return false;
  }
  return true;
}

static bool ext_quic_transport_params_add_clienthello_impl(
    const SSL_HANDSHAKE *hs, CBB *out, bool use_legacy_codepoint) {
  if (hs->config->quic_transport_params.empty() && !SSL_is_quic(hs->ssl)) {
    return true;
  }
  if (hs->config->quic_transport_params.empty() || !SSL_is_quic(hs->ssl)) {

    OPENSSL_PUT_ERROR(SSL, SSL_R_QUIC_TRANSPORT_PARAMETERS_MISCONFIGURED);
    return false;
  }
  assert(hs->min_version > TLS1_2_VERSION);
  if (use_legacy_codepoint != hs->config->quic_use_legacy_codepoint) {

    return true;
  }

  uint16_t extension_type = TLSEXT_TYPE_quic_transport_parameters;
  if (hs->config->quic_use_legacy_codepoint) {
    extension_type = TLSEXT_TYPE_quic_transport_parameters_legacy;
  }

  CBB contents;
  if (!CBB_add_u16(out, extension_type) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_bytes(&contents, hs->config->quic_transport_params.data(),
                     hs->config->quic_transport_params.size()) ||
      !CBB_flush(out)) {
    return false;
  }
  return true;
}

static bool ext_quic_transport_params_add_clienthello(
    const SSL_HANDSHAKE *hs, CBB *out, CBB *out_compressible,
    ssl_client_hello_type_t type) {
  return ext_quic_transport_params_add_clienthello_impl(
      hs, out_compressible,                          false);
}

static bool ext_quic_transport_params_add_clienthello_legacy(
    const SSL_HANDSHAKE *hs, CBB *out, CBB *out_compressible,
    ssl_client_hello_type_t type) {
  return ext_quic_transport_params_add_clienthello_impl(
      hs, out_compressible,                          true);
}

static bool ext_quic_transport_params_parse_serverhello_impl(
    SSL_HANDSHAKE *hs, uint8_t *out_alert, CBS *contents,
    bool used_legacy_codepoint) {
  SSL *const ssl = hs->ssl;
  if (contents == nullptr) {
    if (used_legacy_codepoint != hs->config->quic_use_legacy_codepoint) {

      return true;
    }
    if (!SSL_is_quic(ssl)) {
      return true;
    }
    *out_alert = SSL_AD_MISSING_EXTENSION;
    return false;
  }

  assert(SSL_is_quic(ssl));
  assert(ssl_protocol_version(ssl) == TLS1_3_VERSION);
  assert(used_legacy_codepoint == hs->config->quic_use_legacy_codepoint);
  return ssl->s3->peer_quic_transport_params.CopyFrom(*contents);
}

static bool ext_quic_transport_params_parse_serverhello(SSL_HANDSHAKE *hs,
                                                        uint8_t *out_alert,
                                                        CBS *contents) {
  return ext_quic_transport_params_parse_serverhello_impl(
      hs, out_alert, contents,                           false);
}

static bool ext_quic_transport_params_parse_serverhello_legacy(
    SSL_HANDSHAKE *hs, uint8_t *out_alert, CBS *contents) {
  return ext_quic_transport_params_parse_serverhello_impl(
      hs, out_alert, contents,                           true);
}

static bool ext_quic_transport_params_parse_clienthello_impl(
    SSL_HANDSHAKE *hs, uint8_t *out_alert, CBS *contents,
    bool used_legacy_codepoint) {
  SSL *const ssl = hs->ssl;
  if (!contents) {
    if (!SSL_is_quic(ssl)) {
      if (hs->config->quic_transport_params.empty()) {
        return true;
      }

      OPENSSL_PUT_ERROR(SSL, SSL_R_QUIC_TRANSPORT_PARAMETERS_MISCONFIGURED);
      *out_alert = SSL_AD_INTERNAL_ERROR;
      return false;
    }
    if (used_legacy_codepoint != hs->config->quic_use_legacy_codepoint) {

      return true;
    }
    *out_alert = SSL_AD_MISSING_EXTENSION;
    return false;
  }
  if (!SSL_is_quic(ssl)) {
    if (used_legacy_codepoint) {

      return true;
    }

    *out_alert = SSL_AD_UNSUPPORTED_EXTENSION;
    return false;
  }
  assert(ssl_protocol_version(ssl) == TLS1_3_VERSION);
  if (used_legacy_codepoint != hs->config->quic_use_legacy_codepoint) {

    return true;
  }
  return ssl->s3->peer_quic_transport_params.CopyFrom(*contents);
}

static bool ext_quic_transport_params_parse_clienthello(SSL_HANDSHAKE *hs,
                                                        uint8_t *out_alert,
                                                        CBS *contents) {
  return ext_quic_transport_params_parse_clienthello_impl(
      hs, out_alert, contents,                           false);
}

static bool ext_quic_transport_params_parse_clienthello_legacy(
    SSL_HANDSHAKE *hs, uint8_t *out_alert, CBS *contents) {
  return ext_quic_transport_params_parse_clienthello_impl(
      hs, out_alert, contents,                           true);
}

static bool ext_quic_transport_params_add_serverhello_impl(
    SSL_HANDSHAKE *hs, CBB *out, bool use_legacy_codepoint) {
  if (!SSL_is_quic(hs->ssl) && use_legacy_codepoint) {

    return true;
  }
  assert(SSL_is_quic(hs->ssl));
  if (hs->config->quic_transport_params.empty()) {

    OPENSSL_PUT_ERROR(SSL, SSL_R_QUIC_TRANSPORT_PARAMETERS_MISCONFIGURED);
    return false;
  }
  if (use_legacy_codepoint != hs->config->quic_use_legacy_codepoint) {

    return true;
  }

  uint16_t extension_type = TLSEXT_TYPE_quic_transport_parameters;
  if (hs->config->quic_use_legacy_codepoint) {
    extension_type = TLSEXT_TYPE_quic_transport_parameters_legacy;
  }

  CBB contents;
  if (!CBB_add_u16(out, extension_type) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_bytes(&contents, hs->config->quic_transport_params.data(),
                     hs->config->quic_transport_params.size()) ||
      !CBB_flush(out)) {
    return false;
  }

  return true;
}

static bool ext_quic_transport_params_add_serverhello(SSL_HANDSHAKE *hs,
                                                      CBB *out) {
  return ext_quic_transport_params_add_serverhello_impl(
      hs, out,                          false);
}

static bool ext_quic_transport_params_add_serverhello_legacy(SSL_HANDSHAKE *hs,
                                                             CBB *out) {
  return ext_quic_transport_params_add_serverhello_impl(
      hs, out,                          true);
}

static bool ext_delegated_credential_add_clienthello(
    const SSL_HANDSHAKE *hs, CBB *out, CBB *out_compressible,
    ssl_client_hello_type_t type) {
  return true;
}

static bool ext_delegated_credential_parse_clienthello(SSL_HANDSHAKE *hs,
                                                       uint8_t *out_alert,
                                                       CBS *contents) {
  if (contents == nullptr || ssl_protocol_version(hs->ssl) < TLS1_3_VERSION) {

    return true;
  }

  CBS sigalg_list;
  if (!CBS_get_u16_length_prefixed(contents, &sigalg_list) ||
      CBS_len(&sigalg_list) == 0 ||
      CBS_len(contents) != 0 ||
      !parse_u16_array(&sigalg_list, &hs->peer_delegated_credential_sigalgs)) {
    return false;
  }

  return true;
}

static bool cert_compression_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                             CBB *out_compressible,
                                             ssl_client_hello_type_t type) {
  bool first = true;
  CBB contents, algs;

  for (const auto &alg : hs->ssl->ctx->cert_compression_algs) {
    if (alg.decompress == nullptr) {
      continue;
    }

    if (first &&
        (!CBB_add_u16(out_compressible, TLSEXT_TYPE_cert_compression) ||
         !CBB_add_u16_length_prefixed(out_compressible, &contents) ||
         !CBB_add_u8_length_prefixed(&contents, &algs))) {
      return false;
    }
    first = false;
    if (!CBB_add_u16(&algs, alg.alg_id)) {
      return false;
    }
  }

  return first || CBB_flush(out_compressible);
}

static bool cert_compression_parse_serverhello(SSL_HANDSHAKE *hs,
                                               uint8_t *out_alert,
                                               CBS *contents) {
  if (contents == nullptr) {
    return true;
  }

  return false;
}

static bool cert_compression_parse_clienthello(SSL_HANDSHAKE *hs,
                                               uint8_t *out_alert,
                                               CBS *contents) {
  if (contents == nullptr) {
    return true;
  }

  const SSL_CTX *ctx = hs->ssl->ctx.get();
  const size_t num_algs = ctx->cert_compression_algs.size();

  CBS alg_ids;
  if (!CBS_get_u8_length_prefixed(contents, &alg_ids) ||
      CBS_len(contents) != 0 ||
      CBS_len(&alg_ids) == 0 ||
      CBS_len(&alg_ids) % 2 == 1) {
    return false;
  }

  const size_t num_given_alg_ids = CBS_len(&alg_ids) / 2;
  Array<uint16_t> given_alg_ids;
  if (!given_alg_ids.InitForOverwrite(num_given_alg_ids)) {
    return false;
  }

  size_t best_index = num_algs;
  size_t given_alg_idx = 0;

  while (CBS_len(&alg_ids) > 0) {
    uint16_t alg_id;
    if (!CBS_get_u16(&alg_ids, &alg_id)) {
      return false;
    }

    given_alg_ids[given_alg_idx++] = alg_id;

    for (size_t i = 0; i < num_algs; i++) {
      const auto &alg = ctx->cert_compression_algs[i];
      if (alg.alg_id == alg_id && alg.compress != nullptr) {
        if (i < best_index) {
          best_index = i;
        }
        break;
      }
    }
  }

  qsort(given_alg_ids.data(), given_alg_ids.size(), sizeof(uint16_t),
        compare_uint16_t);
  for (size_t i = 1; i < num_given_alg_ids; i++) {
    if (given_alg_ids[i - 1] == given_alg_ids[i]) {
      return false;
    }
  }

  if (best_index < num_algs &&
      ssl_protocol_version(hs->ssl) >= TLS1_3_VERSION) {
    hs->cert_compression_negotiated = true;
    hs->cert_compression_alg_id = ctx->cert_compression_algs[best_index].alg_id;
  }

  return true;
}

static bool cert_compression_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  return true;
}

bool ssl_setup_pake_shares(SSL_HANDSHAKE *hs) {
  hs->pake_share_bytes.Reset();
  if (hs->max_version < TLS1_3_VERSION) {
    return true;
  }

  const auto &creds = hs->config->cert->credentials;
  if (std::none_of(creds.begin(), creds.end(), [](const auto &cred) {
        return cred->type == SSLCredentialType::kSPAKE2PlusV1Client;
      })) {

    return true;
  }

  if (creds.size() != 1u) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNSUPPORTED_CREDENTIAL_LIST);
    return false;
  }
  SSL_CREDENTIAL *cred = creds[0].get();
  assert(cred->type == SSLCredentialType::kSPAKE2PlusV1Client);

  hs->pake_prover = MakeUnique<spake2plus::Prover>();
  uint8_t prover_share[spake2plus::kShareSize];
  if (hs->pake_prover == nullptr ||
      !hs->pake_prover->Init(cred->pake_context, cred->client_identity,
                             cred->server_identity, cred->password_verifier_w0,
                             cred->password_verifier_w1) ||
      !hs->pake_prover->GenerateShare(prover_share)) {
    return false;
  }

  hs->credential = UpRef(cred);

  bssl::ScopedCBB cbb;
  CBB shares, client_identity, server_identity, pake_message;
  if (!CBB_init(cbb.get(), 64) ||
      !CBB_add_u16_length_prefixed(cbb.get(), &client_identity) ||
      !CBB_add_bytes(&client_identity, cred->client_identity.data(),
                     cred->client_identity.size()) ||
      !CBB_add_u16_length_prefixed(cbb.get(), &server_identity) ||
      !CBB_add_bytes(&server_identity, cred->server_identity.data(),
                     cred->server_identity.size()) ||
      !CBB_add_u16_length_prefixed(cbb.get(), &shares) ||
      !CBB_add_u16(&shares, SSL_PAKE_SPAKE2PLUSV1) ||
      !CBB_add_u16_length_prefixed(&shares, &pake_message) ||
      !CBB_add_bytes(&pake_message, prover_share, sizeof(prover_share))) {
    return false;
  }

  return CBBFinishArray(cbb.get(), &hs->pake_share_bytes);
}

static bool ext_pake_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                     CBB *out_compressible,
                                     ssl_client_hello_type_t type) {
  if (hs->pake_share_bytes.empty()) {
    return true;
  }

  CBB pake_share_bytes;
  if (!CBB_add_u16(out_compressible, TLSEXT_TYPE_pake) ||
      !CBB_add_u16_length_prefixed(out_compressible, &pake_share_bytes) ||
      !CBB_add_bytes(&pake_share_bytes, hs->pake_share_bytes.data(),
                     hs->pake_share_bytes.size()) ||
      !CBB_flush(out_compressible)) {
    return false;
  }

  return true;
}

bool ssl_ext_pake_parse_serverhello(SSL_HANDSHAKE *hs,
                                    Array<uint8_t> *out_secret,
                                    uint8_t *out_alert, CBS *contents) {
  *out_alert = SSL_AD_DECODE_ERROR;

  if (!hs->pake_prover) {

    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return false;
  }

  CBS pake_msg;
  uint16_t named_pake;
  if (!CBS_get_u16(contents, &named_pake) ||
      !CBS_get_u16_length_prefixed(contents, &pake_msg) ||
      CBS_len(contents) != 0 ||
      named_pake != SSL_PAKE_SPAKE2PLUSV1) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }

  if (CBS_len(&pake_msg) != spake2plus::kShareSize + spake2plus::kConfirmSize) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return false;
  }
  Span<const uint8_t> pake_msg_span = pake_msg;

  if (!hs->credential->HasPAKEAttempts()) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_PAKE_EXHAUSTED);
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return false;
  }

  uint8_t prover_confirm[spake2plus::kConfirmSize];
  uint8_t prover_secret[spake2plus::kSecretSize];
  if (!hs->pake_prover->ComputeConfirmation(
          prover_confirm, prover_secret,
          pake_msg_span.subspan(0, spake2plus::kShareSize),
          pake_msg_span.subspan(spake2plus::kShareSize))) {

    hs->credential->ClaimPAKEAttempt();
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    *out_alert = SSL_AD_ILLEGAL_PARAMETER;
    return false;
  }

  Array<uint8_t> secret;
  if (!secret.CopyFrom(prover_secret)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return false;
  }

  *out_secret = std::move(secret);
  return true;
}

static bool ext_pake_parse_clienthello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                       CBS *contents) {
  if (contents == nullptr) {
    return true;
  }

  *out_alert = SSL_AD_DECODE_ERROR;
  CBS client_identity, server_identity, shares;
  if (!CBS_get_u16_length_prefixed(contents, &client_identity) ||
      !CBS_get_u16_length_prefixed(contents, &server_identity) ||
      !CBS_get_u16_length_prefixed(contents, &shares) ||
      CBS_len(contents) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return false;
  }

  uint16_t last_named_pake = 0;
  for (size_t i = 0; CBS_len(&shares) > 0; i++) {
    uint16_t pake_id;
    CBS message;
    if (!CBS_get_u16(&shares, &pake_id) ||
        !CBS_get_u16_length_prefixed(&shares, &message)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      return false;
    }

    if (i > 0 && last_named_pake >= pake_id) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      return false;
    }
    last_named_pake = pake_id;

    if (pake_id != SSL_PAKE_SPAKE2PLUSV1) {
      continue;
    }

    hs->pake_share = MakeUnique<SSLPAKEShare>();
    if (hs->pake_share == nullptr ||
        !hs->pake_share->client_identity.CopyFrom(client_identity) ||
        !hs->pake_share->server_identity.CopyFrom(server_identity) ||
        !hs->pake_share->pake_message.CopyFrom(message)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return false;
    }
    hs->pake_share->named_pake = pake_id;
  }

  return true;
}

bool ssl_get_local_application_settings(const SSL_HANDSHAKE *hs,
                                        Span<const uint8_t> *out_settings,
                                        Span<const uint8_t> protocol) {
  for (const ALPSConfig &config : hs->config->alps_configs) {
    if (protocol == config.protocol) {
      *out_settings = config.settings;
      return true;
    }
  }
  return false;
}

static bool ext_alps_add_clienthello_impl(const SSL_HANDSHAKE *hs, CBB *out,
                                          CBB *out_compressible,
                                          ssl_client_hello_type_t type,
                                          bool use_new_codepoint) {
  const SSL *const ssl = hs->ssl;
  if (
      hs->max_version < TLS1_3_VERSION ||

      hs->config->alpn_client_proto_list.empty() ||

      hs->config->alps_configs.empty() ||

      ssl->s3->initial_handshake_complete) {
    return true;
  }

  if (use_new_codepoint != hs->config->alps_use_new_codepoint) {

    return true;
  }

  uint16_t extension_type = TLSEXT_TYPE_application_settings_old;
  if (hs->config->alps_use_new_codepoint) {
    extension_type = TLSEXT_TYPE_application_settings;
  }

  CBB contents, proto_list, proto;
  if (!CBB_add_u16(out_compressible, extension_type) ||
      !CBB_add_u16_length_prefixed(out_compressible, &contents) ||
      !CBB_add_u16_length_prefixed(&contents, &proto_list)) {
    return false;
  }

  for (const ALPSConfig &config : hs->config->alps_configs) {
    if (!CBB_add_u8_length_prefixed(&proto_list, &proto) ||
        !CBB_add_bytes(&proto, config.protocol.data(),
                       config.protocol.size())) {
      return false;
    }
  }

  return CBB_flush(out_compressible);
}

static bool ext_alps_add_clienthello(const SSL_HANDSHAKE *hs, CBB *out,
                                     CBB *out_compressible,
                                     ssl_client_hello_type_t type) {
  return ext_alps_add_clienthello_impl(hs, out, out_compressible, type,
                                                             true);
}

static bool ext_alps_add_clienthello_old(const SSL_HANDSHAKE *hs, CBB *out,
                                         CBB *out_compressible,
                                         ssl_client_hello_type_t type) {
  return ext_alps_add_clienthello_impl(hs, out, out_compressible, type,
                                                             false);
}

static bool ext_alps_parse_serverhello_impl(SSL_HANDSHAKE *hs,
                                            uint8_t *out_alert, CBS *contents,
                                            bool use_new_codepoint) {
  SSL *const ssl = hs->ssl;
  if (contents == nullptr) {
    return true;
  }

  assert(!ssl->s3->initial_handshake_complete);
  assert(!hs->config->alpn_client_proto_list.empty());
  assert(!hs->config->alps_configs.empty());
  assert(use_new_codepoint == hs->config->alps_use_new_codepoint);

  if (ssl_protocol_version(ssl) < TLS1_3_VERSION) {
    *out_alert = SSL_AD_UNSUPPORTED_EXTENSION;
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
    return false;
  }

  if (!hs->new_session->peer_application_settings.CopyFrom(*contents)) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return false;
  }

  hs->new_session->has_application_settings = true;
  return true;
}

static bool ext_alps_parse_serverhello(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                                       CBS *contents) {
  return ext_alps_parse_serverhello_impl(hs, out_alert, contents,
                                                               true);
}

static bool ext_alps_parse_serverhello_old(SSL_HANDSHAKE *hs,
                                           uint8_t *out_alert, CBS *contents) {
  return ext_alps_parse_serverhello_impl(hs, out_alert, contents,
                                                               false);
}

static bool ext_alps_add_serverhello_impl(SSL_HANDSHAKE *hs, CBB *out,
                                          bool use_new_codepoint) {
  SSL *const ssl = hs->ssl;

  if (hs->new_session == nullptr ||
      !hs->new_session->has_application_settings ||
      ssl->s3->early_data_accepted) {
    return true;
  }

  if (use_new_codepoint != hs->config->alps_use_new_codepoint) {

    return true;
  }

  uint16_t extension_type = TLSEXT_TYPE_application_settings_old;
  if (hs->config->alps_use_new_codepoint) {
    extension_type = TLSEXT_TYPE_application_settings;
  }

  CBB contents;
  if (!CBB_add_u16(out, extension_type) ||
      !CBB_add_u16_length_prefixed(out, &contents) ||
      !CBB_add_bytes(&contents,
                     hs->new_session->local_application_settings.data(),
                     hs->new_session->local_application_settings.size()) ||
      !CBB_flush(out)) {
    return false;
  }

  return true;
}

static bool ext_alps_add_serverhello(SSL_HANDSHAKE *hs, CBB *out) {
  return ext_alps_add_serverhello_impl(hs, out,                       true);
}

static bool ext_alps_add_serverhello_old(SSL_HANDSHAKE *hs, CBB *out) {
  return ext_alps_add_serverhello_impl(hs, out,                       false);
}

bool ssl_negotiate_alps(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                        const SSL_CLIENT_HELLO *client_hello) {
  SSL *const ssl = hs->ssl;
  if (ssl->s3->alpn_selected.empty()) {
    return true;
  }

  CBS alps_contents;
  Span<const uint8_t> settings;
  uint16_t extension_type = TLSEXT_TYPE_application_settings_old;
  if (hs->config->alps_use_new_codepoint) {
    extension_type = TLSEXT_TYPE_application_settings;
  }
  if (ssl_protocol_version(ssl) >= TLS1_3_VERSION &&
      ssl_get_local_application_settings(hs, &settings,
                                         ssl->s3->alpn_selected) &&
      ssl_client_hello_get_extension(client_hello, &alps_contents,
                                     extension_type)) {

    bool found = false;
    CBS alps_list;
    if (!CBS_get_u16_length_prefixed(&alps_contents, &alps_list) ||
        CBS_len(&alps_contents) != 0 ||
        CBS_len(&alps_list) == 0) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      *out_alert = SSL_AD_DECODE_ERROR;
      return false;
    }
    while (CBS_len(&alps_list) > 0) {
      CBS protocol_name;
      if (!CBS_get_u8_length_prefixed(&alps_list, &protocol_name) ||

          CBS_len(&protocol_name) == 0) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
        *out_alert = SSL_AD_DECODE_ERROR;
        return false;
      }
      if (protocol_name == Span(ssl->s3->alpn_selected)) {
        found = true;
      }
    }

    if (found) {
      hs->new_session->has_application_settings = true;
      if (!hs->new_session->local_application_settings.CopyFrom(settings)) {
        *out_alert = SSL_AD_INTERNAL_ERROR;
        return false;
      }
    }
  }

  return true;
}

static const struct tls_extension kExtensions[] = {
    {
        TLSEXT_TYPE_server_name,
        ext_sni_add_clienthello,
        ext_sni_parse_serverhello,
        ext_sni_parse_clienthello,
        ext_sni_add_serverhello,
    },
    {
        TLSEXT_TYPE_encrypted_client_hello,
        ext_ech_add_clienthello,
        ext_ech_parse_serverhello,
        ext_ech_parse_clienthello,
        ext_ech_add_serverhello,
    },
    {
        TLSEXT_TYPE_extended_master_secret,
        ext_ems_add_clienthello,
        ext_ems_parse_serverhello,
        ext_ems_parse_clienthello,
        ext_ems_add_serverhello,
    },
    {
        TLSEXT_TYPE_renegotiate,
        ext_ri_add_clienthello,
        ext_ri_parse_serverhello,
        ext_ri_parse_clienthello,
        ext_ri_add_serverhello,
    },
    {
        TLSEXT_TYPE_supported_groups,
        ext_supported_groups_add_clienthello,
        ext_supported_groups_parse_serverhello,
        ext_supported_groups_parse_clienthello,
        dont_add_serverhello,
    },
    {
        TLSEXT_TYPE_ec_point_formats,
        ext_ec_point_add_clienthello,
        ext_ec_point_parse_serverhello,
        ext_ec_point_parse_clienthello,
        ext_ec_point_add_serverhello,
    },
    {
        TLSEXT_TYPE_session_ticket,
        ext_ticket_add_clienthello,
        ext_ticket_parse_serverhello,

        ignore_parse_clienthello,
        ext_ticket_add_serverhello,
    },
    {
        TLSEXT_TYPE_application_layer_protocol_negotiation,
        ext_alpn_add_clienthello,
        ext_alpn_parse_serverhello,

        ignore_parse_clienthello,
        ext_alpn_add_serverhello,
    },
    {
        TLSEXT_TYPE_status_request,
        ext_ocsp_add_clienthello,
        ext_ocsp_parse_serverhello,
        ext_ocsp_parse_clienthello,
        ext_ocsp_add_serverhello,
    },
    {
        TLSEXT_TYPE_signature_algorithms,
        ext_sigalgs_add_clienthello,
        forbid_parse_serverhello,
        ext_sigalgs_parse_clienthello,
        dont_add_serverhello,
    },
    {
        TLSEXT_TYPE_next_proto_neg,
        ext_npn_add_clienthello,
        ext_npn_parse_serverhello,
        ext_npn_parse_clienthello,
        ext_npn_add_serverhello,
    },
    {
        TLSEXT_TYPE_certificate_timestamp,
        ext_sct_add_clienthello,
        ext_sct_parse_serverhello,
        ext_sct_parse_clienthello,
        ext_sct_add_serverhello,
    },
    {
        TLSEXT_TYPE_channel_id,
        ext_channel_id_add_clienthello,
        ext_channel_id_parse_serverhello,
        ext_channel_id_parse_clienthello,
        ext_channel_id_add_serverhello,
    },
    {
        TLSEXT_TYPE_srtp,
        ext_srtp_add_clienthello,
        ext_srtp_parse_serverhello,
        ext_srtp_parse_clienthello,
        ext_srtp_add_serverhello,
    },
    {
        TLSEXT_TYPE_key_share,
        ext_key_share_add_clienthello,
        forbid_parse_serverhello,
        ignore_parse_clienthello,
        dont_add_serverhello,
    },
    {
        TLSEXT_TYPE_psk_key_exchange_modes,
        ext_psk_key_exchange_modes_add_clienthello,
        forbid_parse_serverhello,
        ext_psk_key_exchange_modes_parse_clienthello,
        dont_add_serverhello,
    },
    {
        TLSEXT_TYPE_early_data,
        ext_early_data_add_clienthello,
        ext_early_data_parse_serverhello,
        ext_early_data_parse_clienthello,
        ext_early_data_add_serverhello,
    },
    {
        TLSEXT_TYPE_supported_versions,
        ext_supported_versions_add_clienthello,
        forbid_parse_serverhello,
        ignore_parse_clienthello,
        dont_add_serverhello,
    },
    {
        TLSEXT_TYPE_cookie,
        ext_cookie_add_clienthello,
        forbid_parse_serverhello,
        ignore_parse_clienthello,
        dont_add_serverhello,
    },
    {
        TLSEXT_TYPE_quic_transport_parameters,
        ext_quic_transport_params_add_clienthello,
        ext_quic_transport_params_parse_serverhello,
        ext_quic_transport_params_parse_clienthello,
        ext_quic_transport_params_add_serverhello,
    },
    {
        TLSEXT_TYPE_quic_transport_parameters_legacy,
        ext_quic_transport_params_add_clienthello_legacy,
        ext_quic_transport_params_parse_serverhello_legacy,
        ext_quic_transport_params_parse_clienthello_legacy,
        ext_quic_transport_params_add_serverhello_legacy,
    },
    {
        TLSEXT_TYPE_cert_compression,
        cert_compression_add_clienthello,
        cert_compression_parse_serverhello,
        cert_compression_parse_clienthello,
        cert_compression_add_serverhello,
    },
    {
        TLSEXT_TYPE_delegated_credential,
        ext_delegated_credential_add_clienthello,
        forbid_parse_serverhello,
        ext_delegated_credential_parse_clienthello,
        dont_add_serverhello,
    },
    {
        TLSEXT_TYPE_application_settings,
        ext_alps_add_clienthello,
        ext_alps_parse_serverhello,

        ignore_parse_clienthello,
        ext_alps_add_serverhello,
    },
    {
        TLSEXT_TYPE_application_settings_old,
        ext_alps_add_clienthello_old,
        ext_alps_parse_serverhello_old,

        ignore_parse_clienthello,
        ext_alps_add_serverhello_old,
    },
    {
        TLSEXT_TYPE_certificate_authorities,
        ext_certificate_authorities_add_clienthello,
        forbid_parse_serverhello,
        ext_certificate_authorities_parse_clienthello,
        dont_add_serverhello,
    },
    {
        TLSEXT_TYPE_pake,
        ext_pake_add_clienthello,

        forbid_parse_serverhello,
        ext_pake_parse_clienthello,
        dont_add_serverhello,
    },
    {
        TLSEXT_TYPE_trust_anchors,
        ext_trust_anchors_add_clienthello,
        ext_trust_anchors_parse_serverhello,
        ext_trust_anchors_parse_clienthello,
        ext_trust_anchors_add_serverhello,
    },
};

#define kNumExtensions (sizeof(kExtensions) / sizeof(struct tls_extension))

static_assert(kNumExtensions <=
                  sizeof(((SSL_HANDSHAKE *)NULL)->extensions.sent) * 8,
              "too many extensions for sent bitset");
static_assert(kNumExtensions <=
                  sizeof(((SSL_HANDSHAKE *)NULL)->extensions.received) * 8,
              "too many extensions for received bitset");

bool ssl_setup_extension_permutation(SSL_HANDSHAKE *hs) {
  if (!hs->config->permute_extensions) {
    return true;
  }

  static_assert(kNumExtensions <= UINT8_MAX,
                "extensions_permutation type is too small");
  uint32_t seeds[kNumExtensions - 1];
  Array<uint8_t> permutation;
  if (!RAND_bytes(reinterpret_cast<uint8_t *>(seeds), sizeof(seeds)) ||
      !permutation.InitForOverwrite(kNumExtensions)) {
    return false;
  }
  for (size_t i = 0; i < kNumExtensions; i++) {
    permutation[i] = i;
  }
  for (size_t i = kNumExtensions - 1; i > 0; i--) {

    std::swap(permutation[i], permutation[seeds[i - 1] % (i + 1)]);
  }
  hs->extension_permutation = std::move(permutation);
  return true;
}

static const struct tls_extension *tls_extension_find(uint32_t *out_index,
                                                      uint16_t value) {
  unsigned i;
  for (i = 0; i < kNumExtensions; i++) {
    if (kExtensions[i].value == value) {
      *out_index = i;
      return &kExtensions[i];
    }
  }

  return NULL;
}

static bool add_padding_extension(CBB *cbb, uint16_t ext, size_t len) {
  CBB child;
  if (!CBB_add_u16(cbb, ext) ||
      !CBB_add_u16_length_prefixed(cbb, &child) ||
      !CBB_add_zeros(&child, len)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }
  return CBB_flush(cbb);
}

static bool ssl_add_clienthello_tlsext_inner(SSL_HANDSHAKE *hs, CBB *out,
                                             CBB *out_encoded,
                                             bool *out_needs_psk_binder) {

  SSL *const ssl = hs->ssl;
  bssl::ScopedCBB compressed, outer_extensions;
  CBB extensions, extensions_encoded;
  if (!CBB_add_u16_length_prefixed(out, &extensions) ||
      !CBB_add_u16_length_prefixed(out_encoded, &extensions_encoded) ||
      !CBB_init(compressed.get(), 64) ||
      !CBB_init(outer_extensions.get(), 64)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  hs->inner_extensions_sent = 0;

  if (ssl->ctx->grease_enabled) {

    uint16_t grease_ext = ssl_get_grease_value(hs, ssl_grease_extension1);
    if (!add_padding_extension(compressed.get(), grease_ext, 0) ||
        !CBB_add_u16(outer_extensions.get(), grease_ext)) {
      return false;
    }
  }

  for (size_t unpermuted = 0; unpermuted < kNumExtensions; unpermuted++) {
    size_t i = hs->extension_permutation.empty()
                   ? unpermuted
                   : hs->extension_permutation[unpermuted];
    const size_t len_before = CBB_len(&extensions);
    const size_t len_compressed_before = CBB_len(compressed.get());
    if (!kExtensions[i].add_clienthello(hs, &extensions, compressed.get(),
                                        ssl_client_hello_inner)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_ERROR_ADDING_EXTENSION);
      ERR_add_error_dataf("extension %u", (unsigned)kExtensions[i].value);
      return false;
    }

    const size_t bytes_written = CBB_len(&extensions) - len_before;
    const size_t bytes_written_compressed =
        CBB_len(compressed.get()) - len_compressed_before;

    assert(bytes_written == 0 || bytes_written_compressed == 0);
    if (bytes_written != 0 || bytes_written_compressed != 0) {
      hs->inner_extensions_sent |= (1u << i);
    }

    if (bytes_written_compressed != 0 &&
        !CBB_add_u16(outer_extensions.get(), kExtensions[i].value)) {
      return false;
    }
  }

  if (ssl->ctx->grease_enabled) {

    uint16_t grease_ext = ssl_get_grease_value(hs, ssl_grease_extension2);
    if (!add_padding_extension(compressed.get(), grease_ext, 1) ||
        !CBB_add_u16(outer_extensions.get(), grease_ext)) {
      return false;
    }
  }

  if (!CBB_add_bytes(&extensions_encoded, CBB_data(&extensions),
                     CBB_len(&extensions))) {
    return false;
  }

  if (CBB_len(compressed.get()) != 0) {
    CBB extension, child;

    if (!CBB_add_bytes(&extensions, CBB_data(compressed.get()),
                       CBB_len(compressed.get())) ||

        !CBB_add_u16(&extensions_encoded, TLSEXT_TYPE_ech_outer_extensions) ||
        !CBB_add_u16_length_prefixed(&extensions_encoded, &extension) ||
        !CBB_add_u8_length_prefixed(&extension, &child) ||
        !CBB_add_bytes(&child, CBB_data(outer_extensions.get()),
                       CBB_len(outer_extensions.get())) ||
        !CBB_flush(&extensions_encoded)) {
      return false;
    }
  }

  const size_t len_before = CBB_len(&extensions);
  if (!ext_pre_shared_key_add_clienthello(hs, &extensions, out_needs_psk_binder,
                                          ssl_client_hello_inner) ||
      !CBB_add_bytes(&extensions_encoded, CBB_data(&extensions) + len_before,
                     CBB_len(&extensions) - len_before) ||
      !CBB_flush(out) ||
      !CBB_flush(out_encoded)) {
    return false;
  }

  return true;
}

bool ssl_add_clienthello_tlsext(SSL_HANDSHAKE *hs, CBB *out, CBB *out_encoded,
                                bool *out_needs_psk_binder,
                                ssl_client_hello_type_t type,
                                size_t header_len) {
  *out_needs_psk_binder = false;

  if (type == ssl_client_hello_inner) {
    return ssl_add_clienthello_tlsext_inner(hs, out, out_encoded,
                                            out_needs_psk_binder);
  }

  assert(out_encoded == nullptr);
  SSL *const ssl = hs->ssl;
  CBB extensions;
  if (!CBB_add_u16_length_prefixed(out, &extensions)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  hs->extensions.sent = 0;

  if (ssl->ctx->grease_enabled &&
      !add_padding_extension(
          &extensions, ssl_get_grease_value(hs, ssl_grease_extension1), 0)) {
    return false;
  }

  bool last_was_empty = false;
  for (size_t unpermuted = 0; unpermuted < kNumExtensions; unpermuted++) {
    size_t i = hs->extension_permutation.empty()
                   ? unpermuted
                   : hs->extension_permutation[unpermuted];
    const size_t len_before = CBB_len(&extensions);
    if (!kExtensions[i].add_clienthello(hs, &extensions, &extensions, type)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_ERROR_ADDING_EXTENSION);
      ERR_add_error_dataf("extension %u", (unsigned)kExtensions[i].value);
      return false;
    }

    const size_t bytes_written = CBB_len(&extensions) - len_before;
    if (bytes_written != 0) {
      hs->extensions.sent |= (1u << i);
    }

    last_was_empty = (bytes_written == 4);
  }

  if (ssl->ctx->grease_enabled) {

    if (!add_padding_extension(
            &extensions, ssl_get_grease_value(hs, ssl_grease_extension2), 1)) {
      return false;
    }
    last_was_empty = false;
  }

  size_t psk_extension_len = ext_pre_shared_key_clienthello_length(hs, type);
  if (!SSL_is_dtls(ssl) && !SSL_is_quic(ssl) &&
      !ssl->s3->used_hello_retry_request) {
    header_len +=
        SSL3_HM_HEADER_LENGTH + 2 + CBB_len(&extensions) + psk_extension_len;
    size_t padding_len = 0;

    if (last_was_empty && psk_extension_len == 0) {
      padding_len = 1;

      header_len += 4 + padding_len;
    }

    if (header_len > 0xff && header_len < 0x200) {

      if (padding_len != 0) {
        header_len -= 4 + padding_len;
      }
      padding_len = 0x200 - header_len;

      if (padding_len >= 4 + 1) {
        padding_len -= 4;
      } else {
        padding_len = 1;
      }
    }

    if (padding_len != 0 &&
        !add_padding_extension(&extensions, TLSEXT_TYPE_padding, padding_len)) {
      return false;
    }
  }

  const size_t len_before = CBB_len(&extensions);
  if (!ext_pre_shared_key_add_clienthello(hs, &extensions, out_needs_psk_binder,
                                          type)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }
  assert(psk_extension_len == CBB_len(&extensions) - len_before);
  (void)len_before;

  if (CBB_len(&extensions) == 0) {
    CBB_discard_child(out);
  }

  return CBB_flush(out);
}

bool ssl_add_serverhello_tlsext(SSL_HANDSHAKE *hs, CBB *out) {
  SSL *const ssl = hs->ssl;
  CBB extensions;
  if (!CBB_add_u16_length_prefixed(out, &extensions)) {
    goto err;
  }

  for (unsigned i = 0; i < kNumExtensions; i++) {
    if (!(hs->extensions.received & (1u << i))) {

      continue;
    }

    if (!kExtensions[i].add_serverhello(hs, &extensions)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_ERROR_ADDING_EXTENSION);
      ERR_add_error_dataf("extension %u", (unsigned)kExtensions[i].value);
      goto err;
    }
  }

  if (ssl_protocol_version(ssl) < TLS1_3_VERSION &&
      CBB_len(&extensions) == 0) {
    CBB_discard_child(out);
  }

  return CBB_flush(out);

err:
  OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
  return false;
}

static bool ssl_scan_clienthello_tlsext(SSL_HANDSHAKE *hs,
                                        const SSL_CLIENT_HELLO *client_hello,
                                        int *out_alert) {
  hs->extensions.received = 0;
  CBS extensions;
  CBS_init(&extensions, client_hello->extensions, client_hello->extensions_len);
  while (CBS_len(&extensions) != 0) {
    uint16_t type;
    CBS extension;

    if (!CBS_get_u16(&extensions, &type) ||
        !CBS_get_u16_length_prefixed(&extensions, &extension)) {
      *out_alert = SSL_AD_DECODE_ERROR;
      return false;
    }

    unsigned ext_index;
    const struct tls_extension *const ext =
        tls_extension_find(&ext_index, type);
    if (ext == NULL) {
      continue;
    }

    hs->extensions.received |= (1u << ext_index);
    uint8_t alert = SSL_AD_DECODE_ERROR;
    if (!ext->parse_clienthello(hs, &alert, &extension)) {
      *out_alert = alert;
      OPENSSL_PUT_ERROR(SSL, SSL_R_ERROR_PARSING_EXTENSION);
      ERR_add_error_dataf("extension %u", (unsigned)type);
      return false;
    }
  }

  for (size_t i = 0; i < kNumExtensions; i++) {
    if (hs->extensions.received & (1u << i)) {
      continue;
    }

    CBS *contents = NULL, fake_contents;
    static const uint8_t kFakeRenegotiateExtension[] = {0};
    if (kExtensions[i].value == TLSEXT_TYPE_renegotiate &&
        ssl_client_cipher_list_contains_cipher(client_hello,
                                               SSL3_CK_SCSV & 0xffff)) {

      CBS_init(&fake_contents, kFakeRenegotiateExtension,
               sizeof(kFakeRenegotiateExtension));
      contents = &fake_contents;
      hs->extensions.received |= (1u << i);
    }

    uint8_t alert = SSL_AD_DECODE_ERROR;
    if (!kExtensions[i].parse_clienthello(hs, &alert, contents)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_EXTENSION);
      ERR_add_error_dataf("extension %u", (unsigned)kExtensions[i].value);
      *out_alert = alert;
      return false;
    }
  }

  return true;
}

bool ssl_parse_clienthello_tlsext(SSL_HANDSHAKE *hs,
                                  const SSL_CLIENT_HELLO *client_hello) {
  SSL *const ssl = hs->ssl;
  int alert = SSL_AD_DECODE_ERROR;
  if (!ssl_scan_clienthello_tlsext(hs, client_hello, &alert)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
    return false;
  }

  if (!ssl_check_clienthello_tlsext(hs)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_TLSEXT);
    return false;
  }

  return true;
}

static bool ssl_scan_serverhello_tlsext(SSL_HANDSHAKE *hs, const CBS *cbs,
                                        int *out_alert) {
  CBS extensions = *cbs;
  if (!tls1_check_duplicate_extensions(&extensions)) {
    *out_alert = SSL_AD_DECODE_ERROR;
    return false;
  }

  uint32_t received = 0;
  while (CBS_len(&extensions) != 0) {
    uint16_t type;
    CBS extension;

    if (!CBS_get_u16(&extensions, &type) ||
        !CBS_get_u16_length_prefixed(&extensions, &extension)) {
      *out_alert = SSL_AD_DECODE_ERROR;
      return false;
    }

    unsigned ext_index;
    const struct tls_extension *const ext =
        tls_extension_find(&ext_index, type);

    if (ext == NULL) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
      ERR_add_error_dataf("extension %u", (unsigned)type);
      *out_alert = SSL_AD_UNSUPPORTED_EXTENSION;
      return false;
    }

    static_assert(kNumExtensions <= sizeof(hs->extensions.sent) * 8,
                  "too many bits");

    if (!(hs->extensions.sent & (1u << ext_index))) {

      OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_EXTENSION);
      ERR_add_error_dataf("extension %u", (unsigned)type);
      *out_alert = SSL_AD_UNSUPPORTED_EXTENSION;
      return false;
    }

    received |= (1u << ext_index);

    uint8_t alert = SSL_AD_DECODE_ERROR;
    if (!ext->parse_serverhello(hs, &alert, &extension)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_ERROR_PARSING_EXTENSION);
      ERR_add_error_dataf("extension %u", (unsigned)type);
      *out_alert = alert;
      return false;
    }
  }

  for (size_t i = 0; i < kNumExtensions; i++) {
    if (!(received & (1u << i))) {

      uint8_t alert = SSL_AD_DECODE_ERROR;
      if (!kExtensions[i].parse_serverhello(hs, &alert, NULL)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_MISSING_EXTENSION);
        ERR_add_error_dataf("extension %u", (unsigned)kExtensions[i].value);
        *out_alert = alert;
        return false;
      }
    }
  }

  return true;
}

static bool ssl_check_clienthello_tlsext(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  int ret = SSL_TLSEXT_ERR_NOACK;
  int al = SSL_AD_UNRECOGNIZED_NAME;
  if (ssl->ctx->servername_callback != 0) {
    ret = ssl->ctx->servername_callback(ssl, &al, ssl->ctx->servername_arg);
  } else if (ssl->session_ctx->servername_callback != 0) {
    ret = ssl->session_ctx->servername_callback(
        ssl, &al, ssl->session_ctx->servername_arg);
  }

  switch (ret) {
    case SSL_TLSEXT_ERR_ALERT_FATAL:
      ssl_send_alert(ssl, SSL3_AL_FATAL, al);
      return false;

    case SSL_TLSEXT_ERR_NOACK:
      hs->should_ack_sni = false;
      return true;

    default:
      hs->should_ack_sni = ssl->s3->hostname != nullptr;
      return true;
  }
}

static bool ssl_check_serverhello_tlsext(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  if (hs->new_session != nullptr && hs->new_session->has_application_settings) {

    if (ssl->s3->alpn_selected.empty()) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_NEGOTIATED_ALPS_WITHOUT_ALPN);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
      return false;
    }

    Span<const uint8_t> settings;
    if (!ssl_get_local_application_settings(hs, &settings,
                                            ssl->s3->alpn_selected)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_INVALID_ALPN_PROTOCOL);
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_ILLEGAL_PARAMETER);
      return false;
    }

    if (!hs->new_session->local_application_settings.CopyFrom(settings)) {
      ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      return false;
    }
  }

  return true;
}

bool ssl_parse_serverhello_tlsext(SSL_HANDSHAKE *hs, const CBS *cbs) {
  SSL *const ssl = hs->ssl;
  int alert = SSL_AD_DECODE_ERROR;
  if (!ssl_scan_serverhello_tlsext(hs, cbs, &alert)) {
    ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
    return false;
  }

  if (!ssl_check_serverhello_tlsext(hs)) {
    return false;
  }

  return true;
}

static enum ssl_ticket_aead_result_t decrypt_ticket_with_cipher_ctx(
    Array<uint8_t> *out, EVP_CIPHER_CTX *cipher_ctx, HMAC_CTX *hmac_ctx,
    Span<const uint8_t> ticket) {
  size_t iv_len = EVP_CIPHER_CTX_iv_length(cipher_ctx);

  uint8_t mac[EVP_MAX_MD_SIZE];
  size_t mac_len = HMAC_size(hmac_ctx);
  if (ticket.size() < SSL_TICKET_KEY_NAME_LEN + iv_len + 1 + mac_len) {

    return ssl_ticket_aead_ignore_ticket;
  }

  auto ticket_mac = ticket.last(mac_len);
  ticket = ticket.first(ticket.size() - mac_len);
  HMAC_Update(hmac_ctx, ticket.data(), ticket.size());
  HMAC_Final(hmac_ctx, mac, NULL);
  assert(mac_len == ticket_mac.size());
  bool mac_ok = CRYPTO_memcmp(mac, ticket_mac.data(), mac_len) == 0;
  if (CRYPTO_fuzzer_mode_enabled()) {
    mac_ok = true;
  }
  if (!mac_ok) {
    return ssl_ticket_aead_ignore_ticket;
  }

  auto ciphertext = ticket.subspan(SSL_TICKET_KEY_NAME_LEN + iv_len);
  Array<uint8_t> plaintext;
  if (CRYPTO_fuzzer_mode_enabled()) {
    if (!plaintext.CopyFrom(ciphertext)) {
      return ssl_ticket_aead_error;
    }
  } else {
    if (ciphertext.size() >= INT_MAX) {
      return ssl_ticket_aead_ignore_ticket;
    }
    if (!plaintext.InitForOverwrite(ciphertext.size())) {
      return ssl_ticket_aead_error;
    }
    int len1, len2;
    if (!EVP_DecryptUpdate(cipher_ctx, plaintext.data(), &len1,
                           ciphertext.data(), (int)ciphertext.size()) ||
        !EVP_DecryptFinal_ex(cipher_ctx, plaintext.data() + len1, &len2)) {
      ERR_clear_error();
      return ssl_ticket_aead_ignore_ticket;
    }
    plaintext.Shrink(static_cast<size_t>(len1) + len2);
  }

  *out = std::move(plaintext);
  return ssl_ticket_aead_success;
}

static enum ssl_ticket_aead_result_t ssl_decrypt_ticket_with_cb(
    SSL_HANDSHAKE *hs, Array<uint8_t> *out, bool *out_renew_ticket,
    Span<const uint8_t> ticket) {
  assert(ticket.size() >= SSL_TICKET_KEY_NAME_LEN + EVP_MAX_IV_LENGTH);
  ScopedEVP_CIPHER_CTX cipher_ctx;
  ScopedHMAC_CTX hmac_ctx;
  auto name = ticket.subspan(0, SSL_TICKET_KEY_NAME_LEN);

  auto iv = ticket.subspan(SSL_TICKET_KEY_NAME_LEN, EVP_MAX_IV_LENGTH);
  int cb_ret = hs->ssl->session_ctx->ticket_key_cb(
      hs->ssl, const_cast<uint8_t *>(name.data()),
      const_cast<uint8_t *>(iv.data()), cipher_ctx.get(), hmac_ctx.get(),
      0              );
  if (cb_ret < 0) {
    return ssl_ticket_aead_error;
  } else if (cb_ret == 0) {
    return ssl_ticket_aead_ignore_ticket;
  } else if (cb_ret == 2) {
    *out_renew_ticket = true;
  } else {
    assert(cb_ret == 1);
  }
  return decrypt_ticket_with_cipher_ctx(out, cipher_ctx.get(), hmac_ctx.get(),
                                        ticket);
}

static enum ssl_ticket_aead_result_t ssl_decrypt_ticket_with_ticket_keys(
    SSL_HANDSHAKE *hs, Array<uint8_t> *out, Span<const uint8_t> ticket) {
  assert(ticket.size() >= SSL_TICKET_KEY_NAME_LEN + EVP_MAX_IV_LENGTH);
  SSL_CTX *ctx = hs->ssl->session_ctx.get();

  if (!ssl_ctx_rotate_ticket_encryption_key(ctx)) {
    return ssl_ticket_aead_error;
  }

  const EVP_CIPHER *cipher = EVP_aes_128_cbc();
  auto name = ticket.subspan(0, SSL_TICKET_KEY_NAME_LEN);
  auto iv =
      ticket.subspan(SSL_TICKET_KEY_NAME_LEN, EVP_CIPHER_iv_length(cipher));

  ScopedEVP_CIPHER_CTX cipher_ctx;
  ScopedHMAC_CTX hmac_ctx;
  {
    MutexReadLock lock(&ctx->lock);
    const TicketKey *key;
    if (ctx->ticket_key_current && name == ctx->ticket_key_current->name) {
      key = ctx->ticket_key_current.get();
    } else if (ctx->ticket_key_prev && name == ctx->ticket_key_prev->name) {
      key = ctx->ticket_key_prev.get();
    } else {
      return ssl_ticket_aead_ignore_ticket;
    }
    if (!HMAC_Init_ex(hmac_ctx.get(), key->hmac_key, sizeof(key->hmac_key),
                      tlsext_tick_md(), NULL) ||
        !EVP_DecryptInit_ex(cipher_ctx.get(), cipher, NULL, key->aes_key,
                            iv.data())) {
      return ssl_ticket_aead_error;
    }
  }
  return decrypt_ticket_with_cipher_ctx(out, cipher_ctx.get(), hmac_ctx.get(),
                                        ticket);
}

static enum ssl_ticket_aead_result_t ssl_decrypt_ticket_with_method(
    SSL_HANDSHAKE *hs, Array<uint8_t> *out, bool *out_renew_ticket,
    Span<const uint8_t> ticket) {
  Array<uint8_t> plaintext;
  if (!plaintext.InitForOverwrite(ticket.size())) {
    return ssl_ticket_aead_error;
  }

  size_t plaintext_len;
  const enum ssl_ticket_aead_result_t result =
      hs->ssl->session_ctx->ticket_aead_method->open(
          hs->ssl, plaintext.data(), &plaintext_len, ticket.size(),
          ticket.data(), ticket.size());
  if (result != ssl_ticket_aead_success) {
    return result;
  }

  plaintext.Shrink(plaintext_len);
  *out = std::move(plaintext);
  return ssl_ticket_aead_success;
}

enum ssl_ticket_aead_result_t ssl_process_ticket(
    SSL_HANDSHAKE *hs, UniquePtr<SSL_SESSION> *out_session,
    bool *out_renew_ticket, Span<const uint8_t> ticket,
    Span<const uint8_t> session_id) {
  SSL *const ssl = hs->ssl;
  *out_renew_ticket = false;
  out_session->reset();

  if ((SSL_get_options(hs->ssl) & SSL_OP_NO_TICKET) ||
      session_id.size() > SSL_MAX_SSL_SESSION_ID_LENGTH) {
    return ssl_ticket_aead_ignore_ticket;
  }

  const bool is_psk = ssl_protocol_version(ssl) >= TLS1_3_VERSION;

  Array<uint8_t> plaintext;
  enum ssl_ticket_aead_result_t result;
  SSL_HANDSHAKE_HINTS *const hints = hs->hints.get();
  if (is_psk && hints && !hs->hints_requested &&
      !hints->decrypted_psk.empty()) {
    result = plaintext.CopyFrom(hints->decrypted_psk) ? ssl_ticket_aead_success
                                                      : ssl_ticket_aead_error;
  } else if (is_psk && hints && !hs->hints_requested && hints->ignore_psk) {
    result = ssl_ticket_aead_ignore_ticket;
  } else if (!is_psk && hints && !hs->hints_requested &&
             !hints->decrypted_ticket.empty()) {
    if (plaintext.CopyFrom(hints->decrypted_ticket)) {
      result = ssl_ticket_aead_success;
      *out_renew_ticket = hints->renew_ticket;
    } else {
      result = ssl_ticket_aead_error;
    }
  } else if (!is_psk && hints && !hs->hints_requested && hints->ignore_ticket) {
    result = ssl_ticket_aead_ignore_ticket;
  } else if (ssl->session_ctx->ticket_aead_method != NULL) {
    result = ssl_decrypt_ticket_with_method(hs, &plaintext, out_renew_ticket,
                                            ticket);
  } else {

    if (ticket.size() < SSL_TICKET_KEY_NAME_LEN + EVP_MAX_IV_LENGTH) {
      result = ssl_ticket_aead_ignore_ticket;
    } else if (ssl->session_ctx->ticket_key_cb != NULL) {
      result =
          ssl_decrypt_ticket_with_cb(hs, &plaintext, out_renew_ticket, ticket);
    } else {
      result = ssl_decrypt_ticket_with_ticket_keys(hs, &plaintext, ticket);
    }
  }

  if (hints && hs->hints_requested) {
    if (result == ssl_ticket_aead_ignore_ticket) {
      if (is_psk) {
        hints->ignore_psk = true;
      } else {
        hints->ignore_ticket = true;
      }
    } else if (result == ssl_ticket_aead_success) {
      if (is_psk) {
        if (!hints->decrypted_psk.CopyFrom(plaintext)) {
          return ssl_ticket_aead_error;
        }
      } else {
        if (!hints->decrypted_ticket.CopyFrom(plaintext)) {
          return ssl_ticket_aead_error;
        }
        hints->renew_ticket = *out_renew_ticket;
      }
    }
  }

  if (result != ssl_ticket_aead_success) {
    return result;
  }

  UniquePtr<SSL_SESSION> session(SSL_SESSION_from_bytes(
      plaintext.data(), plaintext.size(), ssl->ctx.get()));
  if (!session) {
    ERR_clear_error();
    return ssl_ticket_aead_ignore_ticket;
  }

  session->session_id.ResizeForOverwrite(SHA256_DIGEST_LENGTH);
  SHA256(ticket.data(), ticket.size(), session->session_id.data());

  *out_session = std::move(session);
  return ssl_ticket_aead_success;
}

bool tls1_parse_peer_sigalgs(SSL_HANDSHAKE *hs, const CBS *in_sigalgs) {

  if (ssl_protocol_version(hs->ssl) < TLS1_2_VERSION) {
    return true;
  }

  return CBS_len(in_sigalgs) != 0 &&
         parse_u16_array(in_sigalgs, &hs->peer_sigalgs);
}

bool tls1_get_legacy_signature_algorithm(uint16_t *out, const EVP_PKEY *pkey) {
  switch (EVP_PKEY_id(pkey)) {
    case EVP_PKEY_RSA:
      *out = SSL_SIGN_RSA_PKCS1_MD5_SHA1;
      return true;
    case EVP_PKEY_EC:
      *out = SSL_SIGN_ECDSA_SHA1;
      return true;
    default:
      return false;
  }
}

bool tls1_choose_signature_algorithm(SSL_HANDSHAKE *hs,
                                     const SSL_CREDENTIAL *cred,
                                     uint16_t *out) {
  SSL *const ssl = hs->ssl;
  if (!cred->UsesPrivateKey()) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNKNOWN_CERTIFICATE_TYPE);
    return false;
  }

  uint16_t version = ssl_protocol_version(ssl);
  if (version < TLS1_2_VERSION) {
    if (!tls1_get_legacy_signature_algorithm(out, cred->pubkey.get())) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_NO_COMMON_SIGNATURE_ALGORITHMS);
      return false;
    }
    return true;
  }

  Span<const uint16_t> peer_sigalgs;
  if (cred->type == SSLCredentialType::kDelegated) {
    peer_sigalgs = hs->peer_delegated_credential_sigalgs;
  } else {
    peer_sigalgs = hs->peer_sigalgs;
    if (peer_sigalgs.empty() && version == TLS1_2_VERSION) {

      static const uint16_t kTLS12Default[] = {SSL_SIGN_RSA_PKCS1_SHA1,
                                               SSL_SIGN_ECDSA_SHA1};
      peer_sigalgs = kTLS12Default;
    }
  }

  Span<const uint16_t> sigalgs =
      cred->sigalgs.empty() ? Span(kSignSignatureAlgorithms) : cred->sigalgs;
  for (uint16_t sigalg : sigalgs) {
    if (!ssl_pkey_supports_algorithm(ssl, cred->pubkey.get(), sigalg,
                                                   false)) {
      continue;
    }

    if (std::find(peer_sigalgs.begin(), peer_sigalgs.end(), sigalg) !=
        peer_sigalgs.end()) {
      *out = sigalg;
      return true;
    }
  }

  OPENSSL_PUT_ERROR(SSL, SSL_R_NO_COMMON_SIGNATURE_ALGORITHMS);
  return false;
}

bool tls1_verify_channel_id(SSL_HANDSHAKE *hs, const SSLMessage &msg) {
  SSL *const ssl = hs->ssl;

  uint16_t extension_type;
  CBS channel_id = msg.body, extension;
  if (!CBS_get_u16(&channel_id, &extension_type) ||
      !CBS_get_u16_length_prefixed(&channel_id, &extension) ||
      CBS_len(&channel_id) != 0 ||
      extension_type != TLSEXT_TYPE_channel_id ||
      CBS_len(&extension) != TLSEXT_CHANNEL_ID_SIZE) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return false;
  }

  const EC_GROUP *p256 = EC_group_p256();
  UniquePtr<ECDSA_SIG> sig(ECDSA_SIG_new());
  UniquePtr<BIGNUM> x(BN_new()), y(BN_new());
  if (!sig || !x || !y) {
    return false;
  }

  const uint8_t *p = CBS_data(&extension);
  if (BN_bin2bn(p + 0, 32, x.get()) == NULL ||
      BN_bin2bn(p + 32, 32, y.get()) == NULL ||
      BN_bin2bn(p + 64, 32, sig->r) == NULL ||
      BN_bin2bn(p + 96, 32, sig->s) == NULL) {
    return false;
  }

  UniquePtr<EC_KEY> key(EC_KEY_new());
  UniquePtr<EC_POINT> point(EC_POINT_new(p256));
  if (!key || !point ||
      !EC_POINT_set_affine_coordinates_GFp(p256, point.get(), x.get(), y.get(),
                                           nullptr) ||
      !EC_KEY_set_group(key.get(), p256) ||
      !EC_KEY_set_public_key(key.get(), point.get())) {
    return false;
  }

  uint8_t digest[EVP_MAX_MD_SIZE];
  size_t digest_len;
  if (!tls1_channel_id_hash(hs, digest, &digest_len)) {
    return false;
  }

  bool sig_ok = ECDSA_do_verify(digest, digest_len, sig.get(), key.get());
  if (CRYPTO_fuzzer_mode_enabled()) {
    sig_ok = true;
    ERR_clear_error();
  }
  if (!sig_ok) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CHANNEL_ID_SIGNATURE_INVALID);
    ssl_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECRYPT_ERROR);
    return false;
  }

  OPENSSL_memcpy(ssl->s3->channel_id, p, 64);
  ssl->s3->channel_id_valid = true;
  return true;
}

bool tls1_write_channel_id(SSL_HANDSHAKE *hs, CBB *cbb) {
  uint8_t digest[EVP_MAX_MD_SIZE];
  size_t digest_len;
  if (!tls1_channel_id_hash(hs, digest, &digest_len)) {
    return false;
  }

  EC_KEY *ec_key = EVP_PKEY_get0_EC_KEY(hs->config->channel_id_private.get());
  if (ec_key == nullptr) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  UniquePtr<BIGNUM> x(BN_new()), y(BN_new());
  if (!x || !y ||
      !EC_POINT_get_affine_coordinates_GFp(EC_KEY_get0_group(ec_key),
                                           EC_KEY_get0_public_key(ec_key),
                                           x.get(), y.get(), nullptr)) {
    return false;
  }

  UniquePtr<ECDSA_SIG> sig(ECDSA_do_sign(digest, digest_len, ec_key));
  if (!sig) {
    return false;
  }

  CBB child;
  if (!CBB_add_u16(cbb, TLSEXT_TYPE_channel_id) ||
      !CBB_add_u16_length_prefixed(cbb, &child) ||
      !BN_bn2cbb_padded(&child, 32, x.get()) ||
      !BN_bn2cbb_padded(&child, 32, y.get()) ||
      !BN_bn2cbb_padded(&child, 32, sig->r) ||
      !BN_bn2cbb_padded(&child, 32, sig->s) ||
      !CBB_flush(cbb)) {
    return false;
  }

  return true;
}

bool tls1_channel_id_hash(SSL_HANDSHAKE *hs, uint8_t *out, size_t *out_len) {
  SSL *const ssl = hs->ssl;
  if (ssl_protocol_version(ssl) >= TLS1_3_VERSION) {
    Array<uint8_t> msg;
    if (!tls13_get_cert_verify_signature_input(hs, &msg,
                                               ssl_cert_verify_channel_id)) {
      return false;
    }
    SHA256(msg.data(), msg.size(), out);
    *out_len = SHA256_DIGEST_LENGTH;
    return true;
  }

  SHA256_CTX ctx;

  SHA256_Init(&ctx);
  static const char kClientIDMagic[] = "TLS Channel ID signature";
  SHA256_Update(&ctx, kClientIDMagic, sizeof(kClientIDMagic));

  if (ssl->session != NULL) {
    static const char kResumptionMagic[] = "Resumption";
    SHA256_Update(&ctx, kResumptionMagic, sizeof(kResumptionMagic));
    if (ssl->session->original_handshake_hash.empty()) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return false;
    }
    SHA256_Update(&ctx, ssl->session->original_handshake_hash.data(),
                  ssl->session->original_handshake_hash.size());
  }

  uint8_t hs_hash[EVP_MAX_MD_SIZE];
  size_t hs_hash_len;
  if (!hs->transcript.GetHash(hs_hash, &hs_hash_len)) {
    return false;
  }
  SHA256_Update(&ctx, hs_hash, (size_t)hs_hash_len);
  SHA256_Final(out, &ctx);
  *out_len = SHA256_DIGEST_LENGTH;
  return true;
}

bool tls1_record_handshake_hashes_for_channel_id(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;

  if (ssl->session != NULL) {
    return false;
  }

  size_t digest_len;
  hs->new_session->original_handshake_hash.ResizeForOverwrite(
      hs->transcript.DigestLen());
  if (!hs->transcript.GetHash(hs->new_session->original_handshake_hash.data(),
                              &digest_len)) {
    return false;
  }
  assert(digest_len == hs->new_session->original_handshake_hash.size());
  return true;
}

bool ssl_is_sct_list_valid(const CBS *contents) {

  CBS copy = *contents;
  CBS sct_list;
  if (!CBS_get_u16_length_prefixed(&copy, &sct_list) || CBS_len(&copy) != 0 ||
      CBS_len(&sct_list) == 0) {
    return false;
  }

  while (CBS_len(&sct_list) > 0) {
    CBS sct;
    if (!CBS_get_u16_length_prefixed(&sct_list, &sct) || CBS_len(&sct) == 0) {
      return false;
    }
  }

  return true;
}

BSSL_NAMESPACE_END

using namespace bssl;

int SSL_parse_client_hello(const SSL *ssl, SSL_CLIENT_HELLO *out,
                           const uint8_t *in, size_t len) {
  CBS cbs = Span(in, len);
  if (!ssl_parse_client_hello_with_trailing_data(ssl, &cbs, out)) {
    return 0;
  }
  if (CBS_len(&cbs) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CLIENTHELLO_PARSE_FAILED);
    return 0;
  }
  return 1;
}

int SSL_early_callback_ctx_extension_get(const SSL_CLIENT_HELLO *client_hello,
                                         uint16_t extension_type,
                                         const uint8_t **out_data,
                                         size_t *out_len) {
  CBS cbs;
  if (!ssl_client_hello_get_extension(client_hello, &cbs, extension_type)) {
    return 0;
  }

  *out_data = CBS_data(&cbs);
  *out_len = CBS_len(&cbs);
  return 1;
}
