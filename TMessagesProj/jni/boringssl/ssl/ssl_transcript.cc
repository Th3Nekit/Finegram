// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
// Copyright 2005 Nokia. All rights reserved.
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

#include <string_view>

#include <openssl/buf.h>
#include <openssl/digest.h>
#include <openssl/err.h>

#include "internal.h"

BSSL_NAMESPACE_BEGIN

SSLTranscript::SSLTranscript(bool is_dtls) : is_dtls_(is_dtls) {}

SSLTranscript::~SSLTranscript() {}

bool SSLTranscript::Init() {
  buffer_.reset(BUF_MEM_new());
  if (!buffer_) {
    return false;
  }

  hash_.Reset();
  return true;
}

bool SSLTranscript::InitHash(uint16_t version, const SSL_CIPHER *cipher) {
  version_ = version;
  const EVP_MD *md = ssl_get_handshake_digest(version, cipher);
  if (Digest() == md) {

    return true;
  }
  if (!HashBuffer(hash_.get(), md)) {
    return false;
  }
  if (is_dtls_ && version_ >= TLS1_3_VERSION) {

    FreeBuffer();
  }
  return true;
}

bool SSLTranscript::HashBuffer(EVP_MD_CTX *ctx, const EVP_MD *digest) const {
  if (!EVP_DigestInit_ex(ctx, digest, nullptr)) {
    return false;
  }
  if (!is_dtls_ || version_ < TLS1_3_VERSION) {
    return EVP_DigestUpdate(ctx, buffer_->data, buffer_->length);
  }

  if (buffer_->length == 0) {
    return true;
  }

  CBS buf, header;
  CBS_init(&buf, reinterpret_cast<uint8_t *>(buffer_->data), buffer_->length);
  if (!CBS_get_bytes(&buf, &header, 4) ||
      !CBS_skip(&buf, 8) ||
      !EVP_DigestUpdate(ctx, CBS_data(&header), CBS_len(&header)) ||
      !EVP_DigestUpdate(ctx, CBS_data(&buf), CBS_len(&buf))) {
    return false;
  }
  return true;
}

void SSLTranscript::FreeBuffer() { buffer_.reset(); }

size_t SSLTranscript::DigestLen() const { return EVP_MD_size(Digest()); }

const EVP_MD *SSLTranscript::Digest() const {
  return EVP_MD_CTX_get0_md(hash_.get());
}

bool SSLTranscript::UpdateForHelloRetryRequest() {
  if (buffer_) {
    buffer_->length = 0;
  }

  uint8_t old_hash[EVP_MAX_MD_SIZE];
  size_t hash_len;
  if (!GetHash(old_hash, &hash_len)) {
    return false;
  }
  const uint8_t header[4] = {SSL3_MT_MESSAGE_HASH, 0, 0,
                             static_cast<uint8_t>(hash_len)};
  if (!EVP_DigestInit_ex(hash_.get(), Digest(), nullptr) ||
      !AddToBufferOrHash(header) ||
      !AddToBufferOrHash(Span(old_hash, hash_len))) {
    return false;
  }
  return true;
}

bool SSLTranscript::CopyToHashContext(EVP_MD_CTX *ctx,
                                      const EVP_MD *digest) const {
  const EVP_MD *transcript_digest = Digest();
  if (transcript_digest != nullptr &&
      EVP_MD_type(transcript_digest) == EVP_MD_type(digest)) {
    return EVP_MD_CTX_copy_ex(ctx, hash_.get());
  }

  if (buffer_) {
    return HashBuffer(ctx, digest);
  }

  OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
  return false;
}

bool SSLTranscript::Update(Span<const uint8_t> in) {
  if (!is_dtls_ || version_ < TLS1_3_VERSION) {
    return AddToBufferOrHash(in);
  }
  if (in.size() < DTLS1_HM_HEADER_LENGTH) {
    return false;
  }

  if (!AddToBufferOrHash(in.subspan(0, 4)) ||
      !AddToBufferOrHash(in.subspan(12))) {
    return false;
  }
  return true;
}

bool SSLTranscript::AddToBufferOrHash(Span<const uint8_t> in) {

  if (buffer_ &&
      !BUF_MEM_append(buffer_.get(), in.data(), in.size())) {
    return false;
  }

  if (EVP_MD_CTX_md(hash_.get()) != NULL) {
    EVP_DigestUpdate(hash_.get(), in.data(), in.size());
  }

  return true;
}

bool SSLTranscript::GetHash(uint8_t *out, size_t *out_len) const {
  ScopedEVP_MD_CTX ctx;
  unsigned len;
  if (!EVP_MD_CTX_copy_ex(ctx.get(), hash_.get()) ||
      !EVP_DigestFinal_ex(ctx.get(), out, &len)) {
    return false;
  }
  *out_len = len;
  return true;
}

bool SSLTranscript::GetFinishedMAC(uint8_t *out, size_t *out_len,
                                   const SSL_SESSION *session,
                                   bool from_server) const {
  uint8_t digest[EVP_MAX_MD_SIZE];
  size_t digest_len;
  if (!GetHash(digest, &digest_len)) {
    return false;
  }

  std::string_view label = from_server ? "server finished" : "client finished";
  static const size_t kFinishedLen = 12;
  if (!tls1_prf(Digest(), Span(out, kFinishedLen), session->secret, label,
                Span(digest, digest_len), {})) {
    return false;
  }

  *out_len = kFinishedLen;
  return true;
}

BSSL_NAMESPACE_END
