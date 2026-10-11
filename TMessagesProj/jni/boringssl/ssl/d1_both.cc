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
#include <limits.h>
#include <string.h>

#include <algorithm>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/rand.h>

#include "../crypto/internal.h"
#include "internal.h"

BSSL_NAMESPACE_BEGIN

static const unsigned int kMinMTU = 256 - 28;

static const unsigned int kDefaultMTU = 1500 - 28;

static uint8_t BitRange(size_t start, size_t end) {
  assert(start <= end && end <= 8);
  return static_cast<uint8_t>(~((1u << start) - 1) & ((1u << end) - 1));
}

static DTLSMessageBitmap::Range FirstUnmarkedRangeInByte(uint8_t b) {
  size_t start, end;
  for (start = 0; start < 8; start++) {
    if ((b & (1u << start)) == 0) {
      break;
    }
  }
  for (end = start; end < 8; end++) {
    if ((b & (1u << end)) != 0) {
      break;
    }
  }
  return DTLSMessageBitmap::Range{start, end};
}

bool DTLSMessageBitmap::Init(size_t num_bits) {
  if (num_bits + 7 < num_bits) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
    return false;
  }
  size_t num_bytes = (num_bits + 7) / 8;
  size_t bits_rounded = num_bytes * 8;
  if (!bytes_.Init(num_bytes)) {
    return false;
  }
  MarkRange(num_bits, bits_rounded);
  first_unmarked_byte_ = 0;
  return true;
}

void DTLSMessageBitmap::MarkRange(size_t start, size_t end) {
  assert(start <= end);

  start = std::max(start, first_unmarked_byte_ << 3);

  start = std::min(start, bytes_.size() << 3);
  end = std::min(end, bytes_.size() << 3);
  if (start >= end) {
    return;
  }

  if ((start >> 3) == (end >> 3)) {
    bytes_[start >> 3] |= BitRange(start & 7, end & 7);
  } else {
    bytes_[start >> 3] |= BitRange(start & 7, 8);
    for (size_t i = (start >> 3) + 1; i < (end >> 3); i++) {
      bytes_[i] = 0xff;
    }
    if ((end & 7) != 0) {
      bytes_[end >> 3] |= BitRange(0, end & 7);
    }
  }

  while (first_unmarked_byte_ < bytes_.size() &&
         bytes_[first_unmarked_byte_] == 0xff) {
    first_unmarked_byte_++;
  }

  if (first_unmarked_byte_ >= bytes_.size()) {
    bytes_.Reset();
    first_unmarked_byte_ = 0;
  }
}

DTLSMessageBitmap::Range DTLSMessageBitmap::NextUnmarkedRange(
    size_t start) const {

  start = std::max(start, first_unmarked_byte_ << 3);

  size_t idx = start >> 3;
  if (idx >= bytes_.size()) {
    return Range{0, 0};
  }

  uint8_t byte = bytes_[idx] | BitRange(0, start & 7);
  if (byte == 0xff) {

    for (idx = idx + 1; idx < bytes_.size(); idx++) {
      if (bytes_[idx] != 0xff) {
        byte = bytes_[idx];
        break;
      }
    }
    if (idx >= bytes_.size()) {
      return Range{0, 0};
    }
  }

  Range range = FirstUnmarkedRangeInByte(byte);
  assert(!range.empty());
  bool should_extend = range.end == 8;
  range.start += idx << 3;
  range.end += idx << 3;
  if (!should_extend) {

    return range;
  }

  for (idx = idx + 1; idx < bytes_.size(); idx++) {
    if (bytes_[idx] != 0) {
      break;
    }
  }
  range.end = idx << 3;

  if (idx < bytes_.size()) {
    Range extra = FirstUnmarkedRangeInByte(bytes_[idx]);
    if (extra.start == 0) {
      range.end += extra.end;
    }
  }

  return range;
}

static UniquePtr<DTLSIncomingMessage> dtls_new_incoming_message(
    const struct hm_header_st *msg_hdr) {
  ScopedCBB cbb;
  UniquePtr<DTLSIncomingMessage> frag = MakeUnique<DTLSIncomingMessage>();
  if (!frag) {
    return nullptr;
  }
  frag->type = msg_hdr->type;
  frag->seq = msg_hdr->seq;

  if (!frag->data.InitForOverwrite(DTLS1_HM_HEADER_LENGTH + msg_hdr->msg_len)) {
    return nullptr;
  }

  if (!CBB_init_fixed(cbb.get(), frag->data.data(), DTLS1_HM_HEADER_LENGTH) ||
      !CBB_add_u8(cbb.get(), msg_hdr->type) ||
      !CBB_add_u24(cbb.get(), msg_hdr->msg_len) ||
      !CBB_add_u16(cbb.get(), msg_hdr->seq) ||
      !CBB_add_u24(cbb.get(), 0               ) ||
      !CBB_add_u24(cbb.get(), msg_hdr->msg_len) ||
      !CBB_finish(cbb.get(), NULL, NULL)) {
    return nullptr;
  }

  if (!frag->reassembly.Init(msg_hdr->msg_len)) {
    return nullptr;
  }

  return frag;
}

static bool dtls1_is_current_message_complete(const SSL *ssl) {
  size_t idx = ssl->d1->handshake_read_seq % SSL_MAX_HANDSHAKE_FLIGHT;
  DTLSIncomingMessage *frag = ssl->d1->incoming_messages[idx].get();
  return frag != nullptr && frag->reassembly.IsComplete();
}

static DTLSIncomingMessage *dtls1_get_incoming_message(
    SSL *ssl, uint8_t *out_alert, const struct hm_header_st *msg_hdr) {
  if (msg_hdr->seq < ssl->d1->handshake_read_seq ||
      msg_hdr->seq - ssl->d1->handshake_read_seq >= SSL_MAX_HANDSHAKE_FLIGHT) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return NULL;
  }

  size_t idx = msg_hdr->seq % SSL_MAX_HANDSHAKE_FLIGHT;
  DTLSIncomingMessage *frag = ssl->d1->incoming_messages[idx].get();
  if (frag != NULL) {
    assert(frag->seq == msg_hdr->seq);

    if (frag->type != msg_hdr->type ||
        frag->msg_len() != msg_hdr->msg_len) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_FRAGMENT_MISMATCH);
      *out_alert = SSL_AD_ILLEGAL_PARAMETER;
      return NULL;
    }
    return frag;
  }

  ssl->d1->incoming_messages[idx] = dtls_new_incoming_message(msg_hdr);
  if (!ssl->d1->incoming_messages[idx]) {
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return NULL;
  }
  return ssl->d1->incoming_messages[idx].get();
}

bool dtls1_process_handshake_fragments(SSL *ssl, uint8_t *out_alert,
                                       DTLSRecordNumber record_number,
                                       Span<const uint8_t> record) {
  bool implicit_ack = false;
  bool skipped_fragments = false;
  CBS cbs = record;
  while (CBS_len(&cbs) > 0) {

    struct hm_header_st msg_hdr;
    CBS body;
    if (!dtls1_parse_fragment(&cbs, &msg_hdr, &body)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_HANDSHAKE_RECORD);
      *out_alert = SSL_AD_DECODE_ERROR;
      return false;
    }

    const size_t frag_off = msg_hdr.frag_off;
    const size_t frag_len = msg_hdr.frag_len;
    const size_t msg_len = msg_hdr.msg_len;
    if (frag_off > msg_len || frag_len > msg_len - frag_off) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_HANDSHAKE_RECORD);
      *out_alert = SSL_AD_ILLEGAL_PARAMETER;
      return false;
    }

    if (msg_hdr.seq < ssl->d1->handshake_read_seq ||
        ssl->d1->handshake_read_overflow) {

      continue;
    }

    if (record_number.epoch() != ssl->d1->read_epoch.epoch ||
        ssl->d1->next_read_epoch != nullptr) {

      OPENSSL_PUT_ERROR(SSL, SSL_R_EXCESS_HANDSHAKE_DATA);
      *out_alert = SSL_AD_UNEXPECTED_MESSAGE;
      return false;
    }

    if (msg_len > ssl_max_handshake_message_len(ssl)) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_EXCESSIVE_MESSAGE_SIZE);
      *out_alert = SSL_AD_ILLEGAL_PARAMETER;
      return false;
    }

    if (SSL_in_init(ssl) && ssl_has_final_version(ssl) &&
        ssl_protocol_version(ssl) >= TLS1_3_VERSION) {

      implicit_ack = true;
    }

    if (msg_hdr.seq - ssl->d1->handshake_read_seq > SSL_MAX_HANDSHAKE_FLIGHT) {

      skipped_fragments = true;
      continue;
    }

    DTLSIncomingMessage *frag =
        dtls1_get_incoming_message(ssl, out_alert, &msg_hdr);
    if (frag == nullptr) {
      return false;
    }
    assert(frag->msg_len() == msg_len);

    if (frag->reassembly.IsComplete()) {

      continue;
    }
    assert(msg_len > 0);

    Span<uint8_t> dest = frag->msg().subspan(frag_off, CBS_len(&body));
    OPENSSL_memcpy(dest.data(), CBS_data(&body), CBS_len(&body));
    frag->reassembly.MarkRange(frag_off, frag_off + frag_len);
  }

  if (implicit_ack) {
    dtls1_stop_timer(ssl);
    dtls_clear_outgoing_messages(ssl);
  }

  if (!skipped_fragments) {
    ssl->d1->records_to_ack.PushBack(record_number);

    if (ssl_has_final_version(ssl) &&
        ssl_protocol_version(ssl) >= TLS1_3_VERSION &&
        !ssl->d1->ack_timer.IsSet() && !ssl->d1->sending_ack) {

      OPENSSL_timeval now = ssl_ctx_get_current_time(ssl->ctx.get());
      ssl->d1->ack_timer.StartMicroseconds(
          now, uint64_t{ssl->d1->timeout_duration_ms} * 1000 / 4);
    }
  }

  return true;
}

ssl_open_record_t dtls1_open_handshake(SSL *ssl, size_t *out_consumed,
                                       uint8_t *out_alert, Span<uint8_t> in) {
  uint8_t type;
  DTLSRecordNumber record_number;
  Span<uint8_t> record;
  auto ret = dtls_open_record(ssl, &type, &record_number, &record, out_consumed,
                              out_alert, in);
  if (ret != ssl_open_record_success) {
    return ret;
  }

  switch (type) {
    case SSL3_RT_APPLICATION_DATA:

      return ssl_open_record_discard;

    case SSL3_RT_CHANGE_CIPHER_SPEC:
      if (record.size() != 1u || record[0] != SSL3_MT_CCS) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_CHANGE_CIPHER_SPEC);
        *out_alert = SSL_AD_ILLEGAL_PARAMETER;
        return ssl_open_record_error;
      }

      if (record_number.epoch() != 0) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_RECORD);
        *out_alert = SSL_AD_UNEXPECTED_MESSAGE;
        return ssl_open_record_error;
      }

      if (record_number.epoch() != ssl->d1->read_epoch.epoch) {
        return ssl_open_record_discard;
      }

      ssl->d1->has_change_cipher_spec = true;
      ssl_do_msg_callback(ssl, 0           , SSL3_RT_CHANGE_CIPHER_SPEC,
                          record);
      return ssl_open_record_success;

    case SSL3_RT_ACK:
      return dtls1_process_ack(ssl, out_alert, record_number, record);

    case SSL3_RT_HANDSHAKE:
      if (!dtls1_process_handshake_fragments(ssl, out_alert, record_number,
                                             record)) {
        return ssl_open_record_error;
      }
      return ssl_open_record_success;

    default:
      OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_RECORD);
      *out_alert = SSL_AD_UNEXPECTED_MESSAGE;
      return ssl_open_record_error;
  }
}

bool dtls1_get_message(const SSL *ssl, SSLMessage *out) {
  if (!dtls1_is_current_message_complete(ssl)) {
    return false;
  }

  size_t idx = ssl->d1->handshake_read_seq % SSL_MAX_HANDSHAKE_FLIGHT;
  const DTLSIncomingMessage *frag = ssl->d1->incoming_messages[idx].get();
  out->type = frag->type;
  out->raw = CBS(frag->data);
  out->body = CBS(frag->msg());
  out->is_v2_hello = false;
  if (!ssl->s3->has_message) {
    ssl_do_msg_callback(ssl, 0           , SSL3_RT_HANDSHAKE, out->raw);
    ssl->s3->has_message = true;
  }
  return true;
}

void dtls1_next_message(SSL *ssl) {
  assert(ssl->s3->has_message);
  assert(dtls1_is_current_message_complete(ssl));
  size_t index = ssl->d1->handshake_read_seq % SSL_MAX_HANDSHAKE_FLIGHT;
  ssl->d1->incoming_messages[index].reset();
  ssl->d1->handshake_read_seq++;
  if (ssl->d1->handshake_read_seq == 0) {
    ssl->d1->handshake_read_overflow = true;
  }
  ssl->s3->has_message = false;

  if (ssl->d1->outgoing_messages_complete) {
    ssl->d1->flight_has_reply = true;
  }
}

bool dtls_has_unprocessed_handshake_data(const SSL *ssl) {
  size_t current = ssl->d1->handshake_read_seq % SSL_MAX_HANDSHAKE_FLIGHT;
  for (size_t i = 0; i < SSL_MAX_HANDSHAKE_FLIGHT; i++) {

    if (ssl->s3->has_message && i == current) {
      assert(dtls1_is_current_message_complete(ssl));
      continue;
    }
    if (ssl->d1->incoming_messages[i] != nullptr) {
      return true;
    }
  }
  return false;
}

bool dtls1_parse_fragment(CBS *cbs, struct hm_header_st *out_hdr,
                          CBS *out_body) {
  OPENSSL_memset(out_hdr, 0x00, sizeof(struct hm_header_st));

  if (!CBS_get_u8(cbs, &out_hdr->type) ||
      !CBS_get_u24(cbs, &out_hdr->msg_len) ||
      !CBS_get_u16(cbs, &out_hdr->seq) ||
      !CBS_get_u24(cbs, &out_hdr->frag_off) ||
      !CBS_get_u24(cbs, &out_hdr->frag_len) ||
      !CBS_get_bytes(cbs, out_body, out_hdr->frag_len)) {
    return false;
  }

  return true;
}

ssl_open_record_t dtls1_open_change_cipher_spec(SSL *ssl, size_t *out_consumed,
                                                uint8_t *out_alert,
                                                Span<uint8_t> in) {
  if (!ssl->d1->has_change_cipher_spec) {

    auto ret = dtls1_open_handshake(ssl, out_consumed, out_alert, in);
    if (ret != ssl_open_record_success) {
      return ret;
    }
  }
  if (ssl->d1->has_change_cipher_spec) {
    ssl->d1->has_change_cipher_spec = false;
    return ssl_open_record_success;
  }
  return ssl_open_record_discard;
}

void dtls_clear_outgoing_messages(SSL *ssl) {
  ssl->d1->outgoing_messages.clear();
  ssl->d1->sent_records = nullptr;
  ssl->d1->outgoing_written = 0;
  ssl->d1->outgoing_offset = 0;
  ssl->d1->outgoing_messages_complete = false;
  ssl->d1->flight_has_reply = false;
  ssl->d1->sending_flight = false;
  dtls_clear_unused_write_epochs(ssl);
}

void dtls_clear_unused_write_epochs(SSL *ssl) {
  ssl->d1->extra_write_epochs.EraseIf(
      [ssl](const UniquePtr<DTLSWriteEpoch> &write_epoch) -> bool {

        for (const auto &msg : ssl->d1->outgoing_messages) {
          if (msg.epoch == write_epoch->epoch() && !msg.IsFullyAcked()) {
            return false;
          }
        }
        return true;
      });
}

bool dtls1_init_message(const SSL *ssl, CBB *cbb, CBB *body, uint8_t type) {

  if (!CBB_init(cbb, 64) ||
      !CBB_add_u8(cbb, type) ||
      !CBB_add_u24(cbb, 0                               ) ||
      !CBB_add_u16(cbb, ssl->d1->handshake_write_seq) ||
      !CBB_add_u24(cbb, 0             ) ||
      !CBB_add_u24_length_prefixed(cbb, body)) {
    return false;
  }

  return true;
}

bool dtls1_finish_message(const SSL *ssl, CBB *cbb, Array<uint8_t> *out_msg) {
  if (!CBBFinishArray(cbb, out_msg) ||
      out_msg->size() < DTLS1_HM_HEADER_LENGTH) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  OPENSSL_memcpy(out_msg->data() + 1,
                 out_msg->data() + DTLS1_HM_HEADER_LENGTH - 3, 3);
  return true;
}

static bool add_outgoing(SSL *ssl, bool is_ccs, Array<uint8_t> data) {
  if (ssl->d1->outgoing_messages_complete) {

    dtls1_stop_timer(ssl);
    dtls_clear_outgoing_messages(ssl);
  }

  if (!is_ccs) {
    if (ssl->d1->handshake_write_overflow) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
      return false;
    }

    if (ssl->s3->hs != NULL && !ssl->s3->hs->transcript.Update(data)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return false;
    }
    ssl->d1->handshake_write_seq++;
    if (ssl->d1->handshake_write_seq == 0) {
      ssl->d1->handshake_write_overflow = true;
    }
  }

  DTLSOutgoingMessage msg;
  msg.data = std::move(data);
  msg.epoch = ssl->d1->write_epoch.epoch();
  msg.is_ccs = is_ccs;

  if (!is_ccs && !msg.acked.Init(std::max(msg.msg_len(), size_t{1}))) {
    return false;
  }

  if (!ssl->d1->outgoing_messages.TryPushBack(std::move(msg))) {
    assert(false);
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  return true;
}

bool dtls1_add_message(SSL *ssl, Array<uint8_t> data) {
  return add_outgoing(ssl, false                , std::move(data));
}

bool dtls1_add_change_cipher_spec(SSL *ssl) {

  if (ssl_protocol_version(ssl) > TLS1_2_VERSION) {
    return true;
  }
  return add_outgoing(ssl, true                       , Array<uint8_t>());
}

static void dtls1_update_mtu(SSL *ssl) {

  if (ssl->d1->mtu < dtls1_min_mtu() &&
      !(SSL_get_options(ssl) & SSL_OP_NO_QUERY_MTU)) {
    long mtu = BIO_ctrl(ssl->wbio.get(), BIO_CTRL_DGRAM_QUERY_MTU, 0, NULL);
    if (mtu >= 0 && mtu <= (1 << 30) && (unsigned)mtu >= dtls1_min_mtu()) {
      ssl->d1->mtu = (unsigned)mtu;
    } else {
      ssl->d1->mtu = kDefaultMTU;
      BIO_ctrl(ssl->wbio.get(), BIO_CTRL_DGRAM_SET_MTU, ssl->d1->mtu, NULL);
    }
  }

  assert(ssl->d1->mtu >= dtls1_min_mtu());
}

enum seal_result_t {
  seal_error,
  seal_continue,
  seal_flush,
};

static seal_result_t seal_next_record(SSL *ssl, Span<uint8_t> out,
                                      size_t *out_len) {
  *out_len = 0;

  while (ssl->d1->outgoing_written < ssl->d1->outgoing_messages.size() &&
         ssl->d1->outgoing_messages[ssl->d1->outgoing_written].IsFullyAcked()) {
    ssl->d1->outgoing_offset = 0;
    ssl->d1->outgoing_written++;
  }

  if (ssl->d1->outgoing_written >= ssl->d1->outgoing_messages.size()) {
    return seal_flush;
  }

  const auto &first_msg = ssl->d1->outgoing_messages[ssl->d1->outgoing_written];
  size_t prefix_len = dtls_seal_prefix_len(ssl, first_msg.epoch);
  size_t max_in_len = dtls_seal_max_input_len(ssl, first_msg.epoch, out.size());
  if (max_in_len == 0) {

    return seal_flush;
  }

  if (first_msg.is_ccs) {
    static const uint8_t kChangeCipherSpec[1] = {SSL3_MT_CCS};
    DTLSRecordNumber record_number;
    if (!dtls_seal_record(ssl, &record_number, out.data(), out_len, out.size(),
                          SSL3_RT_CHANGE_CIPHER_SPEC, kChangeCipherSpec,
                          sizeof(kChangeCipherSpec), first_msg.epoch)) {
      return seal_error;
    }

    ssl_do_msg_callback(ssl,              1, SSL3_RT_CHANGE_CIPHER_SPEC,
                        kChangeCipherSpec);
    ssl->d1->outgoing_offset = 0;
    ssl->d1->outgoing_written++;
    return seal_continue;
  }

  const bool allow_multiple_messages = first_msg.epoch != 0;

  bool should_continue = false;
  Span<uint8_t> fragments = out.subspan(prefix_len, max_in_len);
  CBB cbb;
  CBB_init_fixed(&cbb, fragments.data(), fragments.size());
  DTLSSentRecord sent_record;
  sent_record.first_msg = ssl->d1->outgoing_written;
  sent_record.first_msg_start = ssl->d1->outgoing_offset;
  while (ssl->d1->outgoing_written < ssl->d1->outgoing_messages.size()) {
    const auto &msg = ssl->d1->outgoing_messages[ssl->d1->outgoing_written];
    if (msg.epoch != first_msg.epoch || msg.is_ccs) {

      should_continue = true;
      break;
    }

    CBS cbs(msg.data), body_cbs;
    struct hm_header_st hdr;
    if (!dtls1_parse_fragment(&cbs, &hdr, &body_cbs) ||
        hdr.frag_off != 0 ||
        hdr.frag_len != CBS_len(&body_cbs) ||
        hdr.msg_len != CBS_len(&body_cbs) ||
        CBS_len(&cbs) != 0) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return seal_error;
    }

    Span<const uint8_t> body = body_cbs;
    for (;;) {
      auto range = msg.acked.NextUnmarkedRange(ssl->d1->outgoing_offset);
      if (range.empty()) {

        ssl->d1->outgoing_offset = 0;
        ssl->d1->outgoing_written++;
        break;
      }

      size_t capacity = fragments.size() - CBB_len(&cbb);
      if (capacity < DTLS1_HM_HEADER_LENGTH + 1) {
        goto packet_full;
      }
      size_t todo = std::min(range.size(), capacity - DTLS1_HM_HEADER_LENGTH);

      Span<const uint8_t> frag;
      if (!body.empty()) {
        frag = body.subspan(range.start, todo);
      }

      size_t frag_start = CBB_len(&cbb);
      CBB child;
      if (!CBB_add_u8(&cbb, hdr.type) ||
          !CBB_add_u24(&cbb, hdr.msg_len) ||
          !CBB_add_u16(&cbb, hdr.seq) ||
          !CBB_add_u24(&cbb, range.start) ||
          !CBB_add_u24_length_prefixed(&cbb, &child) ||
          !CBB_add_bytes(&child, frag.data(), frag.size()) ||
          !CBB_flush(&cbb)) {
        OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
        return seal_error;
      }
      size_t frag_end = CBB_len(&cbb);

      ssl_do_msg_callback(ssl,              1, SSL3_RT_HANDSHAKE,
                          fragments.subspan(frag_start, frag_end - frag_start));

      ssl->d1->outgoing_offset = range.start + todo;
      if (todo < range.size()) {

        goto packet_full;
      }
    }

    if (!allow_multiple_messages) {
      should_continue = true;
      break;
    }
  }

packet_full:
  sent_record.last_msg = ssl->d1->outgoing_written;
  sent_record.last_msg_end = ssl->d1->outgoing_offset;

  if (CBB_len(&cbb) == 0) {
    assert(!should_continue);
    return seal_flush;
  }

  if (!dtls_seal_record(ssl, &sent_record.number, out.data(), out_len,
                        out.size(), SSL3_RT_HANDSHAKE, CBB_data(&cbb),
                        CBB_len(&cbb), first_msg.epoch)) {
    return seal_error;
  }

  if (ssl->s3->version == 0 || ssl_protocol_version(ssl) >= TLS1_3_VERSION) {
    if (ssl->d1->sent_records == nullptr) {
      ssl->d1->sent_records =
          MakeUnique<MRUQueue<DTLSSentRecord, DTLS_MAX_ACK_BUFFER>>();
      if (ssl->d1->sent_records == nullptr) {
        return seal_error;
      }
    }
    ssl->d1->sent_records->PushBack(sent_record);
  }

  return should_continue ? seal_continue : seal_flush;
}

static bool seal_next_packet(SSL *ssl, Span<uint8_t> out, size_t *out_len) {
  size_t total = 0;
  for (;;) {
    size_t len;
    seal_result_t ret = seal_next_record(ssl, out, &len);
    switch (ret) {
      case seal_error:
        return false;

      case seal_flush:
      case seal_continue:
        out = out.subspan(len);
        total += len;
        break;
    }

    if (ret == seal_flush) {
      break;
    }
  }

  *out_len = total;
  return true;
}

static int send_flight(SSL *ssl) {
  if (ssl->s3->write_shutdown != ssl_shutdown_none) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_PROTOCOL_IS_SHUTDOWN);
    return -1;
  }

  if (ssl->wbio == nullptr) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BIO_NOT_SET);
    return -1;
  }

  if (ssl->d1->num_timeouts > DTLS1_MAX_TIMEOUTS) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_READ_TIMEOUT_EXPIRED);
    return -1;
  }

  dtls1_update_mtu(ssl);

  Array<uint8_t> packet;
  if (!packet.InitForOverwrite(ssl->d1->mtu)) {
    return -1;
  }

  while (ssl->d1->outgoing_written < ssl->d1->outgoing_messages.size()) {
    uint8_t old_written = ssl->d1->outgoing_written;
    uint32_t old_offset = ssl->d1->outgoing_offset;

    size_t packet_len;
    if (!seal_next_packet(ssl, Span(packet), &packet_len)) {
      return -1;
    }

    if (packet_len == 0 &&
        ssl->d1->outgoing_written < ssl->d1->outgoing_messages.size()) {

      OPENSSL_PUT_ERROR(SSL, SSL_R_MTU_TOO_SMALL);
      return false;
    }

    if (packet_len != 0) {
      int bio_ret = BIO_write(ssl->wbio.get(), packet.data(), packet_len);
      if (bio_ret <= 0) {

        ssl->d1->outgoing_written = old_written;
        ssl->d1->outgoing_offset = old_offset;
        ssl->s3->rwstate = SSL_ERROR_WANT_WRITE;
        return bio_ret;
      }
    }
  }

  if (BIO_flush(ssl->wbio.get()) <= 0) {
    ssl->s3->rwstate = SSL_ERROR_WANT_WRITE;
    return -1;
  }

  return 1;
}

void dtls1_finish_flight(SSL *ssl) {
  if (ssl->d1->outgoing_messages.empty() ||
      ssl->d1->outgoing_messages_complete) {
    return;
  }

  if (ssl->d1->outgoing_messages[0].epoch <= 2) {

    ssl->d1->records_to_ack.Clear();
    ssl->d1->ack_timer.Stop();
    ssl->d1->sending_ack = false;
  }

  ssl->d1->outgoing_messages_complete = true;
  ssl->d1->sending_flight = true;

  dtls1_stop_timer(ssl);
}

void dtls1_schedule_ack(SSL *ssl) {
  ssl->d1->ack_timer.Stop();
  ssl->d1->sending_ack = !ssl->d1->records_to_ack.empty();
}

static int send_ack(SSL *ssl) {
  assert(ssl_protocol_version(ssl) >= TLS1_3_VERSION);

  dtls1_update_mtu(ssl);
  size_t max_plaintext =
      dtls_seal_max_input_len(ssl, ssl->d1->write_epoch.epoch(), ssl->d1->mtu);
  if (max_plaintext < 2 + 16) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_MTU_TOO_SMALL);
    return -1;
  }
  size_t num_acks =
      std::min((max_plaintext - 2) / 16, ssl->d1->records_to_ack.size());

  InplaceVector<DTLSRecordNumber, DTLS_MAX_ACK_BUFFER> sorted;
  for (size_t i = ssl->d1->records_to_ack.size() - num_acks;
       i < ssl->d1->records_to_ack.size(); i++) {
    sorted.PushBack(ssl->d1->records_to_ack[i]);
  }
  std::sort(sorted.begin(), sorted.end());

  uint8_t buf[2 + 16 * DTLS_MAX_ACK_BUFFER];
  CBB cbb, child;
  CBB_init_fixed(&cbb, buf, sizeof(buf));
  BSSL_CHECK(CBB_add_u16_length_prefixed(&cbb, &child));
  for (const auto &number : sorted) {
    BSSL_CHECK(CBB_add_u64(&child, number.epoch()));
    BSSL_CHECK(CBB_add_u64(&child, number.sequence()));
  }
  BSSL_CHECK(CBB_flush(&cbb));

  uint8_t record[DTLS1_3_RECORD_HEADER_WRITE_LENGTH + sizeof(buf) +
                 1                   + EVP_AEAD_MAX_OVERHEAD];
  size_t record_len;
  DTLSRecordNumber record_number;
  if (!dtls_seal_record(ssl, &record_number, record, &record_len,
                        sizeof(record), SSL3_RT_ACK, CBB_data(&cbb),
                        CBB_len(&cbb), ssl->d1->write_epoch.epoch())) {
    return -1;
  }

  ssl_do_msg_callback(ssl,              1, SSL3_RT_ACK,
                      Span(CBB_data(&cbb), CBB_len(&cbb)));

  int bio_ret =
      BIO_write(ssl->wbio.get(), record, static_cast<int>(record_len));
  if (bio_ret <= 0) {
    ssl->s3->rwstate = SSL_ERROR_WANT_WRITE;
    return bio_ret;
  }

  if (BIO_flush(ssl->wbio.get()) <= 0) {
    ssl->s3->rwstate = SSL_ERROR_WANT_WRITE;
    return -1;
  }

  return 1;
}

int dtls1_flush(SSL *ssl) {

  if (ssl->d1->sending_ack) {
    int ret = send_ack(ssl);
    if (ret <= 0) {
      return ret;
    }
    ssl->d1->sending_ack = false;
  }

  if (ssl->d1->sending_flight) {
    int ret = send_flight(ssl);
    if (ret <= 0) {
      return ret;
    }

    ssl->d1->outgoing_written = 0;
    ssl->d1->outgoing_offset = 0;
    ssl->d1->sending_flight = false;

    if (SSL_in_init(ssl) || ssl_protocol_version(ssl) >= TLS1_3_VERSION) {
      if (ssl->d1->num_timeouts == 0) {
        ssl->d1->timeout_duration_ms = ssl->initial_timeout_duration_ms;
      } else {
        ssl->d1->timeout_duration_ms =
            std::min(ssl->d1->timeout_duration_ms * 2, uint32_t{60000});
      }

      OPENSSL_timeval now = ssl_ctx_get_current_time(ssl->ctx.get());
      ssl->d1->retransmit_timer.StartMicroseconds(
          now, uint64_t{ssl->d1->timeout_duration_ms} * 1000);
    }
  }

  return 1;
}

unsigned int dtls1_min_mtu(void) { return kMinMTU; }

BSSL_NAMESPACE_END
