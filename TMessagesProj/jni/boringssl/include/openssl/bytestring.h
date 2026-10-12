// Copyright 2014 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_BYTESTRING_H
#define OPENSSL_HEADER_BYTESTRING_H

#include <openssl/base.h>

#include <openssl/span.h>
#include <time.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct cbs_st {
  const uint8_t *data;
  size_t len;

#if !defined(BORINGSSL_NO_CXX)

  cbs_st(bssl::Span<const uint8_t> span)
      : data(span.data()), len(span.size()) {}
  operator bssl::Span<const uint8_t>() const { return bssl::Span(data, len); }

  cbs_st() = default;
  cbs_st(const cbs_st &) = default;
  cbs_st &operator=(const cbs_st &) = default;
#endif
};

OPENSSL_INLINE void CBS_init(CBS *cbs, const uint8_t *data, size_t len) {
  cbs->data = data;
  cbs->len = len;
}

OPENSSL_EXPORT int CBS_skip(CBS *cbs, size_t len);

OPENSSL_INLINE const uint8_t *CBS_data(const CBS *cbs) { return cbs->data; }

OPENSSL_INLINE size_t CBS_len(const CBS *cbs) { return cbs->len; }

OPENSSL_EXPORT int CBS_stow(const CBS *cbs, uint8_t **out_ptr, size_t *out_len);

OPENSSL_EXPORT int CBS_strdup(const CBS *cbs, char **out_ptr);

OPENSSL_EXPORT int CBS_contains_zero_byte(const CBS *cbs);

OPENSSL_EXPORT int CBS_mem_equal(const CBS *cbs, const uint8_t *data,
                                 size_t len);

OPENSSL_EXPORT int CBS_get_u8(CBS *cbs, uint8_t *out);

OPENSSL_EXPORT int CBS_get_u16(CBS *cbs, uint16_t *out);

OPENSSL_EXPORT int CBS_get_u16le(CBS *cbs, uint16_t *out);

OPENSSL_EXPORT int CBS_get_u24(CBS *cbs, uint32_t *out);

OPENSSL_EXPORT int CBS_get_u32(CBS *cbs, uint32_t *out);

OPENSSL_EXPORT int CBS_get_u32le(CBS *cbs, uint32_t *out);

OPENSSL_EXPORT int CBS_get_u64(CBS *cbs, uint64_t *out);

OPENSSL_EXPORT int CBS_get_u64le(CBS *cbs, uint64_t *out);

OPENSSL_EXPORT int CBS_get_last_u8(CBS *cbs, uint8_t *out);

OPENSSL_EXPORT int CBS_get_bytes(CBS *cbs, CBS *out, size_t len);

OPENSSL_EXPORT int CBS_copy_bytes(CBS *cbs, uint8_t *out, size_t len);

OPENSSL_EXPORT int CBS_get_u8_length_prefixed(CBS *cbs, CBS *out);

OPENSSL_EXPORT int CBS_get_u16_length_prefixed(CBS *cbs, CBS *out);

OPENSSL_EXPORT int CBS_get_u24_length_prefixed(CBS *cbs, CBS *out);

OPENSSL_EXPORT int CBS_get_until_first(CBS *cbs, CBS *out, uint8_t c);

OPENSSL_EXPORT int CBS_get_u64_decimal(CBS *cbs, uint64_t *out);

#define CBS_ASN1_TAG_SHIFT 24

#define CBS_ASN1_CONSTRUCTED (0x20u << CBS_ASN1_TAG_SHIFT)

#define CBS_ASN1_UNIVERSAL (0u << CBS_ASN1_TAG_SHIFT)
#define CBS_ASN1_APPLICATION (0x40u << CBS_ASN1_TAG_SHIFT)
#define CBS_ASN1_CONTEXT_SPECIFIC (0x80u << CBS_ASN1_TAG_SHIFT)
#define CBS_ASN1_PRIVATE (0xc0u << CBS_ASN1_TAG_SHIFT)

#define CBS_ASN1_CLASS_MASK (0xc0u << CBS_ASN1_TAG_SHIFT)

#define CBS_ASN1_TAG_NUMBER_MASK ((1u << (5 + CBS_ASN1_TAG_SHIFT)) - 1)

#define CBS_ASN1_BOOLEAN 0x1u
#define CBS_ASN1_INTEGER 0x2u
#define CBS_ASN1_BITSTRING 0x3u
#define CBS_ASN1_OCTETSTRING 0x4u
#define CBS_ASN1_NULL 0x5u
#define CBS_ASN1_OBJECT 0x6u
#define CBS_ASN1_ENUMERATED 0xau
#define CBS_ASN1_UTF8STRING 0xcu
#define CBS_ASN1_SEQUENCE (0x10u | CBS_ASN1_CONSTRUCTED)
#define CBS_ASN1_SET (0x11u | CBS_ASN1_CONSTRUCTED)
#define CBS_ASN1_NUMERICSTRING 0x12u
#define CBS_ASN1_PRINTABLESTRING 0x13u
#define CBS_ASN1_T61STRING 0x14u
#define CBS_ASN1_VIDEOTEXSTRING 0x15u
#define CBS_ASN1_IA5STRING 0x16u
#define CBS_ASN1_UTCTIME 0x17u
#define CBS_ASN1_GENERALIZEDTIME 0x18u
#define CBS_ASN1_GRAPHICSTRING 0x19u
#define CBS_ASN1_VISIBLESTRING 0x1au
#define CBS_ASN1_GENERALSTRING 0x1bu
#define CBS_ASN1_UNIVERSALSTRING 0x1cu
#define CBS_ASN1_BMPSTRING 0x1eu

OPENSSL_EXPORT int CBS_get_asn1(CBS *cbs, CBS *out, CBS_ASN1_TAG tag_value);

OPENSSL_EXPORT int CBS_get_asn1_element(CBS *cbs, CBS *out,
                                        CBS_ASN1_TAG tag_value);

OPENSSL_EXPORT int CBS_peek_asn1_tag(const CBS *cbs, CBS_ASN1_TAG tag_value);

OPENSSL_EXPORT int CBS_get_any_asn1(CBS *cbs, CBS *out,
                                    CBS_ASN1_TAG *out_tag);

OPENSSL_EXPORT int CBS_get_any_asn1_element(CBS *cbs, CBS *out,
                                            CBS_ASN1_TAG *out_tag,
                                            size_t *out_header_len);

OPENSSL_EXPORT int CBS_get_any_ber_asn1_element(CBS *cbs, CBS *out,
                                                CBS_ASN1_TAG *out_tag,
                                                size_t *out_header_len,
                                                int *out_ber_found,
                                                int *out_indefinite);

OPENSSL_EXPORT int CBS_get_asn1_uint64(CBS *cbs, uint64_t *out);

OPENSSL_EXPORT int CBS_get_asn1_uint64_with_tag(CBS *cbs, uint64_t *out,
                                                CBS_ASN1_TAG tag);

OPENSSL_EXPORT int CBS_get_asn1_int64(CBS *cbs, int64_t *out);

OPENSSL_EXPORT int CBS_get_asn1_int64_with_tag(CBS *cbs, int64_t *out,
                                               CBS_ASN1_TAG tag);

OPENSSL_EXPORT int CBS_get_asn1_bool(CBS *cbs, int *out);

OPENSSL_EXPORT int CBS_get_optional_asn1(CBS *cbs, CBS *out, int *out_present,
                                         CBS_ASN1_TAG tag);

OPENSSL_EXPORT int CBS_get_optional_asn1_octet_string(CBS *cbs, CBS *out,
                                                      int *out_present,
                                                      CBS_ASN1_TAG tag);

OPENSSL_EXPORT int CBS_get_optional_asn1_uint64(CBS *cbs, uint64_t *out,
                                                CBS_ASN1_TAG tag,
                                                uint64_t default_value);

OPENSSL_EXPORT int CBS_get_optional_asn1_bool(CBS *cbs, int *out,
                                              CBS_ASN1_TAG tag,
                                              int default_value);

OPENSSL_EXPORT int CBS_is_valid_asn1_bitstring(const CBS *cbs);

OPENSSL_EXPORT int CBS_asn1_bitstring_has_bit(const CBS *cbs, unsigned bit);

OPENSSL_EXPORT int CBS_is_valid_asn1_integer(const CBS *cbs,
                                             int *out_is_negative);

OPENSSL_EXPORT int CBS_is_unsigned_asn1_integer(const CBS *cbs);

OPENSSL_EXPORT int CBS_is_valid_asn1_oid(const CBS *cbs);

OPENSSL_EXPORT char *CBS_asn1_oid_to_text(const CBS *cbs);

OPENSSL_EXPORT int CBS_parse_generalized_time(const CBS *cbs, struct tm *out_tm,
                                              int allow_timezone_offset);

OPENSSL_EXPORT int CBS_parse_utc_time(const CBS *cbs, struct tm *out_tm,
                                      int allow_timezone_offset);

struct cbb_buffer_st {
  uint8_t *buf;

  size_t len;

  size_t cap;

  unsigned can_resize : 1;

  unsigned error : 1;
};

struct cbb_child_st {

  struct cbb_buffer_st *base;

  size_t offset;

  uint8_t pending_len_len;
  unsigned pending_is_asn1 : 1;
};

struct cbb_st {

  CBB *child;

  char is_child;
  union {
    struct cbb_buffer_st base;
    struct cbb_child_st child;
  } u;
};

OPENSSL_EXPORT void CBB_zero(CBB *cbb);

OPENSSL_EXPORT int CBB_init(CBB *cbb, size_t initial_capacity);

OPENSSL_EXPORT int CBB_init_fixed(CBB *cbb, uint8_t *buf, size_t len);

OPENSSL_EXPORT void CBB_cleanup(CBB *cbb);

OPENSSL_EXPORT int CBB_finish(CBB *cbb, uint8_t **out_data, size_t *out_len);

OPENSSL_EXPORT int CBB_flush(CBB *cbb);

OPENSSL_EXPORT const uint8_t *CBB_data(const CBB *cbb);

OPENSSL_EXPORT size_t CBB_len(const CBB *cbb);

OPENSSL_EXPORT int CBB_add_u8_length_prefixed(CBB *cbb, CBB *out_contents);

OPENSSL_EXPORT int CBB_add_u16_length_prefixed(CBB *cbb, CBB *out_contents);

OPENSSL_EXPORT int CBB_add_u24_length_prefixed(CBB *cbb, CBB *out_contents);

OPENSSL_EXPORT int CBB_add_asn1(CBB *cbb, CBB *out_contents, CBS_ASN1_TAG tag);

OPENSSL_EXPORT int CBB_add_bytes(CBB *cbb, const uint8_t *data, size_t len);

OPENSSL_EXPORT int CBB_add_zeros(CBB *cbb, size_t len);

OPENSSL_EXPORT int CBB_add_space(CBB *cbb, uint8_t **out_data, size_t len);

OPENSSL_EXPORT int CBB_reserve(CBB *cbb, uint8_t **out_data, size_t len);

OPENSSL_EXPORT int CBB_did_write(CBB *cbb, size_t len);

OPENSSL_EXPORT int CBB_add_u8(CBB *cbb, uint8_t value);

OPENSSL_EXPORT int CBB_add_u16(CBB *cbb, uint16_t value);

OPENSSL_EXPORT int CBB_add_u16le(CBB *cbb, uint16_t value);

OPENSSL_EXPORT int CBB_add_u24(CBB *cbb, uint32_t value);

OPENSSL_EXPORT int CBB_add_u32(CBB *cbb, uint32_t value);

OPENSSL_EXPORT int CBB_add_u32le(CBB *cbb, uint32_t value);

OPENSSL_EXPORT int CBB_add_u64(CBB *cbb, uint64_t value);

OPENSSL_EXPORT int CBB_add_u64le(CBB *cbb, uint64_t value);

OPENSSL_EXPORT void CBB_discard_child(CBB *cbb);

OPENSSL_EXPORT int CBB_add_asn1_uint64(CBB *cbb, uint64_t value);

OPENSSL_EXPORT int CBB_add_asn1_uint64_with_tag(CBB *cbb, uint64_t value,
                                                CBS_ASN1_TAG tag);

OPENSSL_EXPORT int CBB_add_asn1_int64(CBB *cbb, int64_t value);

OPENSSL_EXPORT int CBB_add_asn1_int64_with_tag(CBB *cbb, int64_t value,
                                               CBS_ASN1_TAG tag);

OPENSSL_EXPORT int CBB_add_asn1_octet_string(CBB *cbb, const uint8_t *data,
                                             size_t data_len);

OPENSSL_EXPORT int CBB_add_asn1_bool(CBB *cbb, int value);

OPENSSL_EXPORT int CBB_add_asn1_oid_from_text(CBB *cbb, const char *text,
                                              size_t len);

OPENSSL_EXPORT int CBB_flush_asn1_set_of(CBB *cbb);

OPENSSL_EXPORT int CBS_get_utf8(CBS *cbs, uint32_t *out);
OPENSSL_EXPORT int CBS_get_latin1(CBS *cbs, uint32_t *out);
OPENSSL_EXPORT int CBS_get_ucs2_be(CBS *cbs, uint32_t *out);
OPENSSL_EXPORT int CBS_get_utf32_be(CBS *cbs, uint32_t *out);

OPENSSL_EXPORT size_t CBB_get_utf8_len(uint32_t u);

OPENSSL_EXPORT int CBB_add_utf8(CBB *cbb, uint32_t u);
OPENSSL_EXPORT int CBB_add_latin1(CBB *cbb, uint32_t u);
OPENSSL_EXPORT int CBB_add_ucs2_be(CBB *cbb, uint32_t u);
OPENSSL_EXPORT int CBB_add_utf32_be(CBB *cbb, uint32_t u);

#if defined(__cplusplus)
}

#if !defined(BORINGSSL_NO_CXX)
extern "C++" {

BSSL_NAMESPACE_BEGIN

using ScopedCBB = internal::StackAllocated<CBB, void, CBB_zero, CBB_cleanup>;

BSSL_NAMESPACE_END

}
#endif

#endif

#endif
