// Copyright 2001-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_BASE_H
#define OPENSSL_HEADER_BASE_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#if defined(__MINGW32__)

#include <stdio.h>
#endif

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include <openssl/is_boringssl.h>
#include <openssl/opensslconf.h>
#include <openssl/target.h>

#if defined(BORINGSSL_PREFIX)
#include <boringssl_prefix_symbols.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__APPLE__)

#if defined(TARGET_OS_OSX) && TARGET_OS_OSX
#define OPENSSL_MACOS
#endif
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define OPENSSL_IOS
#endif
#endif

#define OPENSSL_IS_BORINGSSL
#define OPENSSL_VERSION_NUMBER 0x1010107f
#define SSLEAY_VERSION_NUMBER OPENSSL_VERSION_NUMBER

#define BORINGSSL_API_VERSION 35

#if defined(BORINGSSL_SHARED_LIBRARY)

#if defined(OPENSSL_WINDOWS)

#if defined(BORINGSSL_IMPLEMENTATION)
#define OPENSSL_EXPORT __declspec(dllexport)
#else
#define OPENSSL_EXPORT __declspec(dllimport)
#endif

#else

#if defined(BORINGSSL_IMPLEMENTATION)
#define OPENSSL_EXPORT __attribute__((visibility("default")))
#else
#define OPENSSL_EXPORT
#endif

#endif

#else

#define OPENSSL_EXPORT

#endif

#if defined(_MSC_VER)

#define OPENSSL_DEPRECATED __declspec(deprecated)
#define OPENSSL_BEGIN_ALLOW_DEPRECATED \
  __pragma(warning(push)) __pragma(warning(disable : 4996))
#define OPENSSL_END_ALLOW_DEPRECATED __pragma(warning(pop))

#elif defined(__GNUC__) || defined(__clang__)

#define OPENSSL_DEPRECATED __attribute__((__deprecated__))
#define OPENSSL_BEGIN_ALLOW_DEPRECATED \
  _Pragma("GCC diagnostic push")       \
      _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#define OPENSSL_END_ALLOW_DEPRECATED _Pragma("GCC diagnostic pop")

#else

#define OPENSSL_DEPRECATED
#define OPENSSL_BEGIN_ALLOW_DEPRECATED
#define OPENSSL_END_ALLOW_DEPRECATED

#endif

#if defined(__GNUC__) || defined(__clang__)

#if defined(__MINGW_PRINTF_FORMAT)
#define OPENSSL_PRINTF_FORMAT_FUNC(string_index, first_to_check) \
  __attribute__((                                                \
      __format__(__MINGW_PRINTF_FORMAT, string_index, first_to_check)))
#else
#define OPENSSL_PRINTF_FORMAT_FUNC(string_index, first_to_check) \
  __attribute__((__format__(__printf__, string_index, first_to_check)))
#endif
#else
#define OPENSSL_PRINTF_FORMAT_FUNC(string_index, first_to_check)
#endif

#if defined(__clang__)
#define OPENSSL_CLANG_PRAGMA(arg) _Pragma(arg)
#else
#define OPENSSL_CLANG_PRAGMA(arg)
#endif

#if defined(_MSC_VER)
#define OPENSSL_MSVC_PRAGMA(arg) __pragma(arg)
#else
#define OPENSSL_MSVC_PRAGMA(arg)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define OPENSSL_UNUSED __attribute__((unused))
#elif defined(_MSC_VER)

#define OPENSSL_UNUSED __pragma(warning(suppress : 4505)) \
     

#else
#define OPENSSL_UNUSED
#endif

#if defined(__cplusplus)
#define OPENSSL_INLINE inline
#else

#define OPENSSL_INLINE static inline OPENSSL_UNUSED
#endif

#if defined(__cplusplus)

#define BORINGSSL_ENUM_INT : int
enum ssl_early_data_reason_t BORINGSSL_ENUM_INT;
enum ssl_encryption_level_t BORINGSSL_ENUM_INT;
enum ssl_private_key_result_t BORINGSSL_ENUM_INT;
enum ssl_renegotiate_mode_t BORINGSSL_ENUM_INT;
enum ssl_select_cert_result_t BORINGSSL_ENUM_INT;
enum ssl_select_cert_result_t BORINGSSL_ENUM_INT;
enum ssl_ticket_aead_result_t BORINGSSL_ENUM_INT;
enum ssl_verify_result_t BORINGSSL_ENUM_INT;
#else
#define BORINGSSL_ENUM_INT
#endif

typedef ptrdiff_t ossl_ssize_t;

typedef uint32_t CBS_ASN1_TAG;

typedef int CRYPTO_THREADID;

typedef struct asn1_null_st ASN1_NULL;

typedef int ASN1_BOOLEAN;
typedef struct ASN1_ITEM_st ASN1_ITEM;
typedef struct asn1_object_st ASN1_OBJECT;
typedef struct asn1_pctx_st ASN1_PCTX;
typedef struct asn1_string_st ASN1_BIT_STRING;
typedef struct asn1_string_st ASN1_BMPSTRING;
typedef struct asn1_string_st ASN1_ENUMERATED;
typedef struct asn1_string_st ASN1_GENERALIZEDTIME;
typedef struct asn1_string_st ASN1_GENERALSTRING;
typedef struct asn1_string_st ASN1_IA5STRING;
typedef struct asn1_string_st ASN1_INTEGER;
typedef struct asn1_string_st ASN1_OCTET_STRING;
typedef struct asn1_string_st ASN1_PRINTABLESTRING;
typedef struct asn1_string_st ASN1_STRING;
typedef struct asn1_string_st ASN1_T61STRING;
typedef struct asn1_string_st ASN1_TIME;
typedef struct asn1_string_st ASN1_UNIVERSALSTRING;
typedef struct asn1_string_st ASN1_UTCTIME;
typedef struct asn1_string_st ASN1_UTF8STRING;
typedef struct asn1_string_st ASN1_VISIBLESTRING;
typedef struct asn1_type_st ASN1_TYPE;
typedef struct AUTHORITY_KEYID_st AUTHORITY_KEYID;
typedef struct BASIC_CONSTRAINTS_st BASIC_CONSTRAINTS;
typedef struct DIST_POINT_st DIST_POINT;
typedef struct DSA_SIG_st DSA_SIG;
typedef struct GENERAL_NAME_st GENERAL_NAME;
typedef struct ISSUING_DIST_POINT_st ISSUING_DIST_POINT;
typedef struct NAME_CONSTRAINTS_st NAME_CONSTRAINTS;
typedef struct Netscape_spkac_st NETSCAPE_SPKAC;
typedef struct Netscape_spki_st NETSCAPE_SPKI;
typedef struct RIPEMD160state_st RIPEMD160_CTX;
typedef struct X509_VERIFY_PARAM_st X509_VERIFY_PARAM;
typedef struct X509_algor_st X509_ALGOR;
typedef struct X509_crl_st X509_CRL;
typedef struct X509_extension_st X509_EXTENSION;
typedef struct X509_info_st X509_INFO;
typedef struct X509_name_entry_st X509_NAME_ENTRY;
typedef struct X509_name_st X509_NAME;
typedef struct X509_pubkey_st X509_PUBKEY;
typedef struct X509_req_st X509_REQ;
typedef struct X509_sig_st X509_SIG;
typedef struct bignum_ctx BN_CTX;
typedef struct bignum_st BIGNUM;
typedef struct bio_method_st BIO_METHOD;
typedef struct bio_st BIO;
typedef struct blake2b_state_st BLAKE2B_CTX;
typedef struct bn_gencb_st BN_GENCB;
typedef struct bn_mont_ctx_st BN_MONT_CTX;
typedef struct buf_mem_st BUF_MEM;
typedef struct cbb_st CBB;
typedef struct cbs_st CBS;
typedef struct cmac_ctx_st CMAC_CTX;
typedef struct conf_st CONF;
typedef struct conf_value_st CONF_VALUE;
typedef struct crypto_buffer_pool_st CRYPTO_BUFFER_POOL;
typedef struct crypto_buffer_st CRYPTO_BUFFER;
typedef struct ctr_drbg_state_st CTR_DRBG_STATE;
typedef struct dh_st DH;
typedef struct dsa_st DSA;
typedef struct ec_group_st EC_GROUP;
typedef struct ec_key_st EC_KEY;
typedef struct ec_point_st EC_POINT;
typedef struct ecdsa_method_st ECDSA_METHOD;
typedef struct ecdsa_sig_st ECDSA_SIG;
typedef struct engine_st ENGINE;
typedef struct env_md_ctx_st EVP_MD_CTX;
typedef struct env_md_st EVP_MD;
typedef struct evp_aead_st EVP_AEAD;
typedef struct evp_aead_ctx_st EVP_AEAD_CTX;
typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;
typedef struct evp_cipher_st EVP_CIPHER;
typedef struct evp_encode_ctx_st EVP_ENCODE_CTX;
typedef struct evp_hpke_aead_st EVP_HPKE_AEAD;
typedef struct evp_hpke_ctx_st EVP_HPKE_CTX;
typedef struct evp_hpke_kdf_st EVP_HPKE_KDF;
typedef struct evp_hpke_kem_st EVP_HPKE_KEM;
typedef struct evp_hpke_key_st EVP_HPKE_KEY;
typedef struct evp_pkey_ctx_st EVP_PKEY_CTX;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct hmac_ctx_st HMAC_CTX;
typedef struct md4_state_st MD4_CTX;
typedef struct md5_state_st MD5_CTX;
typedef struct ossl_init_settings_st OPENSSL_INIT_SETTINGS;
typedef struct pkcs12_st PKCS12;
typedef struct pkcs8_priv_key_info_st PKCS8_PRIV_KEY_INFO;
typedef struct private_key_st X509_PKEY;
typedef struct rand_meth_st RAND_METHOD;
typedef struct rc4_key_st RC4_KEY;
typedef struct rsa_meth_st RSA_METHOD;
typedef struct rsa_pss_params_st RSA_PSS_PARAMS;
typedef struct rsa_st RSA;
typedef struct sha256_state_st SHA256_CTX;
typedef struct sha512_state_st SHA512_CTX;
typedef struct sha_state_st SHA_CTX;
typedef struct spake2_ctx_st SPAKE2_CTX;
typedef struct srtp_protection_profile_st SRTP_PROTECTION_PROFILE;
typedef struct ssl_cipher_st SSL_CIPHER;
typedef struct ssl_credential_st SSL_CREDENTIAL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_early_callback_ctx SSL_CLIENT_HELLO;
typedef struct ssl_ech_keys_st SSL_ECH_KEYS;
typedef struct ssl_method_st SSL_METHOD;
typedef struct ssl_private_key_method_st SSL_PRIVATE_KEY_METHOD;
typedef struct ssl_quic_method_st SSL_QUIC_METHOD;
typedef struct ssl_session_st SSL_SESSION;
typedef struct ssl_st SSL;
typedef struct ssl_ticket_aead_method_st SSL_TICKET_AEAD_METHOD;
typedef struct st_ERR_FNS ERR_FNS;
typedef struct trust_token_st TRUST_TOKEN;
typedef struct trust_token_client_st TRUST_TOKEN_CLIENT;
typedef struct trust_token_issuer_st TRUST_TOKEN_ISSUER;
typedef struct trust_token_method_st TRUST_TOKEN_METHOD;
typedef struct v3_ext_ctx X509V3_CTX;
typedef struct v3_ext_method X509V3_EXT_METHOD;
typedef struct x509_attributes_st X509_ATTRIBUTE;
typedef struct x509_lookup_st X509_LOOKUP;
typedef struct x509_lookup_method_st X509_LOOKUP_METHOD;
typedef struct x509_object_st X509_OBJECT;
typedef struct x509_purpose_st X509_PURPOSE;
typedef struct x509_revoked_st X509_REVOKED;
typedef struct x509_st X509;
typedef struct x509_store_ctx_st X509_STORE_CTX;
typedef struct x509_store_st X509_STORE;

typedef void *OPENSSL_BLOCK;

#define BSSL_CHECK(condition) \
  do {                        \
    if (!(condition)) {       \
      abort();                \
    }                         \
  } while (0);

#if defined(__cplusplus)
}
#elif !defined(BORINGSSL_NO_CXX)
#define BORINGSSL_NO_CXX
#endif

#if defined(BORINGSSL_PREFIX)
#define BSSL_NAMESPACE_BEGIN \
  namespace bssl {           \
  inline namespace BORINGSSL_PREFIX {
#define BSSL_NAMESPACE_END \
  }                        \
  }
#else
#define BSSL_NAMESPACE_BEGIN namespace bssl {
#define BSSL_NAMESPACE_END }
#endif

#if !defined(BORINGSSL_NO_CXX) && __cplusplus < 201103L && !defined(_MSC_VER)
#define BORINGSSL_NO_CXX
#endif

#if !defined(BORINGSSL_NO_CXX)

extern "C++" {

#include <memory>

#if defined(_STLPORT_VERSION)
#define BORINGSSL_NO_CXX
#endif

}
#endif

#if defined(BORINGSSL_NO_CXX)

#define BORINGSSL_MAKE_DELETER(type, deleter)
#define BORINGSSL_MAKE_UP_REF(type, up_ref_func)

#else

extern "C++" {

BSSL_NAMESPACE_BEGIN

namespace internal {

template <typename T, typename Enable = void>
struct DeleterImpl {};

struct Deleter {
  template <typename T>
  void operator()(T *ptr) {

    DeleterImpl<T>::Free(ptr);
  }
};

template <typename T, typename CleanupRet, void (*init)(T *),
          CleanupRet (*cleanup)(T *)>
class StackAllocated {
 public:
  StackAllocated() { init(&ctx_); }
  ~StackAllocated() { cleanup(&ctx_); }

  StackAllocated(const StackAllocated &) = delete;
  StackAllocated &operator=(const StackAllocated &) = delete;

  T *get() { return &ctx_; }
  const T *get() const { return &ctx_; }

  T *operator->() { return &ctx_; }
  const T *operator->() const { return &ctx_; }

  void Reset() {
    cleanup(&ctx_);
    init(&ctx_);
  }

 private:
  T ctx_;
};

template <typename T, typename CleanupRet, void (*init)(T *),
          CleanupRet (*cleanup)(T *), void (*move)(T *, T *)>
class StackAllocatedMovable {
 public:
  StackAllocatedMovable() { init(&ctx_); }
  ~StackAllocatedMovable() { cleanup(&ctx_); }

  StackAllocatedMovable(StackAllocatedMovable &&other) {
    init(&ctx_);
    move(&ctx_, &other.ctx_);
  }
  StackAllocatedMovable &operator=(StackAllocatedMovable &&other) {
    move(&ctx_, &other.ctx_);
    return *this;
  }

  T *get() { return &ctx_; }
  const T *get() const { return &ctx_; }

  T *operator->() { return &ctx_; }
  const T *operator->() const { return &ctx_; }

  void Reset() {
    cleanup(&ctx_);
    init(&ctx_);
  }

 private:
  T ctx_;
};

}

#define BORINGSSL_MAKE_DELETER(type, deleter)     \
  namespace internal {                            \
  template <>                                     \
  struct DeleterImpl<type> {                      \
    static void Free(type *ptr) { deleter(ptr); } \
  };                                              \
  }

template <typename T>
using UniquePtr = std::unique_ptr<T, internal::Deleter>;

#define BORINGSSL_MAKE_UP_REF(type, up_ref_func)             \
  inline UniquePtr<type> UpRef(type *v) {                    \
    if (v != nullptr) {                                      \
      up_ref_func(v);                                        \
    }                                                        \
    return UniquePtr<type>(v);                               \
  }                                                          \
                                                             \
  inline UniquePtr<type> UpRef(const UniquePtr<type> &ptr) { \
    return UpRef(ptr.get());                                 \
  }

BSSL_NAMESPACE_END

}

#endif

#define RSA_PKCS1_PADDING_SIZE 11

#endif
