// Copyright 2017 The BoringSSL Authors
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

#include <algorithm>

#include <openssl/bytestring.h>
#include <openssl/err.h>
#include <openssl/span.h>

#include "../crypto/internal.h"
#include "internal.h"

BSSL_NAMESPACE_BEGIN

bool ssl_protocol_version_from_wire(uint16_t *out, uint16_t version) {
  switch (version) {
    case TLS1_VERSION:
    case TLS1_1_VERSION:
    case TLS1_2_VERSION:
    case TLS1_3_VERSION:
      *out = version;
      return true;

    case DTLS1_VERSION:

      *out = TLS1_1_VERSION;
      return true;

    case DTLS1_2_VERSION:
      *out = TLS1_2_VERSION;
      return true;

    case DTLS1_3_VERSION:
      *out = TLS1_3_VERSION;
      return true;

    default:
      return false;
  }
}

static const uint16_t kTLSVersions[] = {
    TLS1_3_VERSION,
    TLS1_2_VERSION,
    TLS1_1_VERSION,
    TLS1_VERSION,
};

static const uint16_t kDTLSVersions[] = {
    DTLS1_3_VERSION,
    DTLS1_2_VERSION,
    DTLS1_VERSION,
};

static Span<const uint16_t> get_method_versions(
    const SSL_PROTOCOL_METHOD *method) {
  return method->is_dtls ? Span<const uint16_t>(kDTLSVersions)
                         : Span<const uint16_t>(kTLSVersions);
}

bool ssl_method_supports_version(const SSL_PROTOCOL_METHOD *method,
                                 uint16_t version) {
  for (uint16_t supported : get_method_versions(method)) {
    if (supported == version) {
      return true;
    }
  }
  return false;
}

static const char *kUnknownVersion = "unknown";

struct VersionInfo {
  uint16_t version;
  const char *name;
};

static const VersionInfo kVersionNames[] = {
    {TLS1_3_VERSION, "TLSv1.3"},
    {TLS1_2_VERSION, "TLSv1.2"},
    {TLS1_1_VERSION, "TLSv1.1"},
    {TLS1_VERSION, "TLSv1"},
    {DTLS1_VERSION, "DTLSv1"},
    {DTLS1_2_VERSION, "DTLSv1.2"},
    {DTLS1_3_VERSION, "DTLSv1.3"},
};

static const char *ssl_version_to_string(uint16_t version) {
  for (const auto &v : kVersionNames) {
    if (v.version == version) {
      return v.name;
    }
  }
  return kUnknownVersion;
}

static uint16_t wire_version_to_api(uint16_t version) { return version; }

static bool api_version_to_wire(uint16_t *out, uint16_t version) {

  uint16_t unused;
  if (!ssl_protocol_version_from_wire(&unused, version)) {
    return false;
  }

  *out = version;
  return true;
}

static bool set_version_bound(const SSL_PROTOCOL_METHOD *method, uint16_t *out,
                              uint16_t version) {
  if (!api_version_to_wire(&version, version) ||
      !ssl_method_supports_version(method, version)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNKNOWN_SSL_VERSION);
    return false;
  }

  *out = version;
  return true;
}

static bool set_min_version(const SSL_PROTOCOL_METHOD *method, uint16_t *out,
                            uint16_t version) {

  if (version == 0) {
    *out = method->is_dtls ? DTLS1_2_VERSION : TLS1_2_VERSION;
    return true;
  }

  return set_version_bound(method, out, version);
}

static bool set_max_version(const SSL_PROTOCOL_METHOD *method, uint16_t *out,
                            uint16_t version) {

  if (version == 0) {
    *out = method->is_dtls ? DTLS1_2_VERSION : TLS1_3_VERSION;
    return true;
  }

  return set_version_bound(method, out, version);
}

const struct {
  uint16_t version;
  uint32_t flag;
} kProtocolVersions[] = {
    {TLS1_VERSION, SSL_OP_NO_TLSv1},
    {TLS1_1_VERSION, SSL_OP_NO_TLSv1_1},
    {TLS1_2_VERSION, SSL_OP_NO_TLSv1_2},
    {TLS1_3_VERSION, SSL_OP_NO_TLSv1_3},
};

bool ssl_get_version_range(const SSL_HANDSHAKE *hs, uint16_t *out_min_version,
                           uint16_t *out_max_version) {

  uint32_t options = hs->ssl->options;
  if (SSL_is_dtls(hs->ssl)) {
    options &= ~SSL_OP_NO_TLSv1_1;
    if (options & SSL_OP_NO_DTLSv1) {
      options |= SSL_OP_NO_TLSv1_1;
    }
  }

  uint16_t min_version, max_version;
  if (!ssl_protocol_version_from_wire(&min_version,
                                      hs->config->conf_min_version) ||
      !ssl_protocol_version_from_wire(&max_version,
                                      hs->config->conf_max_version)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  if (SSL_is_quic(hs->ssl) && min_version < TLS1_3_VERSION) {
    min_version = TLS1_3_VERSION;
  }

  bool any_enabled = false;
  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kProtocolVersions); i++) {

    if (min_version > kProtocolVersions[i].version) {
      continue;
    }
    if (max_version < kProtocolVersions[i].version) {
      break;
    }

    if (!(options & kProtocolVersions[i].flag)) {

      if (!any_enabled) {
        any_enabled = true;
        min_version = kProtocolVersions[i].version;
      }
      continue;
    }

    if (any_enabled) {
      max_version = kProtocolVersions[i - 1].version;
      break;
    }
  }

  if (!any_enabled) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_NO_SUPPORTED_VERSIONS_ENABLED);
    return false;
  }

  *out_min_version = min_version;
  *out_max_version = max_version;
  return true;
}

static uint16_t ssl_version(const SSL *ssl) {

  if (SSL_in_early_data(ssl) && !ssl->server) {
    return ssl->s3->hs->early_session->ssl_version;
  }
  if (ssl->s3->version != 0) {
    return ssl->s3->version;
  }

  return SSL_is_dtls(ssl) ? DTLS1_2_VERSION : TLS1_2_VERSION;
}

bool ssl_has_final_version(const SSL *ssl) {
  return ssl->s3->version != 0 &&
         (ssl->s3->hs == nullptr || !ssl->s3->hs->is_early_version);
}

uint16_t ssl_protocol_version(const SSL *ssl) {
  assert(ssl->s3->version != 0);
  uint16_t version;
  if (!ssl_protocol_version_from_wire(&version, ssl->s3->version)) {

    assert(0);
    return 0;
  }

  return version;
}

bool ssl_supports_version(const SSL_HANDSHAKE *hs, uint16_t version) {
  const SSL *const ssl = hs->ssl;
  uint16_t protocol_version;
  if (!ssl_method_supports_version(ssl->method, version) ||
      !ssl_protocol_version_from_wire(&protocol_version, version) ||
      hs->min_version > protocol_version ||
      protocol_version > hs->max_version) {
    return false;
  }

  return true;
}

bool ssl_add_supported_versions(const SSL_HANDSHAKE *hs, CBB *cbb,
                                uint16_t extra_min_version) {
  for (uint16_t version : get_method_versions(hs->ssl->method)) {
    uint16_t protocol_version;
    if (ssl_supports_version(hs, version) &&
        ssl_protocol_version_from_wire(&protocol_version, version) &&
        protocol_version >= extra_min_version &&
        !CBB_add_u16(cbb, version)) {
      return false;
    }
  }
  return true;
}

bool ssl_negotiate_version(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                           uint16_t *out_version, const CBS *peer_versions) {
  for (uint16_t version : get_method_versions(hs->ssl->method)) {
    if (!ssl_supports_version(hs, version)) {
      continue;
    }

    if (version == TLS1_3_VERSION && hs->apply_jdk11_workaround) {
      continue;
    }

    CBS copy = *peer_versions;
    while (CBS_len(&copy) != 0) {
      uint16_t peer_version;
      if (!CBS_get_u16(&copy, &peer_version)) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
        *out_alert = SSL_AD_DECODE_ERROR;
        return false;
      }

      if (peer_version == version) {
        *out_version = version;
        return true;
      }
    }
  }

  OPENSSL_PUT_ERROR(SSL, SSL_R_UNSUPPORTED_PROTOCOL);
  *out_alert = SSL_AD_PROTOCOL_VERSION;
  return false;
}

BSSL_NAMESPACE_END

using namespace bssl;

int SSL_CTX_set_min_proto_version(SSL_CTX *ctx, uint16_t version) {
  return set_min_version(ctx->method, &ctx->conf_min_version, version);
}

int SSL_CTX_set_max_proto_version(SSL_CTX *ctx, uint16_t version) {
  return set_max_version(ctx->method, &ctx->conf_max_version, version);
}

uint16_t SSL_CTX_get_min_proto_version(const SSL_CTX *ctx) {
  return ctx->conf_min_version;
}

uint16_t SSL_CTX_get_max_proto_version(const SSL_CTX *ctx) {
  return ctx->conf_max_version;
}

int SSL_set_min_proto_version(SSL *ssl, uint16_t version) {
  if (!ssl->config) {
    return 0;
  }
  return set_min_version(ssl->method, &ssl->config->conf_min_version, version);
}

int SSL_set_max_proto_version(SSL *ssl, uint16_t version) {
  if (!ssl->config) {
    return 0;
  }
  return set_max_version(ssl->method, &ssl->config->conf_max_version, version);
}

uint16_t SSL_get_min_proto_version(const SSL *ssl) {
  if (!ssl->config) {
    return 0;
  }
  return ssl->config->conf_min_version;
}

uint16_t SSL_get_max_proto_version(const SSL *ssl) {
  if (!ssl->config) {
    return 0;
  }
  return ssl->config->conf_max_version;
}

int SSL_version(const SSL *ssl) {
  return wire_version_to_api(ssl_version(ssl));
}

const char *SSL_get_version(const SSL *ssl) {
  return ssl_version_to_string(ssl_version(ssl));
}

size_t SSL_get_all_version_names(const char **out, size_t max_out) {
  return GetAllNames(out, max_out, Span(&kUnknownVersion, 1),
                     &VersionInfo::name, Span(kVersionNames));
}

const char *SSL_SESSION_get_version(const SSL_SESSION *session) {
  return ssl_version_to_string(session->ssl_version);
}

uint16_t SSL_SESSION_get_protocol_version(const SSL_SESSION *session) {
  return wire_version_to_api(session->ssl_version);
}

int SSL_SESSION_set_protocol_version(SSL_SESSION *session, uint16_t version) {

  return api_version_to_wire(&session->ssl_version, version);
}

int SSL_CTX_set_record_protocol_version(SSL_CTX *ctx, int version) {
  return version == 0;
}
