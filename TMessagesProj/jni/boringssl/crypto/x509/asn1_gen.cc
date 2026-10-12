// Copyright 2002-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <openssl/x509.h>

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

#include <openssl/asn1.h>
#include <openssl/bytestring.h>
#include <openssl/err.h>
#include <openssl/obj.h>

#include "../conf/internal.h"
#include "../internal.h"
#include "internal.h"

#define ASN1_GEN_MAX_DEPTH 50

#define ASN1_GEN_MAX_OUTPUT (64 * 1024)

#define ASN1_GEN_FORMAT_ASCII 1
#define ASN1_GEN_FORMAT_UTF8 2
#define ASN1_GEN_FORMAT_HEX 3
#define ASN1_GEN_FORMAT_BITLIST 4

static int generate_v3(CBB *cbb, const char *str, const X509V3_CTX *cnf,
                       CBS_ASN1_TAG tag, int format, int depth);

static int bitstr_cb(const char *elem, size_t len, void *bitstr);

ASN1_TYPE *ASN1_generate_v3(const char *str, const X509V3_CTX *cnf) {
  CBB cbb;
  if (!CBB_init(&cbb, 0) ||
      !generate_v3(&cbb, str, cnf,         0, ASN1_GEN_FORMAT_ASCII,
                             0)) {
    CBB_cleanup(&cbb);
    return NULL;
  }

  if (CBB_len(&cbb) > ASN1_GEN_MAX_OUTPUT) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_TOO_LONG);
    CBB_cleanup(&cbb);
    return NULL;
  }

  const uint8_t *der = CBB_data(&cbb);
  ASN1_TYPE *ret = d2i_ASN1_TYPE(NULL, &der, CBB_len(&cbb));
  CBB_cleanup(&cbb);
  return ret;
}

static int cbs_str_equal(const CBS *cbs, const char *str) {
  return CBS_len(cbs) == strlen(str) &&
         OPENSSL_memcmp(CBS_data(cbs), str, strlen(str)) == 0;
}

static CBS_ASN1_TAG parse_tag(const CBS *cbs) {
  CBS copy = *cbs;
  uint64_t num;
  if (!CBS_get_u64_decimal(&copy, &num) || num > CBS_ASN1_TAG_NUMBER_MASK) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_INVALID_NUMBER);
    return 0;
  }

  CBS_ASN1_TAG tag_class = CBS_ASN1_CONTEXT_SPECIFIC;

  uint8_t c;
  if (CBS_get_u8(&copy, &c)) {
    switch (c) {
      case 'U':
        tag_class = CBS_ASN1_UNIVERSAL;
        break;
      case 'A':
        tag_class = CBS_ASN1_APPLICATION;
        break;
      case 'P':
        tag_class = CBS_ASN1_PRIVATE;
        break;
      case 'C':
        tag_class = CBS_ASN1_CONTEXT_SPECIFIC;
        break;
      default: {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_INVALID_MODIFIER);
        return 0;
      }
    }
    if (CBS_len(&copy) != 0) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_INVALID_MODIFIER);
      return 0;
    }
  }

  if (tag_class == CBS_ASN1_UNIVERSAL && num == 0) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_INVALID_NUMBER);
    return 0;
  }

  return tag_class | (CBS_ASN1_TAG)num;
}

static int generate_wrapped(CBB *cbb, const char *str, const X509V3_CTX *cnf,
                            CBS_ASN1_TAG tag, int padding, int format,
                            int depth) {
  CBB child;
  return CBB_add_asn1(cbb, &child, tag) &&
         (!padding || CBB_add_u8(&child, 0)) &&
         generate_v3(&child, str, cnf,         0, format, depth + 1) &&
         CBB_flush(cbb);
}

static int generate_v3(CBB *cbb, const char *str, const X509V3_CTX *cnf,
                       CBS_ASN1_TAG tag, int format, int depth) {
  assert((tag & CBS_ASN1_CONSTRUCTED) == 0);
  if (depth > ASN1_GEN_MAX_DEPTH) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_NESTED_TAGGING);
    return 0;
  }

  for (;;) {

    while (*str != '\0' && OPENSSL_isspace((unsigned char)*str)) {
      str++;
    }

    const char *comma = strchr(str, ',');
    if (comma == NULL) {
      break;
    }

    CBS modifier;
    CBS_init(&modifier, (const uint8_t *)str, comma - str);
    for (;;) {
      uint8_t v;
      CBS copy = modifier;
      if (!CBS_get_last_u8(&copy, &v) || !OPENSSL_isspace(v)) {
        break;
      }
      modifier = copy;
    }

    const char *str_old = str;
    str = comma + 1;

    CBS name;
    int has_value = CBS_get_until_first(&modifier, &name, ':');
    if (has_value) {
      CBS_skip(&modifier, 1);
    } else {
      name = modifier;
      CBS_init(&modifier, NULL, 0);
    }

    if (cbs_str_equal(&name, "FORMAT") || cbs_str_equal(&name, "FORM")) {
      if (cbs_str_equal(&modifier, "ASCII")) {
        format = ASN1_GEN_FORMAT_ASCII;
      } else if (cbs_str_equal(&modifier, "UTF8")) {
        format = ASN1_GEN_FORMAT_UTF8;
      } else if (cbs_str_equal(&modifier, "HEX")) {
        format = ASN1_GEN_FORMAT_HEX;
      } else if (cbs_str_equal(&modifier, "BITLIST")) {
        format = ASN1_GEN_FORMAT_BITLIST;
      } else {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_UNKNOWN_FORMAT);
        return 0;
      }
    } else if (cbs_str_equal(&name, "IMP") ||
               cbs_str_equal(&name, "IMPLICIT")) {
      if (tag != 0) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_NESTED_TAGGING);
        return 0;
      }
      tag = parse_tag(&modifier);
      if (tag == 0) {
        return 0;
      }
    } else if (cbs_str_equal(&name, "EXP") ||
               cbs_str_equal(&name, "EXPLICIT")) {

      if (tag != 0) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_NESTED_TAGGING);
        return 0;
      }
      tag = parse_tag(&modifier);
      return tag != 0 &&
             generate_wrapped(cbb, str, cnf, tag | CBS_ASN1_CONSTRUCTED,
                                          0, format, depth);
    } else if (cbs_str_equal(&name, "OCTWRAP")) {
      tag = tag == 0 ? CBS_ASN1_OCTETSTRING : tag;
      return generate_wrapped(cbb, str, cnf, tag,             0, format, depth);
    } else if (cbs_str_equal(&name, "BITWRAP")) {
      tag = tag == 0 ? CBS_ASN1_BITSTRING : tag;
      return generate_wrapped(cbb, str, cnf, tag,             1, format, depth);
    } else if (cbs_str_equal(&name, "SEQWRAP")) {
      tag = tag == 0 ? CBS_ASN1_SEQUENCE : (tag | CBS_ASN1_CONSTRUCTED);
      tag |= CBS_ASN1_CONSTRUCTED;
      return generate_wrapped(cbb, str, cnf, tag,             0, format, depth);
    } else if (cbs_str_equal(&name, "SETWRAP")) {
      tag = tag == 0 ? CBS_ASN1_SET : (tag | CBS_ASN1_CONSTRUCTED);
      return generate_wrapped(cbb, str, cnf, tag,             0, format, depth);
    } else {

      str = str_old;
      break;
    }
  }

  const char *colon = strchr(str, ':');
  CBS name;
  const char *value;
  int has_value = colon != NULL;
  if (has_value) {
    CBS_init(&name, (const uint8_t *)str, colon - str);
    value = colon + 1;
  } else {
    CBS_init(&name, (const uint8_t *)str, strlen(str));
    value = "";
  }

  static const struct {
    const char *name;
    CBS_ASN1_TAG type;
  } kTypes[] = {
      {"BOOL", CBS_ASN1_BOOLEAN},
      {"BOOLEAN", CBS_ASN1_BOOLEAN},
      {"NULL", CBS_ASN1_NULL},
      {"INT", CBS_ASN1_INTEGER},
      {"INTEGER", CBS_ASN1_INTEGER},
      {"ENUM", CBS_ASN1_ENUMERATED},
      {"ENUMERATED", CBS_ASN1_ENUMERATED},
      {"OID", CBS_ASN1_OBJECT},
      {"OBJECT", CBS_ASN1_OBJECT},
      {"UTCTIME", CBS_ASN1_UTCTIME},
      {"UTC", CBS_ASN1_UTCTIME},
      {"GENERALIZEDTIME", CBS_ASN1_GENERALIZEDTIME},
      {"GENTIME", CBS_ASN1_GENERALIZEDTIME},
      {"OCT", CBS_ASN1_OCTETSTRING},
      {"OCTETSTRING", CBS_ASN1_OCTETSTRING},
      {"BITSTR", CBS_ASN1_BITSTRING},
      {"BITSTRING", CBS_ASN1_BITSTRING},
      {"UNIVERSALSTRING", CBS_ASN1_UNIVERSALSTRING},
      {"UNIV", CBS_ASN1_UNIVERSALSTRING},
      {"IA5", CBS_ASN1_IA5STRING},
      {"IA5STRING", CBS_ASN1_IA5STRING},
      {"UTF8", CBS_ASN1_UTF8STRING},
      {"UTF8String", CBS_ASN1_UTF8STRING},
      {"BMP", CBS_ASN1_BMPSTRING},
      {"BMPSTRING", CBS_ASN1_BMPSTRING},
      {"PRINTABLESTRING", CBS_ASN1_PRINTABLESTRING},
      {"PRINTABLE", CBS_ASN1_PRINTABLESTRING},
      {"T61", CBS_ASN1_T61STRING},
      {"T61STRING", CBS_ASN1_T61STRING},
      {"TELETEXSTRING", CBS_ASN1_T61STRING},
      {"SEQUENCE", CBS_ASN1_SEQUENCE},
      {"SEQ", CBS_ASN1_SEQUENCE},
      {"SET", CBS_ASN1_SET},
  };
  CBS_ASN1_TAG type = 0;
  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kTypes); i++) {
    if (cbs_str_equal(&name, kTypes[i].name)) {
      type = kTypes[i].type;
      break;
    }
  }
  if (type == 0) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_UNKNOWN_TAG);
    return 0;
  }

  tag = tag == 0 ? type : (tag | (type & CBS_ASN1_CONSTRUCTED));
  CBB child;
  if (!CBB_add_asn1(cbb, &child, tag)) {
    return 0;
  }

  switch (type) {
    case CBS_ASN1_NULL:
      if (*value != '\0') {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_NULL_VALUE);
        return 0;
      }
      return CBB_flush(cbb);

    case CBS_ASN1_BOOLEAN: {
      if (format != ASN1_GEN_FORMAT_ASCII) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_NOT_ASCII_FORMAT);
        return 0;
      }
      ASN1_BOOLEAN boolean;
      if (!X509V3_bool_from_string(value, &boolean)) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_BOOLEAN);
        return 0;
      }
      return CBB_add_u8(&child, boolean ? 0xff : 0x00) && CBB_flush(cbb);
    }

    case CBS_ASN1_INTEGER:
    case CBS_ASN1_ENUMERATED: {
      if (format != ASN1_GEN_FORMAT_ASCII) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_INTEGER_NOT_ASCII_FORMAT);
        return 0;
      }
      ASN1_INTEGER *obj = s2i_ASN1_INTEGER(NULL, value);
      if (obj == NULL) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_INTEGER);
        return 0;
      }
      int len = i2c_ASN1_INTEGER(obj, NULL);
      uint8_t *out;
      int ok = len > 0 &&
               CBB_add_space(&child, &out, len) &&
               i2c_ASN1_INTEGER(obj, &out) == len && CBB_flush(cbb);
      ASN1_INTEGER_free(obj);
      return ok;
    }

    case CBS_ASN1_OBJECT: {
      if (format != ASN1_GEN_FORMAT_ASCII) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_OBJECT_NOT_ASCII_FORMAT);
        return 0;
      }
      ASN1_OBJECT *obj = OBJ_txt2obj(value,                       0);
      if (obj == NULL || obj->length == 0) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_OBJECT);
        return 0;
      }
      int ok = CBB_add_bytes(&child, obj->data, obj->length) && CBB_flush(cbb);
      ASN1_OBJECT_free(obj);
      return ok;
    }

    case CBS_ASN1_UTCTIME:
    case CBS_ASN1_GENERALIZEDTIME: {
      if (format != ASN1_GEN_FORMAT_ASCII) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_TIME_NOT_ASCII_FORMAT);
        return 0;
      }
      CBS value_cbs;
      CBS_init(&value_cbs, (const uint8_t *)value, strlen(value));
      int ok = type == CBS_ASN1_UTCTIME
                   ? CBS_parse_utc_time(&value_cbs, NULL,
                                                                  0)
                   : CBS_parse_generalized_time(&value_cbs, NULL,
                                                                          0);
      if (!ok) {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_TIME_VALUE);
        return 0;
      }
      return CBB_add_bytes(&child, (const uint8_t *)value, strlen(value)) &&
             CBB_flush(cbb);
    }

    case CBS_ASN1_UNIVERSALSTRING:
    case CBS_ASN1_IA5STRING:
    case CBS_ASN1_UTF8STRING:
    case CBS_ASN1_BMPSTRING:
    case CBS_ASN1_PRINTABLESTRING:
    case CBS_ASN1_T61STRING: {
      int encoding;
      if (format == ASN1_GEN_FORMAT_ASCII) {
        encoding = MBSTRING_ASC;
      } else if (format == ASN1_GEN_FORMAT_UTF8) {
        encoding = MBSTRING_UTF8;
      } else {
        OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_FORMAT);
        return 0;
      }

      ASN1_STRING *obj = NULL;
      if (ASN1_mbstring_ncopy(&obj, (const uint8_t *)value, -1, encoding,
                              ASN1_tag2bit(type),             0,
                                          ASN1_GEN_MAX_OUTPUT) <= 0) {
        return 0;
      }
      int ok = CBB_add_bytes(&child, obj->data, obj->length) && CBB_flush(cbb);
      ASN1_STRING_free(obj);
      return ok;
    }

    case CBS_ASN1_BITSTRING:
      if (format == ASN1_GEN_FORMAT_BITLIST) {
        ASN1_BIT_STRING *obj = ASN1_BIT_STRING_new();
        if (obj == NULL) {
          return 0;
        }
        if (!CONF_parse_list(value, ',', 1, bitstr_cb, obj)) {
          OPENSSL_PUT_ERROR(ASN1, ASN1_R_LIST_ERROR);
          ASN1_BIT_STRING_free(obj);
          return 0;
        }
        int len = i2c_ASN1_BIT_STRING(obj, NULL);
        uint8_t *out;
        int ok = len > 0 &&
                 CBB_add_space(&child, &out, len) &&
                 i2c_ASN1_BIT_STRING(obj, &out) == len &&
                 CBB_flush(cbb);
        ASN1_BIT_STRING_free(obj);
        return ok;
      }

      if (!CBB_add_u8(&child, 0)) {
        return 0;
      }
      [[fallthrough]];

    case CBS_ASN1_OCTETSTRING:
      if (format == ASN1_GEN_FORMAT_ASCII) {
        return CBB_add_bytes(&child, (const uint8_t *)value, strlen(value)) &&
               CBB_flush(cbb);
      }
      if (format == ASN1_GEN_FORMAT_HEX) {
        size_t len;
        uint8_t *data = x509v3_hex_to_bytes(value, &len);
        if (data == NULL) {
          OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_HEX);
          return 0;
        }
        int ok = CBB_add_bytes(&child, data, len) && CBB_flush(cbb);
        OPENSSL_free(data);
        return ok;
      }

      OPENSSL_PUT_ERROR(ASN1, ASN1_R_ILLEGAL_BITSTRING_FORMAT);
      return 0;

    case CBS_ASN1_SEQUENCE:
    case CBS_ASN1_SET:
      if (has_value) {
        if (cnf == NULL) {
          OPENSSL_PUT_ERROR(ASN1, ASN1_R_SEQUENCE_OR_SET_NEEDS_CONFIG);
          return 0;
        }
        const STACK_OF(CONF_VALUE) *section = X509V3_get_section(cnf, value);
        if (section == NULL) {
          OPENSSL_PUT_ERROR(ASN1, ASN1_R_SEQUENCE_OR_SET_NEEDS_CONFIG);
          return 0;
        }
        for (size_t i = 0; i < sk_CONF_VALUE_num(section); i++) {
          const CONF_VALUE *conf = sk_CONF_VALUE_value(section, i);
          if (!generate_v3(&child, conf->value, cnf,         0,
                           ASN1_GEN_FORMAT_ASCII, depth + 1)) {
            return 0;
          }

          if (CBB_len(&child) > ASN1_GEN_MAX_OUTPUT) {
            OPENSSL_PUT_ERROR(ASN1, ASN1_R_TOO_LONG);
            return 0;
          }
        }
      }
      if (type == CBS_ASN1_SET) {

        return CBB_flush_asn1_set_of(&child) && CBB_flush(cbb);
      }
      return CBB_flush(cbb);

    default:
      OPENSSL_PUT_ERROR(ASN1, ERR_R_INTERNAL_ERROR);
      return 0;
  }
}

static int bitstr_cb(const char *elem, size_t len, void *bitstr) {
  CBS cbs;
  CBS_init(&cbs, (const uint8_t *)elem, len);
  uint64_t bitnum;
  if (!CBS_get_u64_decimal(&cbs, &bitnum) || CBS_len(&cbs) != 0 ||

      bitnum > 256) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_INVALID_NUMBER);
    return 0;
  }
  if (!ASN1_BIT_STRING_set_bit(reinterpret_cast<ASN1_BIT_STRING *>(bitstr),
                               (int)bitnum, 1)) {
    return 0;
  }
  return 1;
}
