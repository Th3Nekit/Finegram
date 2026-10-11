// Copyright 2005-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <openssl/bio.h>
#include <openssl/bytestring.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/rand.h>

#include "../crypto/internal.h"
#include "internal.h"

BSSL_NAMESPACE_BEGIN

ssl_open_record_t dtls1_process_ack(SSL *ssl, uint8_t *out_alert,
                                    DTLSRecordNumber ack_record_number,
                                    Span<const uint8_t> data) {

  if (!ssl_has_final_version(ssl)) {
    return ssl_open_record_discard;
  }

  if (ssl_protocol_version(ssl) < TLS1_3_VERSION) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_RECORD);
    *out_alert = SSL_AD_UNEXPECTED_MESSAGE;
    return ssl_open_record_error;
  }

  CBS cbs = data, record_numbers;
  if (!CBS_get_u16_length_prefixed(&cbs, &record_numbers) ||
      CBS_len(&cbs) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    *out_alert = SSL_AD_DECODE_ERROR;
    return ssl_open_record_error;
  }

  while (CBS_len(&record_numbers) != 0) {
    uint64_t epoch, seq;
    if (!CBS_get_u64(&record_numbers, &epoch) ||
        !CBS_get_u64(&record_numbers, &seq)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      *out_alert = SSL_AD_DECODE_ERROR;
      return ssl_open_record_error;
    }

    if ((ack_record_number.epoch() < ssl_encryption_application &&
         epoch > ack_record_number.epoch()) ||
        epoch > UINT16_MAX || seq > DTLSRecordNumber::kMaxSequence) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
      *out_alert = SSL_AD_ILLEGAL_PARAMETER;
      return ssl_open_record_error;
    }

    DTLSRecordNumber number(static_cast<uint16_t>(epoch), seq);
    DTLSSentRecord *sent_record = nullptr;
    if (ssl->d1->sent_records != nullptr) {
      for (size_t i = 0; i < ssl->d1->sent_records->size(); i++) {
        if ((*ssl->d1->sent_records)[i].number == number) {
          sent_record = &(*ssl->d1->sent_records)[i];
          break;
        }
      }
    }
    if (sent_record == nullptr) {

      continue;
    }

    if (sent_record->first_msg == sent_record->last_msg) {
      ssl->d1->outgoing_messages[sent_record->first_msg].acked.MarkRange(
          sent_record->first_msg_start, sent_record->last_msg_end);
    } else {
      ssl->d1->outgoing_messages[sent_record->first_msg].acked.MarkRange(
          sent_record->first_msg_start, SIZE_MAX);
      for (size_t i = size_t{sent_record->first_msg} + 1;
           i < sent_record->last_msg; i++) {
        ssl->d1->outgoing_messages[i].acked.MarkRange(0, SIZE_MAX);
      }
      if (sent_record->last_msg_end != 0) {
        ssl->d1->outgoing_messages[sent_record->last_msg].acked.MarkRange(
            0, sent_record->last_msg_end);
      }
    }

    sent_record->first_msg = 0;
    sent_record->first_msg_start = 0;
    sent_record->last_msg = 0;
    sent_record->last_msg_end = 0;
  }

  if (std::all_of(ssl->d1->outgoing_messages.begin(),
                  ssl->d1->outgoing_messages.end(),
                  [](const auto &msg) { return msg.IsFullyAcked(); })) {
    dtls1_stop_timer(ssl);
    dtls_clear_outgoing_messages(ssl);

    if (ssl->s3->key_update_pending) {
      if (!tls13_rotate_traffic_key(ssl, evp_aead_seal)) {
        return ssl_open_record_error;
      }
      ssl->s3->key_update_pending = false;
    }

    if (ssl->d1->queued_key_update != QueuedKeyUpdate::kNone) {
      int request_type =
          ssl->d1->queued_key_update == QueuedKeyUpdate::kUpdateRequested
              ? SSL_KEY_UPDATE_REQUESTED
              : SSL_KEY_UPDATE_NOT_REQUESTED;
      ssl->d1->queued_key_update = QueuedKeyUpdate::kNone;
      if (!tls13_add_key_update(ssl, request_type)) {
        return ssl_open_record_error;
      }
    }
  } else {

    dtls_clear_unused_write_epochs(ssl);

  }

  ssl_do_msg_callback(ssl,              0, SSL3_RT_ACK, data);
  return ssl_open_record_discard;
}

ssl_open_record_t dtls1_open_app_data(SSL *ssl, Span<uint8_t> *out,
                                      size_t *out_consumed, uint8_t *out_alert,
                                      Span<uint8_t> in) {
  assert(!SSL_in_init(ssl));

  uint8_t type;
  DTLSRecordNumber record_number;
  Span<uint8_t> record;
  auto ret = dtls_open_record(ssl, &type, &record_number, &record, out_consumed,
                              out_alert, in);
  if (ret != ssl_open_record_success) {
    return ret;
  }

  if (type == SSL3_RT_HANDSHAKE) {

    if (ssl_protocol_version(ssl) >= TLS1_3_VERSION) {
      if (!dtls1_process_handshake_fragments(ssl, out_alert, record_number,
                                             record)) {
        return ssl_open_record_error;
      }
      return ssl_open_record_discard;
    }

    CBS cbs, body;
    struct hm_header_st msg_hdr;
    CBS_init(&cbs, record.data(), record.size());
    if (!dtls1_parse_fragment(&cbs, &msg_hdr, &body)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_HANDSHAKE_RECORD);
      *out_alert = SSL_AD_DECODE_ERROR;
      return ssl_open_record_error;
    }

    if (msg_hdr.type == SSL3_MT_FINISHED &&
        msg_hdr.seq == ssl->d1->handshake_read_seq - 1) {
      if (!ssl->d1->sending_flight && msg_hdr.frag_off == 0) {

        ssl->d1->num_timeouts++;
        ssl->d1->sending_flight = true;
      }
      return ssl_open_record_discard;
    }

  }

  if (type == SSL3_RT_ACK) {
    return dtls1_process_ack(ssl, out_alert, record_number, record);
  }

  if (type != SSL3_RT_APPLICATION_DATA) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_RECORD);
    *out_alert = SSL_AD_UNEXPECTED_MESSAGE;
    return ssl_open_record_error;
  }

  if (record.empty()) {
    return ssl_open_record_discard;
  }

  *out = record;
  return ssl_open_record_success;
}

int dtls1_write_app_data(SSL *ssl, bool *out_needs_handshake,
                         size_t *out_bytes_written, Span<const uint8_t> in) {
  assert(!SSL_in_init(ssl));
  *out_needs_handshake = false;

  if (ssl->s3->write_shutdown != ssl_shutdown_none) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_PROTOCOL_IS_SHUTDOWN);
    return -1;
  }

  if (in.size() > SSL3_RT_MAX_PLAIN_LENGTH) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DTLS_MESSAGE_TOO_BIG);
    return -1;
  }

  if (in.empty()) {
    *out_bytes_written = 0;
    return 1;
  }

  int ret = dtls1_write_record(ssl, SSL3_RT_APPLICATION_DATA, in,
                               ssl->d1->write_epoch.epoch());
  if (ret <= 0) {
    return ret;
  }
  *out_bytes_written = in.size();
  return 1;
}

int dtls1_write_record(SSL *ssl, int type, Span<const uint8_t> in,
                       uint16_t epoch) {
  SSLBuffer *buf = &ssl->s3->write_buffer;
  assert(in.size() <= SSL3_RT_MAX_PLAIN_LENGTH);

  assert(buf->empty());

  if (in.size() > SSL3_RT_MAX_PLAIN_LENGTH) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return -1;
  }

  DTLSRecordNumber record_number;
  size_t ciphertext_len;
  if (!buf->EnsureCap(dtls_seal_prefix_len(ssl, epoch),
                      in.size() + SSL_max_seal_overhead(ssl)) ||
      !dtls_seal_record(ssl, &record_number, buf->remaining().data(),
                        &ciphertext_len, buf->remaining().size(), type,
                        in.data(), in.size(), epoch)) {
    buf->Clear();
    return -1;
  }
  buf->DidWrite(ciphertext_len);

  int ret = ssl_write_buffer_flush(ssl);
  if (ret <= 0) {
    return ret;
  }
  return 1;
}

int dtls1_dispatch_alert(SSL *ssl) {
  int ret = dtls1_write_record(ssl, SSL3_RT_ALERT, ssl->s3->send_alert,
                               ssl->d1->write_epoch.epoch());
  if (ret <= 0) {
    return ret;
  }
  ssl->s3->alert_dispatch = false;

  if (ssl->s3->send_alert[0] == SSL3_AL_FATAL) {
    BIO_flush(ssl->wbio.get());
  }

  ssl_do_msg_callback(ssl, 1            , SSL3_RT_ALERT, ssl->s3->send_alert);

  int alert = (ssl->s3->send_alert[0] << 8) | ssl->s3->send_alert[1];
  ssl_do_info_callback(ssl, SSL_CB_WRITE_ALERT, alert);

  return 1;
}

BSSL_NAMESPACE_END
