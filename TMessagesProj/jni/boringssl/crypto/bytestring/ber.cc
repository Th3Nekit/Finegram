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

#include <openssl/bytestring.h>

#include <assert.h>
#include <string.h>

#include "internal.h"

static const uint32_t kMaxDepth = 128;

static int is_string_type(CBS_ASN1_TAG tag) {

  switch (tag & ~CBS_ASN1_CONSTRUCTED) {
    case CBS_ASN1_OCTETSTRING:
    case CBS_ASN1_UTF8STRING:
    case CBS_ASN1_NUMERICSTRING:
    case CBS_ASN1_PRINTABLESTRING:
    case CBS_ASN1_T61STRING:
    case CBS_ASN1_VIDEOTEXSTRING:
    case CBS_ASN1_IA5STRING:
    case CBS_ASN1_GRAPHICSTRING:
    case CBS_ASN1_VISIBLESTRING:
    case CBS_ASN1_GENERALSTRING:
    case CBS_ASN1_UNIVERSALSTRING:
    case CBS_ASN1_BMPSTRING:
      return 1;
    default:
      return 0;
  }
}

static int cbs_find_ber(const CBS *orig_in, int *ber_found, uint32_t depth) {
  if (depth > kMaxDepth) {
    return 0;
  }

  CBS in = *orig_in;
  *ber_found = 0;

  while (CBS_len(&in) > 0) {
    CBS contents;
    CBS_ASN1_TAG tag;
    size_t header_len;
    int indefinite;
    if (!CBS_get_any_ber_asn1_element(&in, &contents, &tag, &header_len,
                                      ber_found, &indefinite)) {
      return 0;
    }
    if (*ber_found) {
      return 1;
    }
    if (tag & CBS_ASN1_CONSTRUCTED) {
      if (is_string_type(tag)) {

        *ber_found = 1;
        return 1;
      }
      if (!CBS_skip(&contents, header_len) ||
          !cbs_find_ber(&contents, ber_found, depth + 1)) {
        return 0;
      }
      if (*ber_found) {

        return 1;
      }
    }
  }

  return 1;
}

static int cbs_get_eoc(CBS *cbs) {
  if (CBS_len(cbs) >= 2 &&
      CBS_data(cbs)[0] == 0 && CBS_data(cbs)[1] == 0) {
    return CBS_skip(cbs, 2);
  }
  return 0;
}

static int cbs_convert_ber(CBS *in, CBB *out, CBS_ASN1_TAG string_tag,
                           int looking_for_eoc, uint32_t depth) {
  assert(!(string_tag & CBS_ASN1_CONSTRUCTED));

  if (depth > kMaxDepth) {
    return 0;
  }

  while (CBS_len(in) > 0) {
    if (looking_for_eoc && cbs_get_eoc(in)) {
      return 1;
    }

    CBS contents;
    CBS_ASN1_TAG tag, child_string_tag = string_tag;
    size_t header_len;
    int indefinite;
    CBB *out_contents, out_contents_storage;
    if (!CBS_get_any_ber_asn1_element(in, &contents, &tag, &header_len,
                                                        NULL, &indefinite)) {
      return 0;
    }

    if (string_tag != 0) {

      if ((tag & ~CBS_ASN1_CONSTRUCTED) != string_tag) {
        return 0;
      }
      out_contents = out;
    } else {
      CBS_ASN1_TAG out_tag = tag;
      if ((tag & CBS_ASN1_CONSTRUCTED) && is_string_type(tag)) {

        out_tag &= ~CBS_ASN1_CONSTRUCTED;
        child_string_tag = out_tag;
      }
      if (!CBB_add_asn1(out, &out_contents_storage, out_tag)) {
        return 0;
      }
      out_contents = &out_contents_storage;
    }

    if (indefinite) {
      if (!cbs_convert_ber(in, out_contents, child_string_tag,
                                               1, depth + 1) ||
          !CBB_flush(out)) {
        return 0;
      }
      continue;
    }

    if (!CBS_skip(&contents, header_len)) {
      return 0;
    }

    if (tag & CBS_ASN1_CONSTRUCTED) {

      if (!cbs_convert_ber(&contents, out_contents, child_string_tag,
                                               0, depth + 1)) {
        return 0;
      }
    } else {

      if (!CBB_add_bytes(out_contents, CBS_data(&contents),
                         CBS_len(&contents))) {
        return 0;
      }
    }

    if (!CBB_flush(out)) {
      return 0;
    }
  }

  return looking_for_eoc == 0;
}

int CBS_asn1_ber_to_der(CBS *in, CBS *out, uint8_t **out_storage) {
  CBB cbb;

  int conversion_needed;
  if (!cbs_find_ber(in, &conversion_needed, 0)) {
    return 0;
  }

  if (!conversion_needed) {
    if (!CBS_get_any_asn1_element(in, out, NULL, NULL)) {
      return 0;
    }
    *out_storage = NULL;
    return 1;
  }

  size_t len;
  if (!CBB_init(&cbb, CBS_len(in)) ||
      !cbs_convert_ber(in, &cbb, 0, 0, 0) ||
      !CBB_finish(&cbb, out_storage, &len)) {
    CBB_cleanup(&cbb);
    return 0;
  }

  CBS_init(out, *out_storage, len);
  return 1;
}

int CBS_get_asn1_implicit_string(CBS *in, CBS *out, uint8_t **out_storage,
                                 CBS_ASN1_TAG outer_tag,
                                 CBS_ASN1_TAG inner_tag) {
  assert(!(outer_tag & CBS_ASN1_CONSTRUCTED));
  assert(!(inner_tag & CBS_ASN1_CONSTRUCTED));
  assert(is_string_type(inner_tag));

  if (CBS_peek_asn1_tag(in, outer_tag)) {

    *out_storage = NULL;
    return CBS_get_asn1(in, out, outer_tag);
  }

  CBB result;
  CBS child;
  if (!CBB_init(&result, CBS_len(in)) ||
      !CBS_get_asn1(in, &child, outer_tag | CBS_ASN1_CONSTRUCTED)) {
    goto err;
  }

  while (CBS_len(&child) > 0) {
    CBS chunk;
    if (!CBS_get_asn1(&child, &chunk, inner_tag) ||
        !CBB_add_bytes(&result, CBS_data(&chunk), CBS_len(&chunk))) {
      goto err;
    }
  }

  uint8_t *data;
  size_t len;
  if (!CBB_finish(&result, &data, &len)) {
    goto err;
  }

  CBS_init(out, data, len);
  *out_storage = data;
  return 1;

err:
  CBB_cleanup(&result);
  return 0;
}
