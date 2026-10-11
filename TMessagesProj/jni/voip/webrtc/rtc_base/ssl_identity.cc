/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Handling of certificates and keypairs for SSLStreamAdapter's peer mode.
#include "rtc_base/ssl_identity.h"

#include <openssl/ossl_typ.h>
#include <string.h>
#include <time.h>

#include "absl/strings/string_view.h"
#include "rtc_base/checks.h"
#ifdef OPENSSL_IS_BORINGSSL
#include "rtc_base/boringssl_identity.h"
#else
#include "rtc_base/openssl_identity.h"
#endif
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/third_party/base64/base64.h"
#include "rtc_base/time_utils.h"

namespace rtc {

namespace {

inline int ASN1ReadInt(const unsigned char** pp, size_t* np, size_t n) {
  const unsigned char* p = *pp;
  int x = 0;
  for (size_t i = 0; i < n; i++) {
    x = 10 * x + p[i] - '0';
  }
  *pp = p + n;
  *np = *np - n;
  return x;
}

}

int64_t ASN1TimeToSec(const unsigned char* s, size_t length, bool long_format) {
  size_t bytes_left = length;

  if (length == 0 || s[length - 1] != 'Z') {
    return -1;
  }

  size_t n = strspn(reinterpret_cast<const char*>(s), "0123456789");
  if (n + 1 != length) {
    return -1;
  }

  int year = 0;
  if (long_format) {

    if (bytes_left < 11) {
      return -1;
    }
    year = ASN1ReadInt(&s, &bytes_left, 4);
    year -= 1900;
  } else {

    if (bytes_left < 9) {
      return -1;
    }
    year = ASN1ReadInt(&s, &bytes_left, 2);

    if (year < 50) {
      year += 100;
    }
  }

  tm tm;
  tm.tm_year = year;
  tm.tm_mon = ASN1ReadInt(&s, &bytes_left, 2) - 1;
  tm.tm_mday = ASN1ReadInt(&s, &bytes_left, 2);
  tm.tm_hour = ASN1ReadInt(&s, &bytes_left, 2);
  tm.tm_min = ASN1ReadInt(&s, &bytes_left, 2);
  tm.tm_sec = ASN1ReadInt(&s, &bytes_left, 2);

  if (bytes_left != 1) {
    return -1;
  }
  return TmToSeconds(tm);
}

const char kPemTypeCertificate[] = "CERTIFICATE";
const char kPemTypeRsaPrivateKey[] = "RSA PRIVATE KEY";
const char kPemTypeEcPrivateKey[] = "EC PRIVATE KEY";

KeyParams::KeyParams(KeyType key_type) {
  if (key_type == KT_ECDSA) {
    type_ = KT_ECDSA;
    params_.curve = EC_NIST_P256;
  } else if (key_type == KT_RSA) {
    type_ = KT_RSA;
    params_.rsa.mod_size = kRsaDefaultModSize;
    params_.rsa.pub_exp = kRsaDefaultExponent;
  } else {
    RTC_DCHECK_NOTREACHED();
  }
}

KeyParams KeyParams::RSA(int mod_size, int pub_exp) {
  KeyParams kt(KT_RSA);
  kt.params_.rsa.mod_size = mod_size;
  kt.params_.rsa.pub_exp = pub_exp;
  return kt;
}

KeyParams KeyParams::ECDSA(ECCurve curve) {
  KeyParams kt(KT_ECDSA);
  kt.params_.curve = curve;
  return kt;
}

bool KeyParams::IsValid() const {
  if (type_ == KT_RSA) {
    return (params_.rsa.mod_size >= kRsaMinModSize &&
            params_.rsa.mod_size <= kRsaMaxModSize &&
            params_.rsa.pub_exp > params_.rsa.mod_size);
  } else if (type_ == KT_ECDSA) {
    return (params_.curve == EC_NIST_P256);
  }
  return false;
}

RSAParams KeyParams::rsa_params() const {
  RTC_DCHECK(type_ == KT_RSA);
  return params_.rsa;
}

ECCurve KeyParams::ec_curve() const {
  RTC_DCHECK(type_ == KT_ECDSA);
  return params_.curve;
}

KeyType IntKeyTypeFamilyToKeyType(int key_type_family) {
  return static_cast<KeyType>(key_type_family);
}

bool SSLIdentity::PemToDer(absl::string_view pem_type,
                           absl::string_view pem_string,
                           std::string* der) {

  std::string pem_type_str(pem_type);
  size_t header = pem_string.find("-----BEGIN " + pem_type_str + "-----");
  if (header == absl::string_view::npos) {
    return false;
  }
  size_t body = pem_string.find('\n', header);
  if (body == absl::string_view::npos) {
    return false;
  }
  size_t trailer = pem_string.find("-----END " + pem_type_str + "-----");
  if (trailer == absl::string_view::npos) {
    return false;
  }
  std::string inner(pem_string.substr(body + 1, trailer - (body + 1)));
  *der = Base64::Decode(inner, Base64::DO_PARSE_WHITE | Base64::DO_PAD_ANY |
                                   Base64::DO_TERM_BUFFER);
  return true;
}

std::string SSLIdentity::DerToPem(absl::string_view pem_type,
                                  const unsigned char* data,
                                  size_t length) {
  rtc::StringBuilder result;
  result << "-----BEGIN " << pem_type << "-----\n";

  std::string b64_encoded;
  Base64::EncodeFromArray(data, length, &b64_encoded);

  static const size_t kChunkSize = 64;
  size_t chunks = (b64_encoded.size() + (kChunkSize - 1)) / kChunkSize;
  for (size_t i = 0, chunk_offset = 0; i < chunks;
       ++i, chunk_offset += kChunkSize) {
    result << b64_encoded.substr(chunk_offset, kChunkSize);
    result << "\n";
  }
  result << "-----END " << pem_type << "-----\n";
  return result.Release();
}

std::unique_ptr<SSLIdentity> SSLIdentity::Create(absl::string_view common_name,
                                                 const KeyParams& key_param,
                                                 time_t certificate_lifetime) {
#ifdef OPENSSL_IS_BORINGSSL
  return BoringSSLIdentity::CreateWithExpiration(common_name, key_param,
                                                 certificate_lifetime);
#else
  return OpenSSLIdentity::CreateWithExpiration(common_name, key_param,
                                               certificate_lifetime);
#endif
}

std::unique_ptr<SSLIdentity> SSLIdentity::Create(absl::string_view common_name,
                                                 const KeyParams& key_param) {
  return Create(common_name, key_param, kDefaultCertificateLifetimeInSeconds);
}

std::unique_ptr<SSLIdentity> SSLIdentity::Create(absl::string_view common_name,
                                                 KeyType key_type) {
  return Create(common_name, KeyParams(key_type),
                kDefaultCertificateLifetimeInSeconds);
}

std::unique_ptr<SSLIdentity> SSLIdentity::CreateForTest(
    const SSLIdentityParams& params) {
#ifdef OPENSSL_IS_BORINGSSL
  return BoringSSLIdentity::CreateForTest(params);
#else
  return OpenSSLIdentity::CreateForTest(params);
#endif
}

std::unique_ptr<SSLIdentity> SSLIdentity::CreateFromPEMStrings(
    absl::string_view private_key,
    absl::string_view certificate) {
#ifdef OPENSSL_IS_BORINGSSL
  return BoringSSLIdentity::CreateFromPEMStrings(private_key, certificate);
#else
  return OpenSSLIdentity::CreateFromPEMStrings(private_key, certificate);
#endif
}

std::unique_ptr<SSLIdentity> SSLIdentity::CreateFromPEMChainStrings(
    absl::string_view private_key,
    absl::string_view certificate_chain) {
#ifdef OPENSSL_IS_BORINGSSL
  return BoringSSLIdentity::CreateFromPEMChainStrings(private_key,
                                                      certificate_chain);
#else
  return OpenSSLIdentity::CreateFromPEMChainStrings(private_key,
                                                    certificate_chain);
#endif
}

bool operator==(const SSLIdentity& a, const SSLIdentity& b) {
#ifdef OPENSSL_IS_BORINGSSL
  return static_cast<const BoringSSLIdentity&>(a) ==
         static_cast<const BoringSSLIdentity&>(b);
#else
  return static_cast<const OpenSSLIdentity&>(a) ==
         static_cast<const OpenSSLIdentity&>(b);
#endif
}
bool operator!=(const SSLIdentity& a, const SSLIdentity& b) {
  return !(a == b);
}

}
