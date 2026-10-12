#ifndef BSSL_VERIFY_H_
#define BSSL_VERIFY_H_

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <openssl/pki/signature_verify_cache.h>
#include <openssl/pki/verify_error.h>

BSSL_NAMESPACE_BEGIN

class CertIssuerSourceStatic;
class TrustStoreInMemory;
class CertificateVerifyOptions;
class CertificateVerifyStatus;

class OPENSSL_EXPORT VerifyTrustStore {
 public:
  std::unique_ptr<TrustStoreInMemory> trust_store;

  ~VerifyTrustStore();

  static std::unique_ptr<VerifyTrustStore> FromDER(
      std::string_view der_certs, std::string *out_diagnostic);

  static std::unique_ptr<VerifyTrustStore> FromDER(
      const std::vector<std::string_view> &der_certs,
      std::string *out_diagnostic);
};

class OPENSSL_EXPORT CertPool {
 public:
  CertPool();
  CertPool(const CertPool &) = delete;
  CertPool &operator=(const CertPool &) = delete;
  virtual ~CertPool();

  static std::unique_ptr<CertPool> FromCerts(
      const std::vector<std::string_view> &der_certs,
      std::string *out_diagnostic);

 private:
  friend std::optional<std::vector<std::vector<std::string>>>
  CertificateVerifyInternal(const CertificateVerifyOptions &opts,
                            VerifyError *out_error,
                            CertificateVerifyStatus *out_status,
                            bool all_paths);
  std::unique_ptr<CertIssuerSourceStatic> impl_;
};

class OPENSSL_EXPORT CertificateVerifyOptions {
 public:

  enum class KeyPurpose {
    ANY_EKU,
    SERVER_AUTH,
    CLIENT_AUTH,
    SERVER_AUTH_STRICT,
    CLIENT_AUTH_STRICT,
    SERVER_AUTH_STRICT_LEAF,
    CLIENT_AUTH_STRICT_LEAF,
    RCS_MLS_CLIENT_AUTH,
  };

  CertificateVerifyOptions();
  CertificateVerifyOptions(const CertificateVerifyOptions &) = delete;
  CertificateVerifyOptions &operator=(const CertificateVerifyOptions &) =
      delete;

  KeyPurpose key_purpose = KeyPurpose::SERVER_AUTH;
  std::string_view leaf_cert;
  std::vector<std::string_view> intermediates;

  const CertPool *extra_intermediates = nullptr;

  const VerifyTrustStore *trust_store = nullptr;

  size_t min_rsa_modulus_length = 1024;

  std::optional<int64_t> time;

  bool insecurely_allow_sha1 = false;

  uint32_t max_iteration_count = 0;

  std::optional<std::chrono::time_point<std::chrono::steady_clock>> deadline;

  uint32_t max_path_building_depth = 0;

  SignatureVerifyCache *signature_verify_cache = nullptr;
};

class OPENSSL_EXPORT CertificateVerifyStatus {
 public:
  CertificateVerifyStatus();

  size_t IterationCount() const;

  size_t MaxDepthSeen() const;

 private:
  friend std::optional<std::vector<std::vector<std::string>>>
  CertificateVerifyInternal(const CertificateVerifyOptions &opts,
                            VerifyError *out_error,
                            CertificateVerifyStatus *out_status,
                            bool all_paths);
  size_t iteration_count_ = 0;
  size_t max_depth_seen_ = 0;
};

OPENSSL_EXPORT std::optional<std::vector<std::string>> CertificateVerify(
    const CertificateVerifyOptions &opts, VerifyError *out_error = nullptr,
    CertificateVerifyStatus *out_status = nullptr);

OPENSSL_EXPORT std::optional<std::vector<std::vector<std::string>>>
CertificateVerifyAllPaths(const CertificateVerifyOptions &opts);

BSSL_NAMESPACE_END

#endif
