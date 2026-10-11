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

#include <openssl/bytestring.h>
#include <openssl/err.h>

#include "../crypto/internal.h"
#include "internal.h"

BSSL_NAMESPACE_BEGIN

bool DTLSReplayBitmap::ShouldDiscard(uint64_t seq_num) const {
  const size_t kWindowSize = map_.size();

  if (seq_num > max_seq_num_) {
    return false;
  }
  uint64_t idx = max_seq_num_ - seq_num;
  return idx >= kWindowSize || map_[idx];
}

void DTLSReplayBitmap::Record(uint64_t seq_num) {
  const size_t kWindowSize = map_.size();

  if (seq_num > max_seq_num_) {
    uint64_t shift = seq_num - max_seq_num_;
    if (shift >= kWindowSize) {
      map_.reset();
    } else {
      map_ <<= shift;
    }
    max_seq_num_ = seq_num;
  }

  uint64_t idx = max_seq_num_ - seq_num;
  if (idx < kWindowSize) {
    map_[idx] = true;
  }
}

static uint16_t dtls_record_version(const SSL *ssl) {
  if (ssl->s3->version == 0) {

    return DTLS1_VERSION;
  }

  return ssl_protocol_version(ssl) >= TLS1_3_VERSION ? DTLS1_2_VERSION
                                                     : ssl->s3->version;
}

static uint64_t dtls_aead_sequence(const SSL *ssl, DTLSRecordNumber num) {

  return (ssl->s3->version != 0 && ssl_protocol_version(ssl) >= TLS1_3_VERSION)
             ? num.sequence()
             : num.combined();
}

static uint16_t reconstruct_epoch(uint8_t wire_epoch, uint16_t current_epoch) {
  uint16_t current_epoch_high = current_epoch & 0xfffc;
  uint16_t epoch = (wire_epoch & 0x3) | current_epoch_high;
  if (epoch > current_epoch && current_epoch_high > 0) {
    epoch -= 0x4;
  }
  return epoch;
}

uint64_t reconstruct_seqnum(uint16_t wire_seq, uint64_t seq_mask,
                            uint64_t max_valid_seqnum) {

  assert(max_valid_seqnum <= DTLSRecordNumber::kMaxSequence);
  assert(seq_mask == 0xff || seq_mask == 0xffff);

  uint64_t max_seqnum_plus_one = max_valid_seqnum + 1;
  uint64_t diff = (wire_seq - max_seqnum_plus_one) & seq_mask;
  uint64_t step = seq_mask + 1;

  uint64_t seqnum = max_seqnum_plus_one + diff;
  bool too_large = seqnum > DTLSRecordNumber::kMaxSequence;

  bool closer_is_less = diff > step / 2;

  bool would_underflow = seqnum < step;
  if (too_large || (closer_is_less && !would_underflow)) {
    seqnum -= step;
  }
  assert(seqnum <= DTLSRecordNumber::kMaxSequence);
  return seqnum;
}

static Span<uint8_t> cbs_to_writable_bytes(CBS cbs) {
  return Span(const_cast<uint8_t *>(CBS_data(&cbs)), CBS_len(&cbs));
}

struct ParsedDTLSRecord {

  DTLSReadEpoch *read_epoch = nullptr;
  DTLSRecordNumber number;
  CBS header, body;
  uint8_t type = 0;
  uint16_t version = 0;
};

static bool use_dtls13_record_header(const SSL *ssl, uint16_t epoch) {

  return ssl->s3->version != 0 && ssl_protocol_version(ssl) > TLS1_2_VERSION &&
         epoch > 0;
}

static bool parse_dtls13_record(SSL *ssl, CBS *in, ParsedDTLSRecord *out) {
  if (out->type & 0x10) {

    return false;
  }

  uint16_t max_epoch = ssl->d1->read_epoch.epoch;
  if (ssl->d1->next_read_epoch != nullptr) {
    max_epoch = std::max(max_epoch, ssl->d1->next_read_epoch->epoch);
  }
  uint16_t epoch = reconstruct_epoch(out->type, max_epoch);
  size_t seq_len = (out->type & 0x08) ? 2 : 1;
  CBS seq_bytes;
  if (!CBS_get_bytes(in, &seq_bytes, seq_len)) {
    return false;
  }
  if (out->type & 0x04) {

    if (!CBS_get_u16_length_prefixed(in, &out->body)) {
      return false;
    }
  } else {

    BSSL_CHECK(CBS_get_bytes(in, &out->body, CBS_len(in)));
  }

  if (ssl->d1->prev_read_epoch != nullptr &&
      ssl_ctx_get_current_time(ssl->ctx.get()).tv_sec >
          ssl->d1->prev_read_epoch->expire) {
    ssl->d1->prev_read_epoch = nullptr;
  }

  DTLSReadEpoch *read_epoch = nullptr;
  if (epoch == ssl->d1->read_epoch.epoch) {
    read_epoch = &ssl->d1->read_epoch;
  } else if (ssl->d1->next_read_epoch != nullptr &&
             epoch == ssl->d1->next_read_epoch->epoch) {
    read_epoch = ssl->d1->next_read_epoch.get();
  } else if (ssl->d1->prev_read_epoch != nullptr &&
             epoch == ssl->d1->prev_read_epoch->epoch.epoch) {
    read_epoch = &ssl->d1->prev_read_epoch->epoch;
  }
  if (read_epoch != nullptr && use_dtls13_record_header(ssl, epoch)) {
    out->read_epoch = read_epoch;

    uint8_t mask[2];
    if (!read_epoch->rn_encrypter->GenerateMask(mask, out->body)) {

      return false;
    }

    auto writable_seq = cbs_to_writable_bytes(seq_bytes);
    uint64_t seq = 0;
    for (size_t i = 0; i < writable_seq.size(); i++) {
      writable_seq[i] ^= mask[i];
      seq = (seq << 8) | writable_seq[i];
    }
    uint64_t full_seq = reconstruct_seqnum(seq, (1 << (seq_len * 8)) - 1,
                                           read_epoch->bitmap.max_seq_num());
    out->number = DTLSRecordNumber(epoch, full_seq);
  }

  return true;
}

static bool parse_dtls12_record(SSL *ssl, CBS *in, ParsedDTLSRecord *out) {
  uint64_t epoch_and_seq;
  if (!CBS_get_u16(in, &out->version) ||
      !CBS_get_u64(in, &epoch_and_seq) ||
      !CBS_get_u16_length_prefixed(in, &out->body)) {
    return false;
  }
  out->number = DTLSRecordNumber::FromCombined(epoch_and_seq);

  uint16_t epoch = out->number.epoch();
  bool version_ok;
  if (epoch == 0) {

    version_ok = (out->version >> 8) == DTLS1_VERSION_MAJOR;
  } else {
    version_ok = out->version == dtls_record_version(ssl);
  }
  if (!version_ok) {
    return false;
  }

  if (epoch == ssl->d1->read_epoch.epoch &&
      !use_dtls13_record_header(ssl, epoch)) {
    out->read_epoch = &ssl->d1->read_epoch;
  }

  return true;
}

static bool parse_dtls_record(SSL *ssl, CBS *cbs, ParsedDTLSRecord *out) {
  CBS copy = *cbs;
  if (!CBS_get_u8(cbs, &out->type)) {
    return false;
  }

  bool ok;
  if ((out->type & 0xe0) == 0x20) {
    ok = parse_dtls13_record(ssl, cbs, out);
  } else {
    ok = parse_dtls12_record(ssl, cbs, out);
  }
  if (!ok) {
    return false;
  }

  if (CBS_len(&out->body) > SSL3_RT_MAX_ENCRYPTED_LENGTH) {
    return false;
  }

  size_t header_len = CBS_data(&out->body) - CBS_data(&copy);
  BSSL_CHECK(CBS_get_bytes(&copy, &out->header, header_len));
  return true;
}

enum ssl_open_record_t dtls_open_record(SSL *ssl, uint8_t *out_type,
                                        DTLSRecordNumber *out_number,
                                        Span<uint8_t> *out,
                                        size_t *out_consumed,
                                        uint8_t *out_alert, Span<uint8_t> in) {
  *out_consumed = 0;
  if (ssl->s3->read_shutdown == ssl_shutdown_close_notify) {
    return ssl_open_record_close_notify;
  }

  if (in.empty()) {
    return ssl_open_record_partial;
  }

  CBS cbs(in);
  ParsedDTLSRecord record;
  if (!parse_dtls_record(ssl, &cbs, &record)) {

    *out_consumed = in.size();
    return ssl_open_record_discard;
  }

  ssl_do_msg_callback(ssl, 0           , SSL3_RT_HEADER, record.header);

  if (record.read_epoch == nullptr ||
      record.read_epoch->bitmap.ShouldDiscard(record.number.sequence())) {

    *out_consumed = in.size() - CBS_len(&cbs);
    return ssl_open_record_discard;
  }

  if (!record.read_epoch->aead->Open(out, record.type, record.version,
                                     dtls_aead_sequence(ssl, record.number),
                                     record.header,
                                     cbs_to_writable_bytes(record.body))) {

    ERR_clear_error();
    *out_consumed = in.size() - CBS_len(&cbs);
    return ssl_open_record_discard;
  }
  *out_consumed = in.size() - CBS_len(&cbs);

  bool has_padding = !record.read_epoch->aead->is_null_cipher() &&
                     ssl_protocol_version(ssl) >= TLS1_3_VERSION;

  size_t plaintext_limit = SSL3_RT_MAX_PLAIN_LENGTH + (has_padding ? 1 : 0);
  if (out->size() > plaintext_limit) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DATA_LENGTH_TOO_LONG);
    *out_alert = SSL_AD_RECORD_OVERFLOW;
    return ssl_open_record_error;
  }

  if (has_padding) {
    do {
      if (out->empty()) {
        OPENSSL_PUT_ERROR(SSL, SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC);
        *out_alert = SSL_AD_DECRYPT_ERROR;
        return ssl_open_record_error;
      }
      record.type = out->back();
      *out = out->subspan(0, out->size() - 1);
    } while (record.type == 0);
  }

  record.read_epoch->bitmap.Record(record.number.sequence());

  if (record.read_epoch == ssl->d1->next_read_epoch.get()) {
    assert(ssl_protocol_version(ssl) >= TLS1_3_VERSION);
    auto prev = MakeUnique<DTLSPrevReadEpoch>();
    if (prev == nullptr) {
      *out_alert = SSL_AD_INTERNAL_ERROR;
      return ssl_open_record_error;
    }

    prev->expire = ssl_ctx_get_current_time(ssl->ctx.get()).tv_sec;
    if (prev->expire >= UINT64_MAX - DTLS_PREV_READ_EPOCH_EXPIRE_SECONDS) {
      prev->expire = UINT64_MAX;
    } else {
      prev->expire += DTLS_PREV_READ_EPOCH_EXPIRE_SECONDS;
    }

    prev->epoch = std::move(ssl->d1->read_epoch);
    ssl->d1->prev_read_epoch = std::move(prev);
    ssl->d1->read_epoch = std::move(*ssl->d1->next_read_epoch);
    ssl->d1->next_read_epoch = nullptr;
  }

  if (record.type == SSL3_RT_ALERT) {
    return ssl_process_alert(ssl, out_alert, *out);
  }

  if (record.type == SSL3_RT_APPLICATION_DATA) {
    bool app_data_allowed;
    if (ssl->s3->version != 0 && ssl_protocol_version(ssl) >= TLS1_3_VERSION) {

      app_data_allowed =
          record.number.epoch() == 1 || record.number.epoch() >= 3;
    } else {

      app_data_allowed = record.number.epoch() >= 1;
    }
    if (!app_data_allowed) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_RECORD);
      *out_alert = SSL_AD_UNEXPECTED_MESSAGE;
      return ssl_open_record_error;
    }
  }

  ssl->s3->warning_alert_count = 0;

  *out_type = record.type;
  *out_number = record.number;
  return ssl_open_record_success;
}

static DTLSWriteEpoch *get_write_epoch(const SSL *ssl, uint16_t epoch) {
  if (ssl->d1->write_epoch.epoch() == epoch) {
    return &ssl->d1->write_epoch;
  }
  for (const auto &e : ssl->d1->extra_write_epochs) {
    if (e->epoch() == epoch) {
      return e.get();
    }
  }
  return nullptr;
}

size_t dtls_record_header_write_len(const SSL *ssl, uint16_t epoch) {
  if (!use_dtls13_record_header(ssl, epoch)) {
    return DTLS_PLAINTEXT_RECORD_HEADER_LENGTH;
  }

  return DTLS1_3_RECORD_HEADER_WRITE_LENGTH;
}

size_t dtls_max_seal_overhead(const SSL *ssl, uint16_t epoch) {
  DTLSWriteEpoch *write_epoch = get_write_epoch(ssl, epoch);
  if (write_epoch == nullptr) {
    return 0;
  }
  size_t ret = dtls_record_header_write_len(ssl, epoch) +
               write_epoch->aead->MaxOverhead();
  if (use_dtls13_record_header(ssl, epoch)) {

    ret++;
  }
  return ret;
}

size_t dtls_seal_prefix_len(const SSL *ssl, uint16_t epoch) {
  DTLSWriteEpoch *write_epoch = get_write_epoch(ssl, epoch);
  if (write_epoch == nullptr) {
    return 0;
  }
  return dtls_record_header_write_len(ssl, epoch) +
         write_epoch->aead->ExplicitNonceLen();
}

size_t dtls_seal_max_input_len(const SSL *ssl, uint16_t epoch, size_t max_out) {
  DTLSWriteEpoch *write_epoch = get_write_epoch(ssl, epoch);
  if (write_epoch == nullptr) {
    return 0;
  }
  size_t header_len = dtls_record_header_write_len(ssl, epoch);
  if (max_out <= header_len) {
    return 0;
  }
  max_out -= header_len;
  max_out = write_epoch->aead->MaxSealInputLen(max_out);
  if (max_out > 0 && use_dtls13_record_header(ssl, epoch)) {

    max_out--;
  }
  return max_out;
}

bool dtls_seal_record(SSL *ssl, DTLSRecordNumber *out_number, uint8_t *out,
                      size_t *out_len, size_t max_out, uint8_t type,
                      const uint8_t *in, size_t in_len, uint16_t epoch) {
  const size_t prefix = dtls_seal_prefix_len(ssl, epoch);
  if (buffers_alias(in, in_len, out, max_out) &&
      (max_out < prefix || out + prefix != in)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_OUTPUT_ALIASES_INPUT);
    return false;
  }

  DTLSWriteEpoch *write_epoch = get_write_epoch(ssl, epoch);
  if (write_epoch == nullptr) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  const size_t record_header_len = dtls_record_header_write_len(ssl, epoch);

  DTLSRecordNumber record_number = write_epoch->next_record;
  if (!record_number.HasNext()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
    return false;
  }

  bool dtls13_header = use_dtls13_record_header(ssl, epoch);
  uint8_t *extra_in = NULL;
  size_t extra_in_len = 0;
  if (dtls13_header) {
    extra_in = &type;
    extra_in_len = 1;
  }

  size_t ciphertext_len;
  if (!write_epoch->aead->CiphertextLen(&ciphertext_len, in_len,
                                        extra_in_len)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_RECORD_TOO_LARGE);
    return false;
  }
  if (max_out < record_header_len + ciphertext_len) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BUFFER_TOO_SMALL);
    return false;
  }

  uint16_t record_version = dtls_record_version(ssl);
  if (dtls13_header) {

    out[0] = 0x2c | (epoch & 0x3);

    CRYPTO_store_u16_be(out + 1, write_epoch->next_record.sequence());

    CRYPTO_store_u16_be(out + 3, ciphertext_len);
  } else {
    out[0] = type;
    CRYPTO_store_u16_be(out + 1, record_version);
    CRYPTO_store_u64_be(out + 3, record_number.combined());
    CRYPTO_store_u16_be(out + 11, ciphertext_len);
  }
  Span<const uint8_t> header(out, record_header_len);

  if (!write_epoch->aead->SealScatter(
          out + record_header_len, out + prefix, out + prefix + in_len, type,
          record_version, dtls_aead_sequence(ssl, record_number), header, in,
          in_len, extra_in, extra_in_len)) {
    return false;
  }

  if (dtls13_header) {

    Span<const uint8_t> sample(out + record_header_len, ciphertext_len);
    uint8_t mask[2];
    if (!write_epoch->rn_encrypter->GenerateMask(mask, sample)) {
      return false;
    }
    out[1] ^= mask[0];
    out[2] ^= mask[1];
  }

  *out_number = record_number;
  write_epoch->next_record = record_number.Next();
  *out_len = record_header_len + ciphertext_len;
  ssl_do_msg_callback(ssl, 1            , SSL3_RT_HEADER, header);
  return true;
}

BSSL_NAMESPACE_END
