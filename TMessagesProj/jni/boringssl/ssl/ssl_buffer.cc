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

#include <openssl/ssl.h>

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/mem.h>

#include "../crypto/internal.h"
#include "internal.h"

BSSL_NAMESPACE_BEGIN

static_assert(0xffff <= INT_MAX, "uint16_t does not fit in int");

static_assert((SSL3_ALIGN_PAYLOAD & (SSL3_ALIGN_PAYLOAD - 1)) == 0,
              "SSL3_ALIGN_PAYLOAD must be a power of 2");

void SSLBuffer::Clear() {
  if (buf_ != inline_buf_) {
    free(buf_);
  }
  buf_ = nullptr;
  offset_ = 0;
  size_ = 0;
  cap_ = 0;
}

bool SSLBuffer::EnsureCap(size_t header_len, size_t new_cap) {
  if (new_cap > 0xffff) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  if (cap_ >= new_cap) {
    return true;
  }

  uint8_t *new_buf;
  size_t new_offset;
  if (new_cap <= sizeof(inline_buf_)) {

    new_buf = inline_buf_;
    new_offset = 0;
  } else {

    new_buf = (uint8_t *)malloc(new_cap + SSL3_ALIGN_PAYLOAD - 1);
    if (new_buf == NULL) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
      return false;
    }

    new_offset =
        (0 - header_len - (uintptr_t)new_buf) & (SSL3_ALIGN_PAYLOAD - 1);
  }

  OPENSSL_memmove(new_buf + new_offset, buf_ + offset_, size_);

  if (buf_ != inline_buf_) {
    free(buf_);
  }

  buf_ = new_buf;
  offset_ = new_offset;
  cap_ = new_cap;
  return true;
}

void SSLBuffer::DidWrite(size_t new_size) {
  if (new_size > cap() - size()) {
    abort();
  }
  size_ += new_size;
}

void SSLBuffer::Consume(size_t len) {
  if (len > size_) {
    abort();
  }
  offset_ += (uint16_t)len;
  size_ -= (uint16_t)len;
  cap_ -= (uint16_t)len;
}

void SSLBuffer::DiscardConsumed() {
  if (size_ == 0) {
    Clear();
  }
}

static int dtls_read_buffer_next_packet(SSL *ssl) {
  SSLBuffer *buf = &ssl->s3->read_buffer;

  if (!buf->empty()) {

    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return -1;
  }

  int ret =
      BIO_read(ssl->rbio.get(), buf->data(), static_cast<int>(buf->cap()));
  if (ret <= 0) {
    ssl->s3->rwstate = SSL_ERROR_WANT_READ;
    return ret;
  }
  buf->DidWrite(static_cast<size_t>(ret));
  return 1;
}

static int tls_read_buffer_extend_to(SSL *ssl, size_t len) {
  SSLBuffer *buf = &ssl->s3->read_buffer;

  if (len > buf->cap()) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BUFFER_TOO_SMALL);
    return -1;
  }

  while (buf->size() < len) {

    int ret = BIO_read(ssl->rbio.get(), buf->data() + buf->size(),
                       static_cast<int>(len - buf->size()));
    if (ret <= 0) {
      ssl->s3->rwstate = SSL_ERROR_WANT_READ;
      return ret;
    }
    buf->DidWrite(static_cast<size_t>(ret));
  }

  return 1;
}

int ssl_read_buffer_extend_to(SSL *ssl, size_t len) {

  ssl->s3->read_buffer.DiscardConsumed();

  if (SSL_is_dtls(ssl)) {
    static_assert(
        DTLS1_RT_MAX_HEADER_LENGTH + SSL3_RT_MAX_ENCRYPTED_LENGTH <= 0xffff,
        "DTLS read buffer is too large");

    len = DTLS1_RT_MAX_HEADER_LENGTH + SSL3_RT_MAX_ENCRYPTED_LENGTH;
  }

  if (!ssl->s3->read_buffer.EnsureCap(DTLS1_RT_MAX_HEADER_LENGTH, len)) {
    return -1;
  }

  if (ssl->rbio == nullptr) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BIO_NOT_SET);
    return -1;
  }

  int ret;
  if (SSL_is_dtls(ssl)) {

    ret = dtls_read_buffer_next_packet(ssl);
  } else {
    ret = tls_read_buffer_extend_to(ssl, len);
  }

  if (ret <= 0) {

    ssl->s3->read_buffer.DiscardConsumed();
  }
  return ret;
}

int ssl_handle_open_record(SSL *ssl, bool *out_retry, ssl_open_record_t ret,
                           size_t consumed, uint8_t alert) {
  *out_retry = false;
  if (ret != ssl_open_record_partial) {
    ssl->s3->read_buffer.Consume(consumed);
  }
  if (ret != ssl_open_record_success) {

    ssl->s3->read_buffer.DiscardConsumed();
  }
  switch (ret) {
    case ssl_open_record_success:
      return 1;

    case ssl_open_record_partial: {
      int read_ret = ssl_read_buffer_extend_to(ssl, consumed);
      if (read_ret <= 0) {
        return read_ret;
      }
      *out_retry = true;
      return 1;
    }

    case ssl_open_record_discard:
      *out_retry = true;
      return 1;

    case ssl_open_record_close_notify:
      ssl->s3->rwstate = SSL_ERROR_ZERO_RETURN;
      return 0;

    case ssl_open_record_error:
      if (alert != 0) {
        ssl_send_alert(ssl, SSL3_AL_FATAL, alert);
      }
      return -1;
  }
  assert(0);
  return -1;
}

static_assert(SSL3_RT_HEADER_LENGTH * 2 +
                      SSL3_RT_SEND_MAX_ENCRYPTED_OVERHEAD * 2 +
                      SSL3_RT_MAX_PLAIN_LENGTH <=
                  0xffff,
              "maximum TLS write buffer is too large");

static_assert(DTLS1_RT_MAX_HEADER_LENGTH + SSL3_RT_SEND_MAX_ENCRYPTED_OVERHEAD +
                      SSL3_RT_MAX_PLAIN_LENGTH <=
                  0xffff,
              "maximum DTLS write buffer is too large");

static int tls_write_buffer_flush(SSL *ssl) {
  SSLBuffer *buf = &ssl->s3->write_buffer;

  while (!buf->empty()) {
    int ret = BIO_write(ssl->wbio.get(), buf->data(), buf->size());
    if (ret <= 0) {
      ssl->s3->rwstate = SSL_ERROR_WANT_WRITE;
      return ret;
    }
    buf->Consume(static_cast<size_t>(ret));
  }
  buf->Clear();
  return 1;
}

static int dtls_write_buffer_flush(SSL *ssl) {
  SSLBuffer *buf = &ssl->s3->write_buffer;
  if (buf->empty()) {
    return 1;
  }

  int ret = BIO_write(ssl->wbio.get(), buf->data(), buf->size());
  if (ret <= 0) {
    ssl->s3->rwstate = SSL_ERROR_WANT_WRITE;

    buf->Clear();
    return ret;
  }
  buf->Clear();
  return 1;
}

int ssl_write_buffer_flush(SSL *ssl) {
  if (ssl->wbio == nullptr) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BIO_NOT_SET);
    return -1;
  }

  if (SSL_is_dtls(ssl)) {
    return dtls_write_buffer_flush(ssl);
  } else {
    return tls_write_buffer_flush(ssl);
  }
}

BSSL_NAMESPACE_END
