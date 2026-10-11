// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
// Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved.
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

#ifndef OPENSSL_HEADER_SSL_INTERNAL_H
#define OPENSSL_HEADER_SSL_INTERNAL_H

#include <openssl/base.h>

#include <stdlib.h>

#include <algorithm>
#include <atomic>
#include <bitset>
#include <initializer_list>
#include <limits>
#include <new>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include <openssl/aead.h>
#include <openssl/curve25519.h>
#include <openssl/err.h>
#include <openssl/hpke.h>
#include <openssl/lhash.h>
#include <openssl/mem.h>
#include <openssl/span.h>
#include <openssl/ssl.h>
#include <openssl/stack.h>

#include "../crypto/err/internal.h"
#include "../crypto/internal.h"
#include "../crypto/lhash/internal.h"
#include "../crypto/spake2plus/internal.h"

#if defined(OPENSSL_WINDOWS)

#include <winsock2.h>
#else
#include <sys/time.h>
#endif

BSSL_NAMESPACE_BEGIN

struct SSL_CONFIG;
struct SSL_HANDSHAKE;
struct SSL_PROTOCOL_METHOD;
struct SSL_X509_METHOD;

template <typename T, typename... Args>
T *New(Args &&...args) {
  void *t = OPENSSL_malloc(sizeof(T));
  if (t == nullptr) {
    return nullptr;
  }
  return new (t) T(std::forward<Args>(args)...);
}

template <typename T>
void Delete(T *t) {
  if (t != nullptr) {
    t->~T();
    OPENSSL_free(t);
  }
}

namespace internal {
template <typename T>
struct DeleterImpl<T, std::enable_if_t<T::kAllowUniquePtr>> {
  static void Free(T *t) { Delete(t); }
};
}

template <typename T, typename... Args>
UniquePtr<T> MakeUnique(Args &&...args) {
  return UniquePtr<T>(New<T>(std::forward<Args>(args)...));
}

template <typename T>
class Array {
 public:

  Array() {}
  Array(const Array &) = delete;
  Array(Array &&other) { *this = std::move(other); }

  ~Array() { Reset(); }

  Array &operator=(const Array &) = delete;
  Array &operator=(Array &&other) {
    Reset();
    other.Release(&data_, &size_);
    return *this;
  }

  const T *data() const { return data_; }
  T *data() { return data_; }
  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  const T &operator[](size_t i) const {
    BSSL_CHECK(i < size_);
    return data_[i];
  }
  T &operator[](size_t i) {
    BSSL_CHECK(i < size_);
    return data_[i];
  }

  T *begin() { return data_; }
  const T *begin() const { return data_; }
  T *end() { return data_ + size_; }
  const T *end() const { return data_ + size_; }

  void Reset() { Reset(nullptr, 0); }

  void Reset(T *new_data, size_t new_size) {
    std::destroy_n(data_, size_);
    OPENSSL_free(data_);
    data_ = new_data;
    size_ = new_size;
  }

  void Release(T **out, size_t *out_size) {
    *out = data_;
    *out_size = size_;
    data_ = nullptr;
    size_ = 0;
  }

  [[nodiscard]] bool Init(size_t new_size) {
    if (!InitUninitialized(new_size)) {
      return false;
    }
    std::uninitialized_value_construct_n(data_, size_);
    return true;
  }

  [[nodiscard]] bool InitForOverwrite(size_t new_size) {
    if (!InitUninitialized(new_size)) {
      return false;
    }
    std::uninitialized_default_construct_n(data_, size_);
    return true;
  }

  [[nodiscard]] bool CopyFrom(Span<const T> in) {
    if (!InitUninitialized(in.size())) {
      return false;
    }
    std::uninitialized_copy(in.begin(), in.end(), data_);
    return true;
  }

  void Shrink(size_t new_size) {
    if (new_size > size_) {
      abort();
    }
    std::destroy_n(data_ + new_size, size_ - new_size);
    size_ = new_size;
  }

 private:

  bool InitUninitialized(size_t new_size) {
    Reset();
    if (new_size == 0) {
      return true;
    }

    if (new_size > std::numeric_limits<size_t>::max() / sizeof(T)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
      return false;
    }
    data_ = reinterpret_cast<T *>(OPENSSL_malloc(new_size * sizeof(T)));
    if (data_ == nullptr) {
      return false;
    }
    size_ = new_size;
    return true;
  }

  T *data_ = nullptr;
  size_t size_ = 0;
};

template <typename T>
class Vector {
 public:
  Vector() = default;
  Vector(const Vector &) = delete;
  Vector(Vector &&other) { *this = std::move(other); }
  ~Vector() { clear(); }

  Vector &operator=(const Vector &) = delete;
  Vector &operator=(Vector &&other) {
    clear();
    std::swap(data_, other.data_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
    return *this;
  }

  const T *data() const { return data_; }
  T *data() { return data_; }
  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  const T &operator[](size_t i) const {
    BSSL_CHECK(i < size_);
    return data_[i];
  }
  T &operator[](size_t i) {
    BSSL_CHECK(i < size_);
    return data_[i];
  }

  T *begin() { return data_; }
  const T *begin() const { return data_; }
  T *end() { return data_ + size_; }
  const T *end() const { return data_ + size_; }

  void clear() {
    std::destroy_n(data_, size_);
    OPENSSL_free(data_);
    data_ = nullptr;
    size_ = 0;
    capacity_ = 0;
  }

  [[nodiscard]] bool Push(T elem) {
    if (!MaybeGrow()) {
      return false;
    }
    new (&data_[size_]) T(std::move(elem));
    size_++;
    return true;
  }

  [[nodiscard]] bool CopyFrom(Span<const T> in) {
    Array<T> copy;
    if (!copy.CopyFrom(in)) {
      return false;
    }

    clear();
    copy.Release(&data_, &size_);
    capacity_ = size_;
    return true;
  }

 private:

  bool MaybeGrow() {

    if (size_ < capacity_) {
      return true;
    }
    size_t new_capacity = kDefaultSize;
    if (capacity_ > 0) {

      if (capacity_ > std::numeric_limits<size_t>::max() / 2) {
        OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
        return false;
      }
      new_capacity = capacity_ * 2;
    }
    if (new_capacity > std::numeric_limits<size_t>::max() / sizeof(T)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
      return false;
    }
    T *new_data =
        reinterpret_cast<T *>(OPENSSL_malloc(new_capacity * sizeof(T)));
    if (new_data == nullptr) {
      return false;
    }
    size_t new_size = size_;
    std::uninitialized_move(begin(), end(), new_data);
    clear();
    data_ = new_data;
    size_ = new_size;
    capacity_ = new_capacity;
    return true;
  }

  T *data_ = nullptr;

  size_t size_ = 0;

  size_t capacity_ = 0;

  static constexpr size_t kDefaultSize = 16;
};

template <size_t N>
using PackedSize = std::conditional_t<
    N <= 0xff, uint8_t,
    std::conditional_t<N <= 0xffff, uint16_t,
                       std::conditional_t<N <= 0xffffffff, uint32_t, size_t>>>;

template <typename T, size_t N>
class InplaceVector {
 public:
  InplaceVector() = default;
  InplaceVector(const InplaceVector &other) { *this = other; }
  InplaceVector(InplaceVector &&other) { *this = std::move(other); }
  ~InplaceVector() { clear(); }
  InplaceVector &operator=(const InplaceVector &other) {
    if (this != &other) {
      CopyFrom(other);
    }
    return *this;
  }
  InplaceVector &operator=(InplaceVector &&other) {
    clear();
    std::uninitialized_move(other.begin(), other.end(), data());
    size_ = other.size();
    return *this;
  }

  const T *data() const { return reinterpret_cast<const T *>(storage_); }
  T *data() { return reinterpret_cast<T *>(storage_); }
  size_t size() const { return size_; }
  static constexpr size_t capacity() { return N; }
  bool empty() const { return size_ == 0; }

  const T &operator[](size_t i) const {
    BSSL_CHECK(i < size_);
    return data()[i];
  }
  T &operator[](size_t i) {
    BSSL_CHECK(i < size_);
    return data()[i];
  }

  T *begin() { return data(); }
  const T *begin() const { return data(); }
  T *end() { return data() + size_; }
  const T *end() const { return data() + size_; }

  void clear() { Shrink(0); }

  void Shrink(size_t new_size) {
    BSSL_CHECK(new_size <= size_);
    std::destroy_n(data() + new_size, size_ - new_size);
    size_ = static_cast<PackedSize<N>>(new_size);
  }

  [[nodiscard]] bool TryResize(size_t new_size) {
    if (new_size <= size_) {
      Shrink(new_size);
      return true;
    }
    if (new_size > capacity()) {
      return false;
    }
    std::uninitialized_value_construct_n(data() + size_, new_size - size_);
    size_ = static_cast<PackedSize<N>>(new_size);
    return true;
  }

  [[nodiscard]] bool TryResizeForOverwrite(size_t new_size) {
    if (new_size <= size_) {
      Shrink(new_size);
      return true;
    }
    if (new_size > capacity()) {
      return false;
    }
    std::uninitialized_default_construct_n(data() + size_, new_size - size_);
    size_ = static_cast<PackedSize<N>>(new_size);
    return true;
  }

  [[nodiscard]] bool TryCopyFrom(Span<const T> in) {
    if (in.size() > capacity()) {
      return false;
    }
    clear();
    std::uninitialized_copy(in.begin(), in.end(), data());
    size_ = in.size();
    return true;
  }

  [[nodiscard]] T *TryPushBack(T val) {
    if (size() >= capacity()) {
      return nullptr;
    }
    T *ret = &data()[size_];
    new (ret) T(std::move(val));
    size_++;
    return ret;
  }

  void Resize(size_t size) { BSSL_CHECK(TryResize(size)); }
  void ResizeForOverwrite(size_t size) {
    BSSL_CHECK(TryResizeForOverwrite(size));
  }
  void CopyFrom(Span<const T> in) { BSSL_CHECK(TryCopyFrom(in)); }
  T &PushBack(T val) {
    T *ret = TryPushBack(std::move(val));
    BSSL_CHECK(ret != nullptr);
    return *ret;
  }

  template <typename Pred>
  void EraseIf(Pred pred) {

    auto iter = std::find_if(begin(), end(), pred);
    if (iter == end()) {
      return;
    }

    size_t new_size = iter - begin();

    for (size_t i = new_size + 1; i < size(); i++) {
      if (!pred((*this)[i])) {
        (*this)[new_size] = std::move((*this)[i]);
        new_size++;
      }
    }

    Shrink(new_size);
  }

 private:
  alignas(T) char storage_[sizeof(T[N])];
  PackedSize<N> size_ = 0;
};

template <typename T, size_t N>
class MRUQueue {
 public:
  static constexpr bool kAllowUniquePtr = true;

  MRUQueue() = default;

  MRUQueue(const MRUQueue &other) = delete;
  MRUQueue &operator=(const MRUQueue &other) = delete;

  bool empty() const { return size() == 0; }
  size_t size() const { return storage_.size(); }

  T &operator[](size_t i) {
    BSSL_CHECK(i < size());
    return storage_[(start_ + i) % N];
  }
  const T &operator[](size_t i) const {
    return (*const_cast<MRUQueue *>(this))[i];
  }

  void Clear() {
    storage_.clear();
    start_ = 0;
  }

  void PushBack(T t) {
    if (storage_.size() < N) {
      assert(start_ == 0);
      storage_.PushBack(std::move(t));
    } else {
      (*this)[0] = std::move(t);
      start_ = (start_ + 1) % N;
    }
  }

 private:
  InplaceVector<T, N> storage_;
  PackedSize<N> start_ = 0;
};

OPENSSL_EXPORT bool CBBFinishArray(CBB *cbb, Array<uint8_t> *out);

template <typename T, typename Name>
inline size_t GetAllNames(const char **out, size_t max_out,
                          Span<const char *const> fixed_names, Name(T::*name),
                          Span<const T> objects) {
  auto span = bssl::Span(out, max_out);
  for (size_t i = 0; !span.empty() && i < fixed_names.size(); i++) {
    span[0] = fixed_names[i];
    span = span.subspan(1);
  }
  span = span.subspan(0, objects.size());
  for (size_t i = 0; i < span.size(); i++) {
    span[i] = objects[i].*name;
  }
  return fixed_names.size() + objects.size();
}

template <typename Derived>
class RefCounted {
 public:
  RefCounted(const RefCounted &) = delete;
  RefCounted &operator=(const RefCounted &) = delete;

  void UpRefInternal() { CRYPTO_refcount_inc(&references_); }
  void DecRefInternal() {
    if (CRYPTO_refcount_dec_and_test_zero(&references_)) {
      Derived *d = static_cast<Derived *>(this);
      d->~Derived();
      OPENSSL_free(d);
    }
  }

 protected:

  class CheckSubClass {
   private:
    friend Derived;
    CheckSubClass() = default;
  };
  RefCounted(CheckSubClass) {
    static_assert(std::is_base_of<RefCounted, Derived>::value,
                  "Derived must subclass RefCounted<Derived>");
  }

  ~RefCounted() = default;

 private:
  CRYPTO_refcount_t references_ = 1;
};

bool ssl_protocol_version_from_wire(uint16_t *out, uint16_t version);

bool ssl_get_version_range(const SSL_HANDSHAKE *hs, uint16_t *out_min_version,
                           uint16_t *out_max_version);

bool ssl_supports_version(const SSL_HANDSHAKE *hs, uint16_t version);

bool ssl_method_supports_version(const SSL_PROTOCOL_METHOD *method,
                                 uint16_t version);

bool ssl_add_supported_versions(const SSL_HANDSHAKE *hs, CBB *cbb,
                                uint16_t extra_min_version);

bool ssl_negotiate_version(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                           uint16_t *out_version, const CBS *peer_versions);

bool ssl_has_final_version(const SSL *ssl);

uint16_t ssl_protocol_version(const SSL *ssl);

BSSL_NAMESPACE_END

struct ssl_cipher_st {

  const char *name;

  const char *standard_name;

  uint32_t id;

  uint32_t algorithm_mkey;
  uint32_t algorithm_auth;
  uint32_t algorithm_enc;
  uint32_t algorithm_mac;
  uint32_t algorithm_prf;
};

BSSL_NAMESPACE_BEGIN

#define SSL_kRSA 0x00000001u
#define SSL_kECDHE 0x00000002u

#define SSL_kPSK 0x00000004u
#define SSL_kGENERIC 0x00000008u

#define SSL_aRSA_SIGN 0x00000001u
#define SSL_aRSA_DECRYPT 0x00000002u
#define SSL_aECDSA 0x00000004u

#define SSL_aPSK 0x00000008u
#define SSL_aGENERIC 0x00000010u

#define SSL_aCERT (SSL_aRSA_SIGN | SSL_aRSA_DECRYPT | SSL_aECDSA)

#define SSL_3DES 0x00000001u
#define SSL_AES128 0x00000002u
#define SSL_AES256 0x00000004u
#define SSL_AES128GCM 0x00000008u
#define SSL_AES256GCM 0x00000010u
#define SSL_CHACHA20POLY1305 0x00000020u

#define SSL_AES (SSL_AES128 | SSL_AES256 | SSL_AES128GCM | SSL_AES256GCM)

#define SSL_SHA1 0x00000001u
#define SSL_SHA256 0x00000002u

#define SSL_AEAD 0x00000004u

#define SSL_HANDSHAKE_MAC_DEFAULT 0x1
#define SSL_HANDSHAKE_MAC_SHA256 0x2
#define SSL_HANDSHAKE_MAC_SHA384 0x4

#define SSL_MAX_MD_SIZE 48

struct SSLCipherPreferenceList {
  static constexpr bool kAllowUniquePtr = true;

  SSLCipherPreferenceList() = default;
  ~SSLCipherPreferenceList();

  bool Init(UniquePtr<STACK_OF(SSL_CIPHER)> ciphers,
            Span<const bool> in_group_flags);
  bool Init(const SSLCipherPreferenceList &);

  void Remove(const SSL_CIPHER *cipher);

  UniquePtr<STACK_OF(SSL_CIPHER)> ciphers;
  bool *in_group_flags = nullptr;
};

Span<const SSL_CIPHER> AllCiphers();

bool ssl_cipher_get_evp_aead(const EVP_AEAD **out_aead,
                             size_t *out_mac_secret_len,
                             size_t *out_fixed_iv_len, const SSL_CIPHER *cipher,
                             uint16_t version);

const EVP_MD *ssl_get_handshake_digest(uint16_t version,
                                       const SSL_CIPHER *cipher);

bool ssl_create_cipher_list(UniquePtr<SSLCipherPreferenceList> *out_cipher_list,
                            const bool has_aes_hw, const char *rule_str,
                            bool strict);

uint32_t ssl_cipher_auth_mask_for_key(const EVP_PKEY *key, bool sign_ok);

bool ssl_cipher_uses_certificate_auth(const SSL_CIPHER *cipher);

bool ssl_cipher_requires_server_key_exchange(const SSL_CIPHER *cipher);

size_t ssl_cipher_get_record_split_len(const SSL_CIPHER *cipher);

const SSL_CIPHER *ssl_choose_tls13_cipher(CBS cipher_suites, bool has_aes_hw,
                                          uint16_t version,
                                          enum ssl_compliance_policy_t policy);

bool ssl_tls13_cipher_meets_policy(uint16_t cipher_id,
                                   enum ssl_compliance_policy_t policy);

OPENSSL_EXPORT bool ssl_cipher_is_deprecated(const SSL_CIPHER *cipher);

class SSLTranscript {
 public:
  explicit SSLTranscript(bool is_dtls);
  ~SSLTranscript();

  SSLTranscript(SSLTranscript &&other) = default;
  SSLTranscript &operator=(SSLTranscript &&other) = default;

  bool Init();

  bool InitHash(uint16_t version, const SSL_CIPHER *cipher);

  bool UpdateForHelloRetryRequest();

  bool CopyToHashContext(EVP_MD_CTX *ctx, const EVP_MD *digest) const;

  Span<const uint8_t> buffer() const {
    return Span(reinterpret_cast<const uint8_t *>(buffer_->data),
                buffer_->length);
  }

  void FreeBuffer();

  size_t DigestLen() const;

  const EVP_MD *Digest() const;

  bool Update(Span<const uint8_t> in);

  bool GetHash(uint8_t *out, size_t *out_len) const;

  bool GetFinishedMAC(uint8_t *out, size_t *out_len, const SSL_SESSION *session,
                      bool from_server) const;

 private:

  bool HashBuffer(EVP_MD_CTX *ctx, const EVP_MD *digest) const;

  bool AddToBufferOrHash(Span<const uint8_t> in);

  UniquePtr<BUF_MEM> buffer_;

  ScopedEVP_MD_CTX hash_;

  bool is_dtls_ : 1;

  uint16_t version_ = 0;
};

bool tls1_prf(const EVP_MD *digest, Span<uint8_t> out,
              Span<const uint8_t> secret, std::string_view label,
              Span<const uint8_t> seed1, Span<const uint8_t> seed2);

class SSLAEADContext {
 public:
  explicit SSLAEADContext(const SSL_CIPHER *cipher);
  ~SSLAEADContext();
  static constexpr bool kAllowUniquePtr = true;

  SSLAEADContext(const SSLAEADContext &&) = delete;
  SSLAEADContext &operator=(const SSLAEADContext &&) = delete;

  static UniquePtr<SSLAEADContext> CreateNullCipher();

  static UniquePtr<SSLAEADContext> Create(enum evp_aead_direction_t direction,
                                          uint16_t version,
                                          const SSL_CIPHER *cipher,
                                          Span<const uint8_t> enc_key,
                                          Span<const uint8_t> mac_key,
                                          Span<const uint8_t> fixed_iv);

  static UniquePtr<SSLAEADContext> CreatePlaceholderForQUIC(
      const SSL_CIPHER *cipher);

  const SSL_CIPHER *cipher() const { return cipher_; }

  bool is_null_cipher() const { return !cipher_; }

  size_t ExplicitNonceLen() const;

  size_t MaxOverhead() const;

  size_t MaxSealInputLen(size_t max_out) const;

  bool SuffixLen(size_t *out_suffix_len, size_t in_len,
                 size_t extra_in_len) const;

  bool CiphertextLen(size_t *out_len, size_t in_len, size_t extra_in_len) const;

  bool Open(Span<uint8_t> *out, uint8_t type, uint16_t record_version,
            uint64_t seqnum, Span<const uint8_t> header, Span<uint8_t> in);

  bool Seal(uint8_t *out, size_t *out_len, size_t max_out, uint8_t type,
            uint16_t record_version, uint64_t seqnum,
            Span<const uint8_t> header, const uint8_t *in, size_t in_len);

  bool SealScatter(uint8_t *out_prefix, uint8_t *out, uint8_t *out_suffix,
                   uint8_t type, uint16_t record_version, uint64_t seqnum,
                   Span<const uint8_t> header, const uint8_t *in, size_t in_len,
                   const uint8_t *extra_in, size_t extra_in_len);

  bool GetIV(const uint8_t **out_iv, size_t *out_iv_len) const;

 private:

  Span<const uint8_t> GetAdditionalData(uint8_t storage[13], uint8_t type,
                                        uint16_t record_version,
                                        uint64_t seqnum, size_t plaintext_len,
                                        Span<const uint8_t> header);

  const SSL_CIPHER *cipher_;
  ScopedEVP_AEAD_CTX ctx_;

  InplaceVector<uint8_t, 12> fixed_nonce_;
  uint8_t variable_nonce_len_ = 0;

  bool variable_nonce_included_in_record_ : 1;

  bool random_variable_nonce_ : 1;

  bool xor_fixed_nonce_ : 1;

  bool omit_length_in_ad_ : 1;

  bool ad_is_header_ : 1;
};

class DTLSReplayBitmap {
 public:

  bool ShouldDiscard(uint64_t seqnum) const;

  void Record(uint64_t seqnum);

  uint64_t max_seq_num() const { return max_seq_num_; }

 private:

  std::bitset<256> map_;

  uint64_t max_seq_num_ = 0;
};

OPENSSL_EXPORT uint64_t reconstruct_seqnum(uint16_t wire_seq, uint64_t seq_mask,
                                           uint64_t max_valid_seqnum);

class DTLSRecordNumber {
 public:
  static constexpr uint64_t kMaxSequence = (uint64_t{1} << 48) - 1;

  DTLSRecordNumber() = default;
  DTLSRecordNumber(uint16_t epoch, uint64_t sequence) {
    BSSL_CHECK(sequence <= kMaxSequence);
    combined_ = (uint64_t{epoch} << 48) | sequence;
  }

  static DTLSRecordNumber FromCombined(uint64_t combined) {
    return DTLSRecordNumber(combined);
  }

  bool operator==(DTLSRecordNumber r) const {
    return combined() == r.combined();
  }
  bool operator!=(DTLSRecordNumber r) const { return !((*this) == r); }
  bool operator<(DTLSRecordNumber r) const { return combined() < r.combined(); }

  uint64_t combined() const { return combined_; }
  uint16_t epoch() const { return combined_ >> 48; }
  uint64_t sequence() const { return combined_ & kMaxSequence; }

  bool HasNext() const { return sequence() < kMaxSequence; }
  DTLSRecordNumber Next() const {
    BSSL_CHECK(HasNext());

    return DTLSRecordNumber::FromCombined(combined_ + 1);
  }

 private:
  explicit DTLSRecordNumber(uint64_t combined) : combined_(combined) {}

  uint64_t combined_ = 0;
};

class RecordNumberEncrypter {
 public:
  static constexpr bool kAllowUniquePtr = true;
  static constexpr size_t kMaxKeySize = 32;

  static UniquePtr<RecordNumberEncrypter> Create(
      const SSL_CIPHER *cipher, Span<const uint8_t> traffic_secret);

  virtual ~RecordNumberEncrypter() = default;
  virtual size_t KeySize() = 0;
  virtual bool SetKey(Span<const uint8_t> key) = 0;
  virtual bool GenerateMask(Span<uint8_t> out, Span<const uint8_t> sample) = 0;
};

struct DTLSReadEpoch {
  static constexpr bool kAllowUniquePtr = true;

  uint16_t epoch = 0;
  UniquePtr<SSLAEADContext> aead;
  UniquePtr<RecordNumberEncrypter> rn_encrypter;
  DTLSReplayBitmap bitmap;
};

struct DTLSWriteEpoch {
  static constexpr bool kAllowUniquePtr = true;

  uint16_t epoch() const { return next_record.epoch(); }

  DTLSRecordNumber next_record;
  UniquePtr<SSLAEADContext> aead;
  UniquePtr<RecordNumberEncrypter> rn_encrypter;
};

size_t ssl_record_prefix_len(const SSL *ssl);

enum ssl_open_record_t {
  ssl_open_record_success,
  ssl_open_record_discard,
  ssl_open_record_partial,
  ssl_open_record_close_notify,
  ssl_open_record_error,
};

enum ssl_open_record_t tls_open_record(SSL *ssl, uint8_t *out_type,
                                       Span<uint8_t> *out, size_t *out_consumed,
                                       uint8_t *out_alert, Span<uint8_t> in);

enum ssl_open_record_t dtls_open_record(SSL *ssl, uint8_t *out_type,
                                        DTLSRecordNumber *out_number,
                                        Span<uint8_t> *out,
                                        size_t *out_consumed,
                                        uint8_t *out_alert, Span<uint8_t> in);

bool ssl_needs_record_splitting(const SSL *ssl);

bool tls_seal_record(SSL *ssl, uint8_t *out, size_t *out_len, size_t max_out,
                     uint8_t type, const uint8_t *in, size_t in_len);

size_t dtls_record_header_write_len(const SSL *ssl, uint16_t epoch);

size_t dtls_max_seal_overhead(const SSL *ssl, uint16_t epoch);

size_t dtls_seal_prefix_len(const SSL *ssl, uint16_t epoch);

size_t dtls_seal_max_input_len(const SSL *ssl, uint16_t epoch, size_t max_out);

bool dtls_seal_record(SSL *ssl, DTLSRecordNumber *out_number, uint8_t *out,
                      size_t *out_len, size_t max_out, uint8_t type,
                      const uint8_t *in, size_t in_len, uint16_t epoch);

enum ssl_open_record_t ssl_process_alert(SSL *ssl, uint8_t *out_alert,
                                         Span<const uint8_t> in);

enum ssl_private_key_result_t ssl_private_key_sign(
    SSL_HANDSHAKE *hs, uint8_t *out, size_t *out_len, size_t max_out,
    uint16_t sigalg, Span<const uint8_t> in);

enum ssl_private_key_result_t ssl_private_key_decrypt(SSL_HANDSHAKE *hs,
                                                      uint8_t *out,
                                                      size_t *out_len,
                                                      size_t max_out,
                                                      Span<const uint8_t> in);

bool ssl_pkey_supports_algorithm(const SSL *ssl, EVP_PKEY *pkey,
                                 uint16_t sigalg, bool is_verify);

bool ssl_public_key_verify(SSL *ssl, Span<const uint8_t> signature,
                           uint16_t sigalg, EVP_PKEY *pkey,
                           Span<const uint8_t> in);

class SSLKeyShare {
 public:
  virtual ~SSLKeyShare() {}
  static constexpr bool kAllowUniquePtr = true;

  static UniquePtr<SSLKeyShare> Create(uint16_t group_id);

  virtual uint16_t GroupID() const = 0;

  virtual bool Generate(CBB *out_public_key) = 0;

  virtual bool Encap(CBB *out_ciphertext, Array<uint8_t> *out_secret,
                     uint8_t *out_alert, Span<const uint8_t> peer_key) = 0;

  virtual bool Decap(Array<uint8_t> *out_secret, uint8_t *out_alert,
                     Span<const uint8_t> ciphertext) = 0;

  virtual bool SerializePrivateKey(CBB *out) { return false; }

  virtual bool DeserializePrivateKey(CBS *in) { return false; }
};

struct NamedGroup {
  int nid;
  uint16_t group_id;
  const char name[32], alias[32];
};

Span<const NamedGroup> NamedGroups();

bool ssl_nid_to_group_id(uint16_t *out_group_id, int nid);

bool ssl_name_to_group_id(uint16_t *out_group_id, const char *name, size_t len);

int ssl_group_id_to_nid(uint16_t group_id);

struct SSLMessage {
  bool is_v2_hello;
  uint8_t type;
  CBS body;

  CBS raw;
};

#define SSL_MAX_HANDSHAKE_FLIGHT 7

extern const uint8_t kHelloRetryRequest[SSL3_RANDOM_SIZE];
extern const uint8_t kTLS12DowngradeRandom[8];
extern const uint8_t kTLS13DowngradeRandom[8];
extern const uint8_t kJDK11DowngradeRandom[8];

size_t ssl_max_handshake_message_len(const SSL *ssl);

bool tls_can_accept_handshake_data(const SSL *ssl, uint8_t *out_alert);

bool tls_has_unprocessed_handshake_data(const SSL *ssl);

bool tls_append_handshake_data(SSL *ssl, Span<const uint8_t> data);

bool dtls_has_unprocessed_handshake_data(const SSL *ssl);

bool tls_flush_pending_hs_data(SSL *ssl);

void dtls_clear_outgoing_messages(SSL *ssl);

void dtls_clear_unused_write_epochs(SSL *ssl);

void ssl_do_info_callback(const SSL *ssl, int type, int value);

void ssl_do_msg_callback(const SSL *ssl, int is_write, int content_type,
                         Span<const uint8_t> in);

class SSLBuffer {
 public:
  SSLBuffer() {}
  ~SSLBuffer() { Clear(); }

  SSLBuffer(const SSLBuffer &) = delete;
  SSLBuffer &operator=(const SSLBuffer &) = delete;

  uint8_t *data() { return buf_ + offset_; }
  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  size_t cap() const { return cap_; }

  Span<uint8_t> span() { return Span(data(), size()); }

  Span<uint8_t> remaining() { return Span(data() + size(), cap() - size()); }

  void Clear();

  bool EnsureCap(size_t header_len, size_t new_cap);

  void DidWrite(size_t len);

  void Consume(size_t len);

  void DiscardConsumed();

 private:

  uint8_t *buf_ = nullptr;

  uint16_t offset_ = 0;

  uint16_t size_ = 0;

  uint16_t cap_ = 0;

  uint8_t inline_buf_[SSL3_RT_HEADER_LENGTH];
};

int ssl_read_buffer_extend_to(SSL *ssl, size_t len);

int ssl_handle_open_record(SSL *ssl, bool *out_retry, ssl_open_record_t ret,
                           size_t consumed, uint8_t alert);

int ssl_write_buffer_flush(SSL *ssl);

bool ssl_parse_cert_chain(uint8_t *out_alert,
                          UniquePtr<STACK_OF(CRYPTO_BUFFER)> *out_chain,
                          UniquePtr<EVP_PKEY> *out_pubkey,
                          uint8_t *out_leaf_sha256, CBS *cbs,
                          CRYPTO_BUFFER_POOL *pool);

enum ssl_key_usage_t {
  key_usage_digital_signature = 0,
  key_usage_encipherment = 2,
};

OPENSSL_EXPORT bool ssl_cert_check_key_usage(const CBS *in,
                                             enum ssl_key_usage_t bit);

OPENSSL_EXPORT bool ssl_cert_extract_issuer(const CBS *in, CBS *out_dn);

bool ssl_cert_matches_issuer(const CBS *in, const CBS *dn);

UniquePtr<EVP_PKEY> ssl_cert_parse_pubkey(const CBS *in);

UniquePtr<STACK_OF(CRYPTO_BUFFER)> SSL_parse_CA_list(SSL *ssl,
                                                     uint8_t *out_alert,
                                                     CBS *cbs);

bool ssl_has_client_CAs(const SSL_CONFIG *cfg);

bool ssl_add_client_CA_list(const SSL_HANDSHAKE *hs, CBB *cbb);

bool ssl_has_CA_names(const SSL_CONFIG *cfg);

bool ssl_add_CA_names(const SSL_HANDSHAKE *hs, CBB *cbb);

bool ssl_check_leaf_certificate(SSL_HANDSHAKE *hs, EVP_PKEY *pkey,
                                const CRYPTO_BUFFER *leaf);

bool tls13_init_key_schedule(SSL_HANDSHAKE *hs, Span<const uint8_t> psk);

bool tls13_init_early_key_schedule(SSL_HANDSHAKE *hs,
                                   const SSL_SESSION *session);

bool tls13_advance_key_schedule(SSL_HANDSHAKE *hs, Span<const uint8_t> in);

bool tls13_set_traffic_key(SSL *ssl, enum ssl_encryption_level_t level,
                           enum evp_aead_direction_t direction,
                           const SSL_SESSION *session,
                           Span<const uint8_t> traffic_secret);

bool tls13_derive_early_secret(SSL_HANDSHAKE *hs);

bool tls13_derive_handshake_secrets(SSL_HANDSHAKE *hs);

bool tls13_rotate_traffic_key(SSL *ssl, enum evp_aead_direction_t direction);

bool tls13_derive_application_secrets(SSL_HANDSHAKE *hs);

bool tls13_derive_resumption_secret(SSL_HANDSHAKE *hs);

bool tls13_export_keying_material(const SSL *ssl, Span<uint8_t> out,
                                  Span<const uint8_t> secret,
                                  std::string_view label,
                                  Span<const uint8_t> context);

bool tls13_finished_mac(SSL_HANDSHAKE *hs, uint8_t *out, size_t *out_len,
                        bool is_server);

bool tls13_derive_session_psk(SSL_SESSION *session, Span<const uint8_t> nonce,
                              bool is_dtls);

bool tls13_write_psk_binder(const SSL_HANDSHAKE *hs,
                            const SSLTranscript &transcript, Span<uint8_t> msg,
                            size_t *out_binder_len);

bool tls13_verify_psk_binder(const SSL_HANDSHAKE *hs,
                             const SSL_SESSION *session, const SSLMessage &msg,
                             CBS *binders);

struct ECHConfig {
  static constexpr bool kAllowUniquePtr = true;

  Array<uint8_t> raw;

  Span<const uint8_t> public_key;
  Span<const uint8_t> public_name;
  Span<const uint8_t> cipher_suites;
  uint16_t kem_id = 0;
  uint8_t maximum_name_length = 0;
  uint8_t config_id = 0;
};

class ECHServerConfig {
 public:
  static constexpr bool kAllowUniquePtr = true;
  ECHServerConfig() = default;
  ECHServerConfig(const ECHServerConfig &other) = delete;
  ECHServerConfig &operator=(ECHServerConfig &&) = delete;

  bool Init(Span<const uint8_t> ech_config, const EVP_HPKE_KEY *key,
            bool is_retry_config);

  bool SetupContext(EVP_HPKE_CTX *ctx, uint16_t kdf_id, uint16_t aead_id,
                    Span<const uint8_t> enc) const;

  const ECHConfig &ech_config() const { return ech_config_; }
  bool is_retry_config() const { return is_retry_config_; }

 private:
  ECHConfig ech_config_;
  ScopedEVP_HPKE_KEY key_;
  bool is_retry_config_ = false;
};

enum ssl_client_hello_type_t {
  ssl_client_hello_unencrypted,
  ssl_client_hello_inner,
  ssl_client_hello_outer,
};

#define ECH_CLIENT_OUTER 0
#define ECH_CLIENT_INNER 1

OPENSSL_EXPORT bool ssl_decode_client_hello_inner(
    SSL *ssl, uint8_t *out_alert, Array<uint8_t> *out_client_hello_inner,
    Span<const uint8_t> encoded_client_hello_inner,
    const SSL_CLIENT_HELLO *client_hello_outer);

bool ssl_client_hello_decrypt(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                              bool *out_is_decrypt_error, Array<uint8_t> *out,
                              const SSL_CLIENT_HELLO *client_hello_outer,
                              Span<const uint8_t> payload);

#define ECH_CONFIRMATION_SIGNAL_LEN 8

size_t ssl_ech_confirmation_signal_hello_offset(const SSL *ssl);

bool ssl_ech_accept_confirmation(const SSL_HANDSHAKE *hs, Span<uint8_t> out,
                                 Span<const uint8_t> client_random,
                                 const SSLTranscript &transcript, bool is_hrr,
                                 Span<const uint8_t> msg, size_t offset);

OPENSSL_EXPORT bool ssl_is_valid_ech_public_name(
    Span<const uint8_t> public_name);

bool ssl_is_valid_ech_config_list(Span<const uint8_t> ech_config_list);

bool ssl_select_ech_config(SSL_HANDSHAKE *hs, Span<uint8_t> out_enc,
                           size_t *out_enc_len);

size_t ssl_ech_extension_body_length(const EVP_HPKE_AEAD *aead, size_t enc_len,
                                     size_t in_len);

bool ssl_encrypt_client_hello(SSL_HANDSHAKE *hs, Span<const uint8_t> enc);

enum class SSLCredentialType {
  kX509,
  kDelegated,
  kSPAKE2PlusV1Client,
  kSPAKE2PlusV1Server,
};

BSSL_NAMESPACE_END

struct ssl_credential_st : public bssl::RefCounted<ssl_credential_st> {
  explicit ssl_credential_st(bssl::SSLCredentialType type);
  ssl_credential_st(const ssl_credential_st &) = delete;
  ssl_credential_st &operator=(const ssl_credential_st &) = delete;

  bssl::UniquePtr<SSL_CREDENTIAL> Dup() const;

  void ClearCertAndKey();

  bool UsesX509() const;

  bool UsesPrivateKey() const;

  bool IsComplete() const;

  bool SetLeafCert(bssl::UniquePtr<CRYPTO_BUFFER> leaf,
                   bool discard_key_on_mismatch);

  void ClearIntermediateCerts();

  bool AppendIntermediateCert(bssl::UniquePtr<CRYPTO_BUFFER> cert);

  bool ChainContainsIssuer(bssl::Span<const uint8_t> dn) const;

  bssl::SSLCredentialType type;

  bssl::UniquePtr<EVP_PKEY> pubkey;

  bssl::UniquePtr<EVP_PKEY> privkey;

  const SSL_PRIVATE_KEY_METHOD *key_method = nullptr;

  bssl::Array<uint16_t> sigalgs;

  bssl::UniquePtr<STACK_OF(CRYPTO_BUFFER)> chain;

  bssl::UniquePtr<CRYPTO_BUFFER> dc;

  uint16_t dc_algorithm = 0;

  bssl::UniquePtr<CRYPTO_BUFFER> signed_cert_timestamp_list;

  bssl::UniquePtr<CRYPTO_BUFFER> ocsp_response;

  bssl::Array<uint8_t> pake_context;
  bssl::Array<uint8_t> client_identity;
  bssl::Array<uint8_t> server_identity;
  bssl::Array<uint8_t> password_verifier_w0;
  bssl::Array<uint8_t> password_verifier_w1;
  bssl::Array<uint8_t> registration_record;
  mutable std::atomic<uint32_t> pake_limit;

  bool HasPAKEAttempts() const;

  bool ClaimPAKEAttempt() const;

  void RestorePAKEAttempt() const;

  bssl::Array<uint8_t> trust_anchor_id;

  CRYPTO_EX_DATA ex_data;

  bool must_match_issuer = false;

 private:
  friend RefCounted;
  ~ssl_credential_st();
};

BSSL_NAMESPACE_BEGIN

bool ssl_get_full_credential_list(SSL_HANDSHAKE *hs,
                                  Array<SSL_CREDENTIAL *> *out);

bool ssl_credential_matches_requested_issuers(SSL_HANDSHAKE *hs,
                                              const SSL_CREDENTIAL *cred);

enum ssl_hs_wait_t {
  ssl_hs_error,
  ssl_hs_ok,
  ssl_hs_read_server_hello,
  ssl_hs_read_message,
  ssl_hs_flush,
  ssl_hs_certificate_selection_pending,
  ssl_hs_handoff,
  ssl_hs_handback,
  ssl_hs_x509_lookup,
  ssl_hs_private_key_operation,
  ssl_hs_pending_session,
  ssl_hs_pending_ticket,
  ssl_hs_early_return,
  ssl_hs_early_data_rejected,
  ssl_hs_read_end_of_early_data,
  ssl_hs_read_change_cipher_spec,
  ssl_hs_certificate_verify,
  ssl_hs_hints_ready,
};

enum ssl_grease_index_t {
  ssl_grease_cipher = 0,
  ssl_grease_group,
  ssl_grease_extension1,
  ssl_grease_extension2,
  ssl_grease_version,
  ssl_grease_ticket_extension,
  ssl_grease_ech_config_id,
  ssl_grease_last_index = ssl_grease_ech_config_id,
};

enum tls12_server_hs_state_t {
  state12_start_accept = 0,
  state12_read_client_hello,
  state12_read_client_hello_after_ech,
  state12_cert_callback,
  state12_tls13,
  state12_select_parameters,
  state12_send_server_hello,
  state12_send_server_certificate,
  state12_send_server_key_exchange,
  state12_send_server_hello_done,
  state12_read_client_certificate,
  state12_verify_client_certificate,
  state12_read_client_key_exchange,
  state12_read_client_certificate_verify,
  state12_read_change_cipher_spec,
  state12_process_change_cipher_spec,
  state12_read_next_proto,
  state12_read_channel_id,
  state12_read_client_finished,
  state12_send_server_finished,
  state12_finish_server_handshake,
  state12_done,
};

enum tls13_server_hs_state_t {
  state13_select_parameters = 0,
  state13_select_session,
  state13_send_hello_retry_request,
  state13_read_second_client_hello,
  state13_send_server_hello,
  state13_send_server_certificate_verify,
  state13_send_server_finished,
  state13_send_half_rtt_ticket,
  state13_read_second_client_flight,
  state13_process_end_of_early_data,
  state13_read_client_encrypted_extensions,
  state13_read_client_certificate,
  state13_read_client_certificate_verify,
  state13_read_channel_id,
  state13_read_client_finished,
  state13_send_new_session_ticket,
  state13_done,
};

enum handback_t {
  handback_after_session_resumption = 0,
  handback_after_ecdhe = 1,
  handback_after_handshake = 2,
  handback_tls13 = 3,
  handback_max_value = handback_tls13,
};

struct SSL_HANDSHAKE_HINTS {
  static constexpr bool kAllowUniquePtr = true;

  Array<uint8_t> server_random_tls12;
  Array<uint8_t> server_random_tls13;

  uint16_t key_share_group_id = 0;
  Array<uint8_t> key_share_ciphertext;
  Array<uint8_t> key_share_secret;

  uint16_t signature_algorithm = 0;
  Array<uint8_t> signature_input;
  Array<uint8_t> signature_spki;
  Array<uint8_t> signature;

  Array<uint8_t> decrypted_psk;
  bool ignore_psk = false;

  uint16_t cert_compression_alg_id = 0;
  Array<uint8_t> cert_compression_input;
  Array<uint8_t> cert_compression_output;

  uint16_t ecdhe_group_id = 0;
  Array<uint8_t> ecdhe_public_key;
  Array<uint8_t> ecdhe_private_key;

  Array<uint8_t> decrypted_ticket;
  bool renew_ticket = false;
  bool ignore_ticket = false;
};

struct SSLPAKEShare {
  static constexpr bool kAllowUniquePtr = true;
  uint16_t named_pake;
  Array<uint8_t> client_identity;
  Array<uint8_t> server_identity;
  Array<uint8_t> pake_message;
};

struct SSL_HANDSHAKE {
  explicit SSL_HANDSHAKE(SSL *ssl);
  ~SSL_HANDSHAKE();
  static constexpr bool kAllowUniquePtr = true;

  SSL *ssl;

  SSL_CONFIG *config;

  enum ssl_hs_wait_t wait = ssl_hs_ok;

  int state = 0;

  int tls13_state = 0;

  uint16_t min_version = 0;

  uint16_t max_version = 0;

  InplaceVector<uint8_t, SSL_MAX_MD_SIZE> secret;
  InplaceVector<uint8_t, SSL_MAX_MD_SIZE> early_traffic_secret;
  InplaceVector<uint8_t, SSL_MAX_MD_SIZE> client_handshake_secret;
  InplaceVector<uint8_t, SSL_MAX_MD_SIZE> server_handshake_secret;
  InplaceVector<uint8_t, SSL_MAX_MD_SIZE> client_traffic_secret_0;
  InplaceVector<uint8_t, SSL_MAX_MD_SIZE> server_traffic_secret_0;
  InplaceVector<uint8_t, SSL_MAX_MD_SIZE> expected_client_finished;

  bool GetClientHello(SSLMessage *out_msg, SSL_CLIENT_HELLO *out_client_hello);

  union {

    uint32_t sent = 0;

    uint32_t received;
  } extensions;

  uint32_t inner_extensions_sent = 0;

  UniquePtr<ERR_SAVE_STATE> error;

  UniquePtr<SSLKeyShare> key_shares[2];

  SSLTranscript transcript;

  SSLTranscript inner_transcript;

  uint8_t inner_client_random[SSL3_RANDOM_SIZE] = {0};

  Array<uint8_t> cookie;

  Array<uint8_t> dtls_cookie;

  Array<uint8_t> ech_client_outer;

  Array<uint8_t> ech_retry_configs;

  Array<uint8_t> ech_client_hello_buf;

  Array<uint8_t> key_share_bytes;

  Array<uint8_t> key_share_ciphertext;

  Array<uint16_t> peer_sigalgs;

  Array<uint16_t> peer_supported_group_list;

  Array<uint16_t> peer_delegated_credential_sigalgs;

  Array<uint8_t> peer_key;

  Array<uint8_t> extension_permutation;

  uint16_t cert_compression_alg_id;

  ScopedEVP_HPKE_CTX ech_hpke_ctx;

  Array<uint8_t> server_params;

  UniquePtr<char> peer_psk_identity_hint;

  UniquePtr<STACK_OF(CRYPTO_BUFFER)> ca_names;

  std::optional<Array<uint8_t>> peer_requested_trust_anchors;

  Array<uint8_t> peer_available_trust_anchors;

  STACK_OF(X509_NAME) *cached_x509_ca_names = nullptr;

  Array<uint8_t> certificate_types;

  UniquePtr<SSL_CREDENTIAL> credential;

  UniquePtr<EVP_PKEY> peer_pubkey;

  UniquePtr<SSL_SESSION> new_session;

  UniquePtr<SSL_SESSION> early_session;

  UniquePtr<SSL_ECH_KEYS> ech_keys;

  UniquePtr<ECHConfig> selected_ech_config;

  const SSL_CIPHER *new_cipher = nullptr;

  Array<uint8_t> key_block;

  UniquePtr<SSL_HANDSHAKE_HINTS> hints;

  bool ech_is_inner : 1;

  bool ech_authenticated_reject : 1;

  bool scts_requested : 1;

  bool handshake_finalized : 1;

  bool accept_psk_mode : 1;

  bool cert_request : 1;

  bool certificate_status_expected : 1;

  bool ocsp_stapling_requested : 1;

  bool should_ack_sni : 1;

  bool in_false_start : 1;

  bool in_early_data : 1;

  bool early_data_offered : 1;

  bool can_early_read : 1;

  bool can_early_write : 1;

  bool is_early_version : 1;

  bool next_proto_neg_seen : 1;

  bool ticket_expected : 1;

  bool extended_master_secret : 1;

  bool pending_private_key_op : 1;

  bool handback : 1;

  bool hints_requested : 1;

  bool cert_compression_negotiated : 1;

  bool apply_jdk11_workaround : 1;

  bool can_release_private_key : 1;

  bool channel_id_negotiated : 1;

  bool received_hello_verify_request : 1;

  bool matched_peer_trust_anchor : 1;

  bool peer_matched_trust_anchor : 1;

  uint16_t client_version = 0;

  uint16_t early_data_read = 0;

  uint16_t early_data_written = 0;

  uint16_t signature_algorithm = 0;

  uint8_t ech_config_id = 0;

  InplaceVector<uint8_t, SSL_MAX_SSL_SESSION_ID_LENGTH> session_id;

  uint8_t grease_seed[ssl_grease_last_index + 1] = {0};

  UniquePtr<SSLPAKEShare> pake_share;

  Array<uint8_t> pake_share_bytes;

  UniquePtr<spake2plus::Prover> pake_prover;

  UniquePtr<spake2plus::Verifier> pake_verifier;
};

constexpr size_t kMaxTickets = 16;

UniquePtr<SSL_HANDSHAKE> ssl_handshake_new(SSL *ssl);

bool ssl_check_message_type(SSL *ssl, const SSLMessage &msg, int type);

int ssl_run_handshake(SSL_HANDSHAKE *hs, bool *out_early_return);

enum ssl_hs_wait_t ssl_client_handshake(SSL_HANDSHAKE *hs);
enum ssl_hs_wait_t ssl_server_handshake(SSL_HANDSHAKE *hs);
enum ssl_hs_wait_t tls13_client_handshake(SSL_HANDSHAKE *hs);
enum ssl_hs_wait_t tls13_server_handshake(SSL_HANDSHAKE *hs);

const char *ssl_client_handshake_state(SSL_HANDSHAKE *hs);
const char *ssl_server_handshake_state(SSL_HANDSHAKE *hs);
const char *tls13_client_handshake_state(SSL_HANDSHAKE *hs);
const char *tls13_server_handshake_state(SSL_HANDSHAKE *hs);

bool tls13_add_key_update(SSL *ssl, int request_type);

bool tls13_post_handshake(SSL *ssl, const SSLMessage &msg);

bool tls13_process_certificate(SSL_HANDSHAKE *hs, const SSLMessage &msg,
                               bool allow_anonymous);
bool tls13_process_certificate_verify(SSL_HANDSHAKE *hs, const SSLMessage &msg);

bool tls13_process_finished(SSL_HANDSHAKE *hs, const SSLMessage &msg,
                            bool use_saved_value);

bool tls13_add_certificate(SSL_HANDSHAKE *hs);

enum ssl_private_key_result_t tls13_add_certificate_verify(SSL_HANDSHAKE *hs);

bool tls13_add_finished(SSL_HANDSHAKE *hs);
bool tls13_process_new_session_ticket(SSL *ssl, const SSLMessage &msg);
bssl::UniquePtr<SSL_SESSION> tls13_create_session_with_ticket(SSL *ssl,
                                                              CBS *body);

bool ssl_setup_extension_permutation(SSL_HANDSHAKE *hs);

bool ssl_setup_key_shares(SSL_HANDSHAKE *hs, uint16_t override_group_id);

bool ssl_setup_pake_shares(SSL_HANDSHAKE *hs);

bool ssl_ext_key_share_parse_serverhello(SSL_HANDSHAKE *hs,
                                         Array<uint8_t> *out_secret,
                                         uint8_t *out_alert, CBS *contents);
bool ssl_ext_key_share_parse_clienthello(SSL_HANDSHAKE *hs, bool *out_found,
                                         Span<const uint8_t> *out_peer_key,
                                         uint8_t *out_alert,
                                         const SSL_CLIENT_HELLO *client_hello);
bool ssl_ext_pake_add_serverhello(SSL_HANDSHAKE *hs, CBB *out);
bool ssl_ext_key_share_add_serverhello(SSL_HANDSHAKE *hs, CBB *out);

bool ssl_ext_pake_parse_serverhello(SSL_HANDSHAKE *hs,
                                    Array<uint8_t> *out_secret,
                                    uint8_t *out_alert, CBS *contents);

bool ssl_ext_pre_shared_key_parse_serverhello(SSL_HANDSHAKE *hs,
                                              uint8_t *out_alert,
                                              CBS *contents);
bool ssl_ext_pre_shared_key_parse_clienthello(
    SSL_HANDSHAKE *hs, CBS *out_ticket, CBS *out_binders,
    uint32_t *out_obfuscated_ticket_age, uint8_t *out_alert,
    const SSL_CLIENT_HELLO *client_hello, CBS *contents);
bool ssl_ext_pre_shared_key_add_serverhello(SSL_HANDSHAKE *hs, CBB *out);

bool ssl_is_sct_list_valid(const CBS *contents);

bool ssl_write_client_hello_without_extensions(const SSL_HANDSHAKE *hs,
                                               CBB *cbb,
                                               ssl_client_hello_type_t type,
                                               bool empty_session_id);

bool ssl_add_client_hello(SSL_HANDSHAKE *hs);

struct ParsedServerHello {
  CBS raw;
  uint16_t legacy_version = 0;
  CBS random;
  CBS session_id;
  uint16_t cipher_suite = 0;
  uint8_t compression_method = 0;
  CBS extensions;
};

bool ssl_parse_server_hello(ParsedServerHello *out, uint8_t *out_alert,
                            const SSLMessage &msg);

enum ssl_cert_verify_context_t {
  ssl_cert_verify_server,
  ssl_cert_verify_client,
  ssl_cert_verify_channel_id,
};

bool tls13_get_cert_verify_signature_input(
    SSL_HANDSHAKE *hs, Array<uint8_t> *out,
    enum ssl_cert_verify_context_t cert_verify_context);

bool ssl_is_valid_alpn_list(Span<const uint8_t> in);

bool ssl_is_alpn_protocol_allowed(const SSL_HANDSHAKE *hs,
                                  Span<const uint8_t> protocol);

bool ssl_alpn_list_contains_protocol(Span<const uint8_t> list,
                                     Span<const uint8_t> protocol);

bool ssl_negotiate_alpn(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                        const SSL_CLIENT_HELLO *client_hello);

bool ssl_get_local_application_settings(const SSL_HANDSHAKE *hs,
                                        Span<const uint8_t> *out_settings,
                                        Span<const uint8_t> protocol);

bool ssl_negotiate_alps(SSL_HANDSHAKE *hs, uint8_t *out_alert,
                        const SSL_CLIENT_HELLO *client_hello);

bool ssl_is_valid_trust_anchor_list(Span<const uint8_t> in);

struct SSLExtension {
  SSLExtension(uint16_t type_arg, bool allowed_arg = true)
      : type(type_arg), allowed(allowed_arg), present(false) {
    CBS_init(&data, nullptr, 0);
  }

  uint16_t type;
  bool allowed;
  bool present;
  CBS data;
};

bool ssl_parse_extensions(const CBS *cbs, uint8_t *out_alert,
                          std::initializer_list<SSLExtension *> extensions,
                          bool ignore_unknown);

enum ssl_verify_result_t ssl_verify_peer_cert(SSL_HANDSHAKE *hs);

enum ssl_verify_result_t ssl_reverify_peer_cert(SSL_HANDSHAKE *hs,
                                                bool send_alert);

enum ssl_hs_wait_t ssl_get_finished(SSL_HANDSHAKE *hs);

bool ssl_send_finished(SSL_HANDSHAKE *hs);

bool ssl_send_tls12_certificate(SSL_HANDSHAKE *hs);

const SSL_SESSION *ssl_handshake_session(const SSL_HANDSHAKE *hs);

void ssl_done_writing_client_hello(SSL_HANDSHAKE *hs);

using SSLFlags = uint32_t;
inline constexpr SSLFlags kSSLFlagResumptionAcrossNames = 1 << 8;

bool ssl_add_flags_extension(CBB *cbb, SSLFlags flags);

bool ssl_parse_flags_extension_request(const CBS *cbs, SSLFlags *out,
                                       uint8_t *out_alert);

bool ssl_parse_flags_extension_response(const CBS *cbs, SSLFlags *out,
                                        uint8_t *out_alert,
                                        SSLFlags allowed_flags);

bool ssl_log_secret(const SSL *ssl, const char *label,
                    Span<const uint8_t> secret);

bool ssl_parse_client_hello_with_trailing_data(const SSL *ssl, CBS *cbs,
                                               SSL_CLIENT_HELLO *out);

bool ssl_client_hello_get_extension(const SSL_CLIENT_HELLO *client_hello,
                                    CBS *out, uint16_t extension_type);

bool ssl_client_cipher_list_contains_cipher(
    const SSL_CLIENT_HELLO *client_hello, uint16_t id);

uint16_t ssl_get_grease_value(const SSL_HANDSHAKE *hs,
                              enum ssl_grease_index_t index);

bool tls1_parse_peer_sigalgs(SSL_HANDSHAKE *hs, const CBS *sigalgs);

bool tls1_get_legacy_signature_algorithm(uint16_t *out, const EVP_PKEY *pkey);

bool tls1_choose_signature_algorithm(SSL_HANDSHAKE *hs,
                                     const SSL_CREDENTIAL *cred, uint16_t *out);

bool tls12_add_verify_sigalgs(const SSL_HANDSHAKE *hs, CBB *out);

bool tls12_check_peer_sigalg(const SSL_HANDSHAKE *hs, uint8_t *out_alert,
                             uint16_t sigalg, EVP_PKEY *pkey);

#define TLSEXT_CHANNEL_ID_SIZE 128

#define NAMED_CURVE_TYPE 3

struct CERT {
  static constexpr bool kAllowUniquePtr = true;

  explicit CERT(const SSL_X509_METHOD *x509_method);
  ~CERT();

  bool is_valid() const { return legacy_credential != nullptr; }

  Vector<UniquePtr<SSL_CREDENTIAL>> credentials;

  UniquePtr<SSL_CREDENTIAL> legacy_credential;

  const SSL_X509_METHOD *x509_method = nullptr;

  STACK_OF(X509) *x509_chain = nullptr;

  X509 *x509_leaf = nullptr;

  X509 *x509_stash = nullptr;

  int (*cert_cb)(SSL *ssl, void *arg) = nullptr;
  void *cert_cb_arg = nullptr;

  X509_STORE *verify_store = nullptr;

  InplaceVector<uint8_t, SSL_MAX_SID_CTX_LENGTH> sid_ctx;
};

struct SSL_PROTOCOL_METHOD {
  bool is_dtls;
  bool (*ssl_new)(SSL *ssl);
  void (*ssl_free)(SSL *ssl);

  bool (*get_message)(const SSL *ssl, SSLMessage *out);

  void (*next_message)(SSL *ssl);

  bool (*has_unprocessed_handshake_data)(const SSL *ssl);

  ssl_open_record_t (*open_handshake)(SSL *ssl, size_t *out_consumed,
                                      uint8_t *out_alert, Span<uint8_t> in);

  ssl_open_record_t (*open_change_cipher_spec)(SSL *ssl, size_t *out_consumed,
                                               uint8_t *out_alert,
                                               Span<uint8_t> in);

  ssl_open_record_t (*open_app_data)(SSL *ssl, Span<uint8_t> *out,
                                     size_t *out_consumed, uint8_t *out_alert,
                                     Span<uint8_t> in);

  int (*write_app_data)(SSL *ssl, bool *out_needs_handshake,
                        size_t *out_bytes_written, Span<const uint8_t> in);
  int (*dispatch_alert)(SSL *ssl);

  bool (*init_message)(const SSL *ssl, CBB *cbb, CBB *body, uint8_t type);

  bool (*finish_message)(const SSL *ssl, CBB *cbb,
                         bssl::Array<uint8_t> *out_msg);

  bool (*add_message)(SSL *ssl, bssl::Array<uint8_t> msg);

  bool (*add_change_cipher_spec)(SSL *ssl);

  void (*finish_flight)(SSL *ssl);

  void (*schedule_ack)(SSL *ssl);

  int (*flush)(SSL *ssl);

  void (*on_handshake_complete)(SSL *ssl);

  bool (*set_read_state)(SSL *ssl, ssl_encryption_level_t level,
                         UniquePtr<SSLAEADContext> aead_ctx,
                         Span<const uint8_t> traffic_secret);

  bool (*set_write_state)(SSL *ssl, ssl_encryption_level_t level,
                          UniquePtr<SSLAEADContext> aead_ctx,
                          Span<const uint8_t> traffic_secret);
};

ssl_open_record_t ssl_open_handshake(SSL *ssl, size_t *out_consumed,
                                     uint8_t *out_alert, Span<uint8_t> in);

ssl_open_record_t ssl_open_change_cipher_spec(SSL *ssl, size_t *out_consumed,
                                              uint8_t *out_alert,
                                              Span<uint8_t> in);

ssl_open_record_t ssl_open_app_data(SSL *ssl, Span<uint8_t> *out,
                                    size_t *out_consumed, uint8_t *out_alert,
                                    Span<uint8_t> in);

struct SSL_X509_METHOD {

  bool (*check_CA_list)(STACK_OF(CRYPTO_BUFFER) *names);

  void (*cert_clear)(CERT *cert);

  void (*cert_free)(CERT *cert);

  void (*cert_dup)(CERT *new_cert, const CERT *cert);
  void (*cert_flush_cached_chain)(CERT *cert);

  void (*cert_flush_cached_leaf)(CERT *cert);

  bool (*session_cache_objects)(SSL_SESSION *session);

  bool (*session_dup)(SSL_SESSION *new_session, const SSL_SESSION *session);

  void (*session_clear)(SSL_SESSION *session);

  bool (*session_verify_cert_chain)(SSL_SESSION *session, SSL_HANDSHAKE *ssl,
                                    uint8_t *out_alert);

  void (*hs_flush_cached_ca_names)(SSL_HANDSHAKE *hs);

  bool (*ssl_new)(SSL_HANDSHAKE *hs);

  void (*ssl_config_free)(SSL_CONFIG *cfg);

  void (*ssl_flush_cached_client_CA)(SSL_CONFIG *cfg);

  bool (*ssl_auto_chain_if_needed)(SSL_HANDSHAKE *hs);

  bool (*ssl_ctx_new)(SSL_CTX *ctx);

  void (*ssl_ctx_free)(SSL_CTX *ctx);

  void (*ssl_ctx_flush_cached_client_CA)(SSL_CTX *ssl);
};

extern const SSL_X509_METHOD ssl_crypto_x509_method;

extern const SSL_X509_METHOD ssl_noop_x509_method;

struct TicketKey {
  static constexpr bool kAllowUniquePtr = true;

  uint8_t name[SSL_TICKET_KEY_NAME_LEN] = {0};
  uint8_t hmac_key[16] = {0};
  uint8_t aes_key[16] = {0};

  uint64_t next_rotation_tv_sec = 0;
};

struct CertCompressionAlg {
  static constexpr bool kAllowUniquePtr = true;

  ssl_cert_compression_func_t compress = nullptr;
  ssl_cert_decompression_func_t decompress = nullptr;
  uint16_t alg_id = 0;
};

BSSL_NAMESPACE_END

DEFINE_LHASH_OF(SSL_SESSION)

BSSL_NAMESPACE_BEGIN

enum ssl_shutdown_t {
  ssl_shutdown_none = 0,
  ssl_shutdown_close_notify = 1,
  ssl_shutdown_error = 2,
};

enum ssl_ech_status_t {

  ssl_ech_none,

  ssl_ech_accepted,

  ssl_ech_rejected,
};

struct SSL3_STATE {
  static constexpr bool kAllowUniquePtr = true;

  SSL3_STATE();
  ~SSL3_STATE();

  uint64_t read_sequence = 0;
  uint64_t write_sequence = 0;

  uint8_t server_random[SSL3_RANDOM_SIZE] = {0};
  uint8_t client_random[SSL3_RANDOM_SIZE] = {0};

  SSLBuffer read_buffer;

  SSLBuffer write_buffer;

  Span<uint8_t> pending_app_data;

  size_t unreported_bytes_written = 0;

  Span<const uint8_t> pending_write;

  uint8_t pending_write_type = 0;

  enum ssl_shutdown_t read_shutdown = ssl_shutdown_none;

  enum ssl_shutdown_t write_shutdown = ssl_shutdown_none;

  UniquePtr<ERR_SAVE_STATE> read_error;

  int total_renegotiations = 0;

  int rwstate = SSL_ERROR_NONE;

  enum ssl_encryption_level_t quic_read_level = ssl_encryption_initial;
  enum ssl_encryption_level_t quic_write_level = ssl_encryption_initial;

  uint16_t version = 0;

  uint16_t early_data_skipped = 0;

  uint8_t empty_record_count = 0;

  uint8_t warning_alert_count = 0;

  uint8_t key_update_count = 0;

  ssl_ech_status_t ech_status = ssl_ech_none;

  bool skip_early_data : 1;

  bool v2_hello_done : 1;

  bool is_v2_hello : 1;

  bool has_message : 1;

  bool initial_handshake_complete : 1;

  bool session_reused : 1;

  bool send_connection_binding : 1;

  bool channel_id_valid : 1;

  bool key_update_pending : 1;

  bool early_data_accepted : 1;

  bool alert_dispatch : 1;

  bool renegotiate_pending : 1;

  bool used_hello_retry_request : 1;

  bool was_key_usage_invalid : 1;

  UniquePtr<BUF_MEM> hs_buf;

  UniquePtr<BUF_MEM> pending_hs_data;

  UniquePtr<BUF_MEM> pending_flight;

  uint32_t pending_flight_offset = 0;

  int32_t ticket_age_skew = 0;

  enum ssl_early_data_reason_t early_data_reason = ssl_early_data_unknown;

  UniquePtr<SSLAEADContext> aead_read_ctx;

  UniquePtr<SSLAEADContext> aead_write_ctx;

  UniquePtr<SSL_HANDSHAKE> hs;

  InplaceVector<uint8_t, SSL_MAX_MD_SIZE> write_traffic_secret;
  InplaceVector<uint8_t, SSL_MAX_MD_SIZE> read_traffic_secret;
  InplaceVector<uint8_t, SSL_MAX_MD_SIZE> exporter_secret;

  InplaceVector<uint8_t, 12> previous_client_finished;
  InplaceVector<uint8_t, 12> previous_server_finished;

  uint8_t send_alert[2] = {0};

  UniquePtr<SSL_SESSION> established_session;

  Array<uint8_t> next_proto_negotiated;

  Array<uint8_t> alpn_selected;

  UniquePtr<char> hostname;

  uint8_t channel_id[64] = {0};

  Array<uint8_t> peer_quic_transport_params;

  const SRTP_PROTECTION_PROFILE *srtp_profile = nullptr;
};

#define DTLS1_RT_MAX_HEADER_LENGTH 13

#define DTLS_PLAINTEXT_RECORD_HEADER_LENGTH 13

#define DTLS1_3_RECORD_HEADER_WRITE_LENGTH 5

static_assert(DTLS1_RT_MAX_HEADER_LENGTH >= DTLS_PLAINTEXT_RECORD_HEADER_LENGTH,
              "DTLS1_RT_MAX_HEADER_LENGTH must not be smaller than defined "
              "record header lengths");
static_assert(DTLS1_RT_MAX_HEADER_LENGTH >= DTLS1_3_RECORD_HEADER_WRITE_LENGTH,
              "DTLS1_RT_MAX_HEADER_LENGTH must not be smaller than defined "
              "record header lengths");

#define DTLS1_HM_HEADER_LENGTH 12

class DTLSMessageBitmap {
 public:

  struct Range {
    size_t start = 0;
    size_t end = 0;

    bool empty() const { return start == end; }
    size_t size() const { return end - start; }
    bool operator==(const Range &r) const {
      return start == r.start && end == r.end;
    }
    bool operator!=(const Range &r) const { return !(*this == r); }
  };

  bool Init(size_t num_bits);

  void MarkRange(size_t start, size_t end);

  Range NextUnmarkedRange(size_t start) const;

  bool IsComplete() const { return bytes_.empty(); }

 private:

  Array<uint8_t> bytes_;

  size_t first_unmarked_byte_ = 0;
};

struct hm_header_st {
  uint8_t type;
  uint32_t msg_len;
  uint16_t seq;
  uint32_t frag_off;
  uint32_t frag_len;
};

struct DTLSIncomingMessage {
  static constexpr bool kAllowUniquePtr = true;

  Span<uint8_t> msg() { return Span(data).subspan(DTLS1_HM_HEADER_LENGTH); }
  Span<const uint8_t> msg() const {
    return Span(data).subspan(DTLS1_HM_HEADER_LENGTH);
  }
  size_t msg_len() const { return msg().size(); }

  uint8_t type = 0;

  uint16_t seq = 0;

  Array<uint8_t> data;

  DTLSMessageBitmap reassembly;
};

struct DTLSOutgoingMessage {
  size_t msg_len() const {
    assert(!is_ccs);
    assert(data.size() >= DTLS1_HM_HEADER_LENGTH);
    return data.size() - DTLS1_HM_HEADER_LENGTH;
  }

  bool IsFullyAcked() const {

    return !is_ccs && acked.IsComplete();
  }

  Array<uint8_t> data;
  uint16_t epoch = 0;
  bool is_ccs = false;

  DTLSMessageBitmap acked;
};

struct OPENSSL_timeval {
  uint64_t tv_sec;
  uint32_t tv_usec;
};

struct DTLSTimer {
 public:
  static constexpr uint64_t kNever = UINT64_MAX;

  void StartMicroseconds(OPENSSL_timeval now, uint64_t microseconds);

  void Stop();

  bool IsExpired(OPENSSL_timeval now) const;

  bool IsSet() const;

  uint64_t MicrosecondsRemaining(OPENSSL_timeval now) const;

 private:

  OPENSSL_timeval expire_time_ = {0, 0};
};

#define DTLS_MAX_EXTRA_WRITE_EPOCHS 2

#define DTLS_MAX_ACK_BUFFER 32

struct DTLSSentRecord {
  DTLSRecordNumber number;
  PackedSize<SSL_MAX_HANDSHAKE_FLIGHT> first_msg = 0;
  PackedSize<SSL_MAX_HANDSHAKE_FLIGHT> last_msg = 0;
  uint32_t first_msg_start = 0;
  uint32_t last_msg_end = 0;
};

enum class QueuedKeyUpdate {
  kNone,
  kUpdateNotRequested,
  kUpdateRequested,
};

#define DTLS_PREV_READ_EPOCH_EXPIRE_SECONDS (4 * 60)

struct DTLSPrevReadEpoch {
  static constexpr bool kAllowUniquePtr = true;
  DTLSReadEpoch epoch;

  uint64_t expire;
};

struct DTLS1_STATE {
  static constexpr bool kAllowUniquePtr = true;

  DTLS1_STATE();
  ~DTLS1_STATE();

  bool Init();

  bool has_change_cipher_spec : 1;

  bool outgoing_messages_complete : 1;

  bool flight_has_reply : 1;

  bool handshake_write_overflow : 1;
  bool handshake_read_overflow : 1;

  bool sending_flight : 1;
  bool sending_ack : 1;

  QueuedKeyUpdate queued_key_update : 2;

  uint16_t handshake_write_seq = 0;
  uint16_t handshake_read_seq = 0;

  DTLSReadEpoch read_epoch;

  UniquePtr<DTLSReadEpoch> next_read_epoch;

  UniquePtr<DTLSPrevReadEpoch> prev_read_epoch;

  DTLSWriteEpoch write_epoch;

  InplaceVector<UniquePtr<DTLSWriteEpoch>, DTLS_MAX_EXTRA_WRITE_EPOCHS>
      extra_write_epochs;

  UniquePtr<DTLSIncomingMessage> incoming_messages[SSL_MAX_HANDSHAKE_FLIGHT];

  InplaceVector<DTLSOutgoingMessage, SSL_MAX_HANDSHAKE_FLIGHT>
      outgoing_messages;

  UniquePtr<MRUQueue<DTLSSentRecord, DTLS_MAX_ACK_BUFFER>> sent_records;

  MRUQueue<DTLSRecordNumber, DTLS_MAX_ACK_BUFFER> records_to_ack;

  uint8_t outgoing_written = 0;

  uint32_t outgoing_offset = 0;

  unsigned mtu = 0;

  unsigned num_timeouts = 0;

  DTLSTimer retransmit_timer;

  DTLSTimer ack_timer;

  uint32_t timeout_duration_ms = 0;
};

struct ALPSConfig {
  Array<uint8_t> protocol;
  Array<uint8_t> settings;
};

struct SSL_CONFIG {
  static constexpr bool kAllowUniquePtr = true;

  explicit SSL_CONFIG(SSL *ssl_arg);
  ~SSL_CONFIG();

  SSL *const ssl = nullptr;

  uint16_t conf_max_version = 0;

  uint16_t conf_min_version = 0;

  X509_VERIFY_PARAM *param = nullptr;

  UniquePtr<SSLCipherPreferenceList> cipher_list;

  UniquePtr<CERT> cert;

  int (*verify_callback)(int ok,
                         X509_STORE_CTX *ctx) =
      nullptr;

  enum ssl_verify_result_t (*custom_verify_callback)(
      SSL *ssl, uint8_t *out_alert) = nullptr;

  UniquePtr<char> psk_identity_hint;

  unsigned (*psk_client_callback)(SSL *ssl, const char *hint, char *identity,
                                  unsigned max_identity_len, uint8_t *psk,
                                  unsigned max_psk_len) = nullptr;
  unsigned (*psk_server_callback)(SSL *ssl, const char *identity, uint8_t *psk,
                                  unsigned max_psk_len) = nullptr;

  UniquePtr<STACK_OF(CRYPTO_BUFFER)> client_CA;

  STACK_OF(X509_NAME) *cached_x509_client_CA = nullptr;

  UniquePtr<STACK_OF(CRYPTO_BUFFER)> CA_names;

  std::optional<Array<uint8_t>> requested_trust_anchors;

  Array<uint16_t> supported_group_list;

  UniquePtr<EVP_PKEY> channel_id_private;

  Array<uint8_t> alpn_client_proto_list;

  Vector<ALPSConfig> alps_configs;

  Array<uint8_t> quic_transport_params;

  Array<uint8_t> quic_early_data_context;

  Array<uint16_t> verify_sigalgs;

  UniquePtr<STACK_OF(SRTP_PROTECTION_PROFILE)> srtp_profiles;

  Array<uint8_t> client_ech_config_list;

  enum ssl_compliance_policy_t compliance_policy = ssl_compliance_policy_none;

  uint8_t verify_mode = SSL_VERIFY_NONE;

  bool ech_grease_enabled : 1;

  bool signed_cert_timestamps_enabled : 1;

  bool ocsp_stapling_enabled : 1;

  bool channel_id_enabled : 1;

  bool enforce_rsa_key_usage : 1;

  bool retain_only_sha256_of_client_certs : 1;

  bool handoff : 1;

  bool shed_handshake_config : 1;

  bool jdk11_workaround : 1;

  bool quic_use_legacy_codepoint : 1;

  bool permute_extensions : 1;

  bool aes_hw_override : 1;

  bool aes_hw_override_value : 1;

  bool alps_use_new_codepoint : 1;
};

#define SSL_PSK_DHE_KE 0x1

static const size_t kMaxEarlyDataAccepted = 14336;

UniquePtr<CERT> ssl_cert_dup(CERT *cert);
bool ssl_set_cert(CERT *cert, UniquePtr<CRYPTO_BUFFER> buffer);
bool ssl_is_key_type_supported(int key_type);

bool ssl_compare_public_and_private_key(const EVP_PKEY *pubkey,
                                        const EVP_PKEY *privkey);
bool ssl_get_new_session(SSL_HANDSHAKE *hs);

bool ssl_encrypt_ticket(SSL_HANDSHAKE *hs, CBB *out,
                        const SSL_SESSION *session);

bool ssl_ctx_rotate_ticket_encryption_key(SSL_CTX *ctx);

UniquePtr<SSL_SESSION> ssl_session_new(const SSL_X509_METHOD *x509_method);

uint32_t ssl_hash_session_id(Span<const uint8_t> session_id);

OPENSSL_EXPORT UniquePtr<SSL_SESSION> SSL_SESSION_parse(
    CBS *cbs, const SSL_X509_METHOD *x509_method, CRYPTO_BUFFER_POOL *pool);

OPENSSL_EXPORT bool ssl_session_serialize(const SSL_SESSION *in, CBB *cbb);

enum class SSLSessionType {

  kNotResumable,

  kID,

  kTicket,

  kPreSharedKey,
};

SSLSessionType ssl_session_get_type(const SSL_SESSION *session);

bool ssl_session_is_context_valid(const SSL_HANDSHAKE *hs,
                                  const SSL_SESSION *session);

bool ssl_session_is_time_valid(const SSL *ssl, const SSL_SESSION *session);

bool ssl_session_is_resumable(const SSL_HANDSHAKE *hs,
                              const SSL_SESSION *session);

uint16_t ssl_session_protocol_version(const SSL_SESSION *session);

const EVP_MD *ssl_session_get_digest(const SSL_SESSION *session);

void ssl_set_session(SSL *ssl, SSL_SESSION *session);

enum ssl_hs_wait_t ssl_get_prev_session(SSL_HANDSHAKE *hs,
                                        UniquePtr<SSL_SESSION> *out_session,
                                        bool *out_tickets_supported,
                                        bool *out_renew_ticket,
                                        const SSL_CLIENT_HELLO *client_hello);

#define SSL_SESSION_DUP_AUTH_ONLY 0x0
#define SSL_SESSION_INCLUDE_TICKET 0x1
#define SSL_SESSION_INCLUDE_NONAUTH 0x2
#define SSL_SESSION_DUP_ALL \
  (SSL_SESSION_INCLUDE_TICKET | SSL_SESSION_INCLUDE_NONAUTH)

OPENSSL_EXPORT UniquePtr<SSL_SESSION> SSL_SESSION_dup(SSL_SESSION *session,
                                                      int dup_flags);

void ssl_session_rebase_time(SSL *ssl, SSL_SESSION *session);

void ssl_session_renew_timeout(SSL *ssl, SSL_SESSION *session,
                               uint32_t timeout);

void ssl_update_cache(SSL *ssl);

void ssl_send_alert(SSL *ssl, int level, int desc);
int ssl_send_alert_impl(SSL *ssl, int level, int desc);
bool tls_get_message(const SSL *ssl, SSLMessage *out);
ssl_open_record_t tls_open_handshake(SSL *ssl, size_t *out_consumed,
                                     uint8_t *out_alert, Span<uint8_t> in);
void tls_next_message(SSL *ssl);

int tls_dispatch_alert(SSL *ssl);
ssl_open_record_t tls_open_app_data(SSL *ssl, Span<uint8_t> *out,
                                    size_t *out_consumed, uint8_t *out_alert,
                                    Span<uint8_t> in);
ssl_open_record_t tls_open_change_cipher_spec(SSL *ssl, size_t *out_consumed,
                                              uint8_t *out_alert,
                                              Span<uint8_t> in);
int tls_write_app_data(SSL *ssl, bool *out_needs_handshake,
                       size_t *out_bytes_written, Span<const uint8_t> in);

bool tls_new(SSL *ssl);
void tls_free(SSL *ssl);

bool tls_init_message(const SSL *ssl, CBB *cbb, CBB *body, uint8_t type);
bool tls_finish_message(const SSL *ssl, CBB *cbb, Array<uint8_t> *out_msg);
bool tls_add_message(SSL *ssl, Array<uint8_t> msg);
bool tls_add_change_cipher_spec(SSL *ssl);
int tls_flush(SSL *ssl);

bool dtls1_init_message(const SSL *ssl, CBB *cbb, CBB *body, uint8_t type);
bool dtls1_finish_message(const SSL *ssl, CBB *cbb, Array<uint8_t> *out_msg);
bool dtls1_add_message(SSL *ssl, Array<uint8_t> msg);
bool dtls1_add_change_cipher_spec(SSL *ssl);
void dtls1_finish_flight(SSL *ssl);
void dtls1_schedule_ack(SSL *ssl);
int dtls1_flush(SSL *ssl);

bool ssl_add_message_cbb(SSL *ssl, CBB *cbb);

bool ssl_hash_message(SSL_HANDSHAKE *hs, const SSLMessage &msg);

ssl_open_record_t dtls1_process_ack(SSL *ssl, uint8_t *out_alert,
                                    DTLSRecordNumber ack_record_number,
                                    Span<const uint8_t> data);
ssl_open_record_t dtls1_open_app_data(SSL *ssl, Span<uint8_t> *out,
                                      size_t *out_consumed, uint8_t *out_alert,
                                      Span<uint8_t> in);
ssl_open_record_t dtls1_open_change_cipher_spec(SSL *ssl, size_t *out_consumed,
                                                uint8_t *out_alert,
                                                Span<uint8_t> in);

int dtls1_write_app_data(SSL *ssl, bool *out_needs_handshake,
                         size_t *out_bytes_written, Span<const uint8_t> in);

int dtls1_write_record(SSL *ssl, int type, Span<const uint8_t> in,
                       uint16_t epoch);

bool dtls1_parse_fragment(CBS *cbs, struct hm_header_st *out_hdr,
                          CBS *out_body);

#define DTLS1_MTU_TIMEOUTS 2

#define DTLS1_MAX_TIMEOUTS 12

void dtls1_stop_timer(SSL *ssl);

unsigned int dtls1_min_mtu(void);

bool dtls1_new(SSL *ssl);
void dtls1_free(SSL *ssl);

bool dtls1_process_handshake_fragments(SSL *ssl, uint8_t *out_alert,
                                       DTLSRecordNumber record_number,
                                       Span<const uint8_t> record);
bool dtls1_get_message(const SSL *ssl, SSLMessage *out);
ssl_open_record_t dtls1_open_handshake(SSL *ssl, size_t *out_consumed,
                                       uint8_t *out_alert, Span<uint8_t> in);
void dtls1_next_message(SSL *ssl);
int dtls1_dispatch_alert(SSL *ssl);

bool tls1_configure_aead(SSL *ssl, evp_aead_direction_t direction,
                         Array<uint8_t> *key_block_cache,
                         const SSL_SESSION *session,
                         Span<const uint8_t> iv_override);

bool tls1_change_cipher_state(SSL_HANDSHAKE *hs,
                              evp_aead_direction_t direction);

bool tls1_generate_master_secret(SSL_HANDSHAKE *hs, Span<uint8_t> out,
                                 Span<const uint8_t> premaster);

Span<const uint16_t> tls1_get_grouplist(const SSL_HANDSHAKE *ssl);

bool tls1_check_group_id(const SSL_HANDSHAKE *ssl, uint16_t group_id);

bool tls1_get_shared_group(SSL_HANDSHAKE *hs, uint16_t *out_group_id);

bool ssl_add_clienthello_tlsext(SSL_HANDSHAKE *hs, CBB *out, CBB *out_encoded,
                                bool *out_needs_psk_binder,
                                ssl_client_hello_type_t type,
                                size_t header_len);

bool ssl_add_serverhello_tlsext(SSL_HANDSHAKE *hs, CBB *out);
bool ssl_parse_clienthello_tlsext(SSL_HANDSHAKE *hs,
                                  const SSL_CLIENT_HELLO *client_hello);
bool ssl_parse_serverhello_tlsext(SSL_HANDSHAKE *hs, const CBS *extensions);

#define tlsext_tick_md EVP_sha256

enum ssl_ticket_aead_result_t ssl_process_ticket(
    SSL_HANDSHAKE *hs, UniquePtr<SSL_SESSION> *out_session,
    bool *out_renew_ticket, Span<const uint8_t> ticket,
    Span<const uint8_t> session_id);

bool tls1_verify_channel_id(SSL_HANDSHAKE *hs, const SSLMessage &msg);

bool tls1_write_channel_id(SSL_HANDSHAKE *hs, CBB *cbb);

bool tls1_channel_id_hash(SSL_HANDSHAKE *hs, uint8_t *out, size_t *out_len);

bool tls1_record_handshake_hashes_for_channel_id(SSL_HANDSHAKE *hs);

bool ssl_can_write(const SSL *ssl);

bool ssl_can_read(const SSL *ssl);

OPENSSL_timeval ssl_ctx_get_current_time(const SSL_CTX *ctx);

void ssl_reset_error_state(SSL *ssl);

void ssl_set_read_error(SSL *ssl);

BSSL_NAMESPACE_END

struct ssl_method_st {

  uint16_t version;

  const bssl::SSL_PROTOCOL_METHOD *method;

  const bssl::SSL_X509_METHOD *x509_method;
};

struct ssl_ctx_st : public bssl::RefCounted<ssl_ctx_st> {
  explicit ssl_ctx_st(const SSL_METHOD *ssl_method);
  ssl_ctx_st(const ssl_ctx_st &) = delete;
  ssl_ctx_st &operator=(const ssl_ctx_st &) = delete;

  const bssl::SSL_PROTOCOL_METHOD *method = nullptr;
  const bssl::SSL_X509_METHOD *x509_method = nullptr;

  CRYPTO_MUTEX lock;

  uint16_t conf_max_version = 0;

  uint16_t conf_min_version = 0;

  uint8_t num_tickets = 2;

  const SSL_QUIC_METHOD *quic_method = nullptr;

  bssl::UniquePtr<bssl::SSLCipherPreferenceList> cipher_list;

  X509_STORE *cert_store = nullptr;
  LHASH_OF(SSL_SESSION) *sessions = nullptr;

  unsigned long session_cache_size = SSL_SESSION_CACHE_MAX_SIZE_DEFAULT;
  SSL_SESSION *session_cache_head = nullptr;
  SSL_SESSION *session_cache_tail = nullptr;

  int handshakes_since_cache_flush = 0;

  int session_cache_mode = SSL_SESS_CACHE_SERVER;

  uint32_t session_timeout = SSL_DEFAULT_SESSION_TIMEOUT;

  uint32_t session_psk_dhe_timeout = SSL_DEFAULT_SESSION_PSK_DHE_TIMEOUT;

  int (*new_session_cb)(SSL *ssl, SSL_SESSION *sess) = nullptr;
  void (*remove_session_cb)(SSL_CTX *ctx, SSL_SESSION *sess) = nullptr;
  SSL_SESSION *(*get_session_cb)(SSL *ssl, const uint8_t *data, int len,
                                 int *copy) = nullptr;

  int (*app_verify_callback)(X509_STORE_CTX *store_ctx, void *arg) = nullptr;
  void *app_verify_arg = nullptr;

  ssl_verify_result_t (*custom_verify_callback)(SSL *ssl,
                                                uint8_t *out_alert) = nullptr;

  pem_password_cb *default_passwd_callback = nullptr;

  void *default_passwd_callback_userdata = nullptr;

  int (*client_cert_cb)(SSL *ssl, X509 **out_x509,
                        EVP_PKEY **out_pkey) = nullptr;

  CRYPTO_EX_DATA ex_data;

  void (*info_callback)(const SSL *ssl, int type, int value) = nullptr;

  bssl::UniquePtr<STACK_OF(CRYPTO_BUFFER)> client_CA;

  STACK_OF(X509_NAME) *cached_x509_client_CA = nullptr;

  bssl::UniquePtr<STACK_OF(CRYPTO_BUFFER)> CA_names;

  std::optional<bssl::Array<uint8_t>> requested_trust_anchors;

  uint32_t options = 0;

  uint32_t mode = SSL_MODE_NO_AUTO_CHAIN;
  uint32_t max_cert_list = SSL_MAX_CERT_LIST_DEFAULT;

  bssl::UniquePtr<bssl::CERT> cert;

  void (*msg_callback)(int is_write, int version, int content_type,
                       const void *buf, size_t len, SSL *ssl,
                       void *arg) = nullptr;
  void *msg_callback_arg = nullptr;

  int verify_mode = SSL_VERIFY_NONE;
  int (*default_verify_callback)(int ok, X509_STORE_CTX *ctx) =
      nullptr;

  X509_VERIFY_PARAM *param = nullptr;

  ssl_select_cert_result_t (*select_certificate_cb)(const SSL_CLIENT_HELLO *) =
      nullptr;

  int (*dos_protection_cb)(const SSL_CLIENT_HELLO *) = nullptr;

  bool reverify_on_resume = false;

  uint16_t max_send_fragment = SSL3_RT_MAX_PLAIN_LENGTH;

  int (*servername_callback)(SSL *, int *, void *) = nullptr;
  void *servername_arg = nullptr;

  bssl::UniquePtr<bssl::TicketKey> ticket_key_current;
  bssl::UniquePtr<bssl::TicketKey> ticket_key_prev;

  int (*ticket_key_cb)(SSL *ssl, uint8_t *name, uint8_t *iv,
                       EVP_CIPHER_CTX *ectx, HMAC_CTX *hctx, int enc) = nullptr;

  bssl::UniquePtr<char> psk_identity_hint;

  unsigned (*psk_client_callback)(SSL *ssl, const char *hint, char *identity,
                                  unsigned max_identity_len, uint8_t *psk,
                                  unsigned max_psk_len) = nullptr;
  unsigned (*psk_server_callback)(SSL *ssl, const char *identity, uint8_t *psk,
                                  unsigned max_psk_len) = nullptr;

  int (*next_protos_advertised_cb)(SSL *ssl, const uint8_t **out,
                                   unsigned *out_len, void *arg) = nullptr;
  void *next_protos_advertised_cb_arg = nullptr;

  int (*next_proto_select_cb)(SSL *ssl, uint8_t **out, uint8_t *out_len,
                              const uint8_t *in, unsigned in_len,
                              void *arg) = nullptr;
  void *next_proto_select_cb_arg = nullptr;

  int (*alpn_select_cb)(SSL *ssl, const uint8_t **out, uint8_t *out_len,
                        const uint8_t *in, unsigned in_len,
                        void *arg) = nullptr;
  void *alpn_select_cb_arg = nullptr;

  bssl::Array<uint8_t> alpn_client_proto_list;

  bssl::UniquePtr<STACK_OF(SRTP_PROTECTION_PROFILE)> srtp_profiles;

  bssl::Vector<bssl::CertCompressionAlg> cert_compression_algs;

  bssl::Array<uint16_t> supported_group_list;

  bssl::UniquePtr<EVP_PKEY> channel_id_private;

  bssl::UniquePtr<SSL_ECH_KEYS> ech_keys;

  void (*keylog_callback)(const SSL *ssl, const char *line) = nullptr;

  void (*current_time_cb)(const SSL *ssl, struct timeval *out_clock) = nullptr;

  CRYPTO_BUFFER_POOL *pool = nullptr;

  const SSL_TICKET_AEAD_METHOD *ticket_aead_method = nullptr;

  int (*legacy_ocsp_callback)(SSL *ssl, void *arg) = nullptr;
  void *legacy_ocsp_callback_arg = nullptr;

  enum ssl_compliance_policy_t compliance_policy = ssl_compliance_policy_none;

  bssl::Array<uint16_t> verify_sigalgs;

  bool retain_only_sha256_of_client_certs : 1;

  bool quiet_shutdown : 1;

  bool ocsp_stapling_enabled : 1;

  bool signed_cert_timestamps_enabled : 1;

  bool channel_id_enabled : 1;

  bool grease_enabled : 1;

  bool permute_extensions : 1;

  bool allow_unknown_alpn_protos : 1;

  bool false_start_allowed_without_alpn : 1;

  bool handoff : 1;

  bool enable_early_data : 1;

  bool aes_hw_override : 1;

  bool aes_hw_override_value : 1;

  bool resumption_across_names_enabled : 1;

 private:
  friend RefCounted;
  ~ssl_ctx_st();
};

struct ssl_st {
  explicit ssl_st(SSL_CTX *ctx_arg);
  ssl_st(const ssl_st &) = delete;
  ssl_st &operator=(const ssl_st &) = delete;
  ~ssl_st();

  const bssl::SSL_PROTOCOL_METHOD *method = nullptr;

  bssl::UniquePtr<bssl::SSL_CONFIG> config;

  uint16_t max_send_fragment = 0;

  bssl::UniquePtr<BIO> rbio;
  bssl::UniquePtr<BIO> wbio;

  bssl::ssl_hs_wait_t (*do_handshake)(bssl::SSL_HANDSHAKE *hs) = nullptr;

  bssl::SSL3_STATE *s3 = nullptr;
  bssl::DTLS1_STATE *d1 = nullptr;

  void (*msg_callback)(int write_p, int version, int content_type,
                       const void *buf, size_t len, SSL *ssl,
                       void *arg) = nullptr;
  void *msg_callback_arg = nullptr;

  uint32_t initial_timeout_duration_ms = 400;

  bssl::UniquePtr<SSL_SESSION> session;

  void (*info_callback)(const SSL *ssl, int type, int value) = nullptr;

  bssl::UniquePtr<SSL_CTX> ctx;

  bssl::UniquePtr<SSL_CTX> session_ctx;

  CRYPTO_EX_DATA ex_data;

  uint32_t options = 0;
  uint32_t mode = 0;
  uint32_t max_cert_list = 0;
  bssl::UniquePtr<char> hostname;

  const SSL_QUIC_METHOD *quic_method = nullptr;

  ssl_renegotiate_mode_t renegotiate_mode = ssl_renegotiate_never;

  bool server : 1;

  bool quiet_shutdown : 1;

  bool enable_early_data : 1;

  bool resumption_across_names_enabled : 1;
};

struct ssl_session_st : public bssl::RefCounted<ssl_session_st> {
  explicit ssl_session_st(const bssl::SSL_X509_METHOD *method);
  ssl_session_st(const ssl_session_st &) = delete;
  ssl_session_st &operator=(const ssl_session_st &) = delete;

  uint16_t ssl_version = 0;

  uint16_t group_id = 0;

  uint16_t peer_signature_algorithm = 0;

  bssl::InplaceVector<uint8_t, SSL_MAX_MASTER_KEY_LENGTH> secret;

  bssl::InplaceVector<uint8_t, SSL_MAX_SSL_SESSION_ID_LENGTH> session_id;

  bssl::InplaceVector<uint8_t, SSL_MAX_SID_CTX_LENGTH> sid_ctx;

  bssl::UniquePtr<char> psk_identity;

  bssl::UniquePtr<STACK_OF(CRYPTO_BUFFER)> certs;

  const bssl::SSL_X509_METHOD *x509_method = nullptr;

  X509 *x509_peer = nullptr;

  STACK_OF(X509) *x509_chain = nullptr;

  STACK_OF(X509) *x509_chain_without_leaf = nullptr;

  long verify_result = X509_V_ERR_INVALID_CALL;

  uint32_t timeout = SSL_DEFAULT_SESSION_TIMEOUT;

  uint32_t auth_timeout = SSL_DEFAULT_SESSION_TIMEOUT;

  uint64_t time = 0;

  const SSL_CIPHER *cipher = nullptr;

  CRYPTO_EX_DATA ex_data;

  SSL_SESSION *prev = nullptr, *next = nullptr;

  bssl::Array<uint8_t> ticket;

  bssl::UniquePtr<CRYPTO_BUFFER> signed_cert_timestamp_list;

  bssl::UniquePtr<CRYPTO_BUFFER> ocsp_response;

  uint8_t peer_sha256[SHA256_DIGEST_LENGTH] = {0};

  bssl::InplaceVector<uint8_t, SSL_MAX_MD_SIZE> original_handshake_hash;

  uint32_t ticket_lifetime_hint = 0;

  uint32_t ticket_age_add = 0;

  uint32_t ticket_max_early_data = 0;

  bssl::Array<uint8_t> early_alpn;

  bssl::Array<uint8_t> local_application_settings;

  bssl::Array<uint8_t> peer_application_settings;

  bool extended_master_secret : 1;

  bool peer_sha256_valid : 1;

  bool not_resumable : 1;

  bool ticket_age_add_valid : 1;

  bool is_server : 1;

  bool is_quic : 1;

  bool has_application_settings : 1;

  bool is_resumable_across_names : 1;

  bssl::Array<uint8_t> quic_early_data_context;

 private:
  friend RefCounted;
  ~ssl_session_st();
};

struct ssl_ech_keys_st : public bssl::RefCounted<ssl_ech_keys_st> {
  ssl_ech_keys_st() : RefCounted(CheckSubClass()) {}

  bssl::Vector<bssl::UniquePtr<bssl::ECHServerConfig>> configs;

 private:
  friend RefCounted;
  ~ssl_ech_keys_st() = default;
};

#endif
