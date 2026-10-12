// Copyright 2016 The Chromium Authors
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

#include "path_builder.h"

#include <algorithm>

#include "cert_error_params.h"
#include "cert_issuer_source_static.h"
#include "common_cert_errors.h"
#include "input.h"
#include "mock_signature_verify_cache.h"
#include "parsed_certificate.h"
#include "simple_path_builder_delegate.h"
#include "test_helpers.h"
#include "trust_store_collection.h"
#include "trust_store_in_memory.h"
#include "verify_certificate_chain.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <openssl/pool.h>

BSSL_NAMESPACE_BEGIN

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::StrictMock;

class TestPathBuilderDelegate : public SimplePathBuilderDelegate {
 public:
  TestPathBuilderDelegate(size_t min_rsa_modulus_length_bits,
                          DigestPolicy digest_policy)
      : SimplePathBuilderDelegate(min_rsa_modulus_length_bits, digest_policy) {}

  bool IsDeadlineExpired() override { return deadline_is_expired_; }

  void SetDeadlineExpiredForTesting(bool deadline_is_expired) {
    deadline_is_expired_ = deadline_is_expired;
  }

  SignatureVerifyCache *GetVerifyCache() override {
    return use_signature_cache_ ? &cache_ : nullptr;
  }

  void ActivateCache() { use_signature_cache_ = true; }

  void DeActivateCache() { use_signature_cache_ = false; }

  MockSignatureVerifyCache *GetMockVerifyCache() { return &cache_; }

  void AllowPrecert() { allow_precertificate_ = true; }

  void DisallowPrecert() { allow_precertificate_ = false; }

  bool AcceptPreCertificates() override {
    return allow_precertificate_;
  }

private:
  bool deadline_is_expired_ = false;
  bool use_signature_cache_ = false;
  bool allow_precertificate_ = false;
  MockSignatureVerifyCache cache_;
};

class CertPathBuilderDelegateBase : public SimplePathBuilderDelegate {
 public:
  CertPathBuilderDelegateBase()
      : SimplePathBuilderDelegate(
            1024, SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1) {}
  void CheckPathAfterVerification(const CertPathBuilder &path_builder,
                                  CertPathBuilderResultPath *path) override {
    ADD_FAILURE() << "Tests must override this";
  }
};

class MockPathBuilderDelegate : public CertPathBuilderDelegateBase {
 public:
  MOCK_METHOD2(CheckPathAfterVerification,
               void(const CertPathBuilder &path_builder,
                    CertPathBuilderResultPath *path));
};

class AsyncCertIssuerSourceStatic : public CertIssuerSource {
 public:
  class StaticAsyncRequest : public Request {
   public:
    explicit StaticAsyncRequest(ParsedCertificateList &&issuers) {
      issuers_.swap(issuers);
      issuers_iter_ = issuers_.begin();
    }

    StaticAsyncRequest(const StaticAsyncRequest &) = delete;
    StaticAsyncRequest &operator=(const StaticAsyncRequest &) = delete;

    ~StaticAsyncRequest() override = default;

    void GetNext(ParsedCertificateList *out_certs) override {
      if (issuers_iter_ != issuers_.end()) {
        out_certs->push_back(std::move(*issuers_iter_++));
      }
    }

    ParsedCertificateList issuers_;
    ParsedCertificateList::iterator issuers_iter_;
  };

  ~AsyncCertIssuerSourceStatic() override = default;

  void SetAsyncGetCallback(std::function<void()> closure) {
    async_get_callback_ = std::move(closure);
  }

  void AddCert(std::shared_ptr<const ParsedCertificate> cert) {
    static_cert_issuer_source_.AddCert(std::move(cert));
  }

  void SyncGetIssuersOf(const ParsedCertificate *cert,
                        ParsedCertificateList *issuers) override {}
  void AsyncGetIssuersOf(const ParsedCertificate *cert,
                         std::unique_ptr<Request> *out_req) override {
    num_async_gets_++;
    ParsedCertificateList issuers;
    static_cert_issuer_source_.SyncGetIssuersOf(cert, &issuers);
    auto req = std::make_unique<StaticAsyncRequest>(std::move(issuers));
    *out_req = std::move(req);
    if (async_get_callback_) {
      async_get_callback_();
    }
  }
  int num_async_gets() const { return num_async_gets_; }

 private:
  CertIssuerSourceStatic static_cert_issuer_source_;

  int num_async_gets_ = 0;
  std::function<void()> async_get_callback_ = nullptr;
};

::testing::AssertionResult ReadTestPem(const std::string &file_name,
                                       const std::string &block_name,
                                       std::string *result) {
  const PemBlockMapping mappings[] = {
      {block_name.c_str(), result},
  };

  return ReadTestDataFromPemFile(file_name, mappings);
}

::testing::AssertionResult ReadTestCert(
    const std::string &file_name,
    std::shared_ptr<const ParsedCertificate> *result) {
  std::string der;
  ::testing::AssertionResult r = ReadTestPem(
      "testdata/path_builder_unittest/" + file_name, "CERTIFICATE", &der);
  if (!r) {
    return r;
  }
  CertErrors errors;
  *result = ParsedCertificate::Create(
      bssl::UniquePtr<CRYPTO_BUFFER>(CRYPTO_BUFFER_new(
          reinterpret_cast<const uint8_t *>(der.data()), der.size(), nullptr)),
      {}, &errors);
  if (!*result) {
    return ::testing::AssertionFailure()
           << "ParseCertificate::Create() failed:\n"
           << errors.ToDebugString();
  }
  return ::testing::AssertionSuccess();
}

class PathBuilderMultiRootTest : public ::testing::Test {
 public:
  PathBuilderMultiRootTest()
      : delegate_(1024, TestPathBuilderDelegate::DigestPolicy::kWeakAllowSha1) {
  }

  void SetUp() override {
    ASSERT_TRUE(ReadTestCert("multi-root-A-by-B.pem", &a_by_b_));
    ASSERT_TRUE(ReadTestCert("multi-root-B-by-C.pem", &b_by_c_));
    ASSERT_TRUE(ReadTestCert("multi-root-B-by-F.pem", &b_by_f_));
    ASSERT_TRUE(ReadTestCert("multi-root-C-by-D.pem", &c_by_d_));
    ASSERT_TRUE(ReadTestCert("multi-root-C-by-E.pem", &c_by_e_));
    ASSERT_TRUE(ReadTestCert("multi-root-D-by-D.pem", &d_by_d_));
    ASSERT_TRUE(ReadTestCert("multi-root-E-by-E.pem", &e_by_e_));
    ASSERT_TRUE(ReadTestCert("multi-root-F-by-E.pem", &f_by_e_));
  }

 protected:
  std::shared_ptr<const ParsedCertificate> a_by_b_, b_by_c_, b_by_f_, c_by_d_,
      c_by_e_, d_by_d_, e_by_e_, f_by_e_;

  TestPathBuilderDelegate delegate_;
  der::GeneralizedTime time_ = {2017, 3, 1, 0, 0, 0};

  const InitialExplicitPolicy initial_explicit_policy_ =
      InitialExplicitPolicy::kFalse;
  const std::set<der::Input> user_initial_policy_set_ = {
      der::Input(kAnyPolicyOid)};
  const InitialPolicyMappingInhibit initial_policy_mapping_inhibit_ =
      InitialPolicyMappingInhibit::kFalse;
  const InitialAnyPolicyInhibit initial_any_policy_inhibit_ =
      InitialAnyPolicyInhibit::kFalse;
};

TEST_F(PathBuilderMultiRootTest, TargetHasNameAndSpkiOfTrustAnchor) {
  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(a_by_b_);
  trust_store.AddTrustAnchor(b_by_f_);

  CertPathBuilder path_builder(
      a_by_b_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);

  auto result = path_builder.Run();

  ASSERT_TRUE(result.HasValidPath());
  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
  const auto &path = *result.GetBestValidPath();
  ASSERT_EQ(2U, path.certs.size());
  EXPECT_EQ(a_by_b_, path.certs[0]);
  EXPECT_EQ(b_by_f_, path.certs[1]);
}

TEST_F(PathBuilderMultiRootTest, TargetWithSameNameAsTrustAnchorFails) {
  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(a_by_b_);

  CertPathBuilder path_builder(
      a_by_b_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);

  auto result = path_builder.Run();

  EXPECT_FALSE(result.HasValidPath());
  EXPECT_EQ(1U, result.max_depth_seen);
  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_NOT_FOUND)
      << error.DiagnosticString();
}

TEST_F(PathBuilderMultiRootTest, SelfSignedTrustAnchorSupplementalCert) {
  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(d_by_d_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(d_by_d_);
  sync_certs.AddCert(c_by_d_);

  der::GeneralizedTime expired_time = {2016, 1, 1, 0, 0, 0};

  CertPathBuilder path_builder(
      b_by_c_, &trust_store, &delegate_, expired_time, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&sync_certs);

  auto result = path_builder.Run();

  EXPECT_FALSE(result.HasValidPath());
  ASSERT_EQ(1U, result.paths.size());

  EXPECT_FALSE(result.paths[0]->IsValid());
  const auto &path0 = *result.paths[0];
  ASSERT_EQ(3U, path0.certs.size());
  EXPECT_EQ(b_by_c_, path0.certs[0]);
  EXPECT_EQ(c_by_d_, path0.certs[1]);
  EXPECT_EQ(d_by_d_, path0.certs[2]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::CERTIFICATE_NOT_YET_VALID)
      << error.DiagnosticString();
}

TEST_F(PathBuilderMultiRootTest, TargetIsSelfSignedTrustAnchor) {
  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(e_by_e_);

  trust_store.AddTrustAnchor(f_by_e_);

  CertPathBuilder path_builder(
      e_by_e_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);

  auto result = path_builder.Run();

  ASSERT_TRUE(result.HasValidPath());

  const auto &path = *result.GetBestValidPath();
  ASSERT_EQ(2U, path.certs.size());
  EXPECT_EQ(e_by_e_, path.certs[0]);
  EXPECT_EQ(e_by_e_, path.certs[1]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
}

TEST_F(PathBuilderMultiRootTest, TargetDirectlySignedByTrustAnchor) {
  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(b_by_f_);

  CertPathBuilder path_builder(
      a_by_b_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);

  auto result = path_builder.Run();

  ASSERT_TRUE(result.HasValidPath());
  const auto &path = *result.GetBestValidPath();
  ASSERT_EQ(2U, path.certs.size());
  EXPECT_EQ(a_by_b_, path.certs[0]);
  EXPECT_EQ(b_by_f_, path.certs[1]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
}

TEST_F(PathBuilderMultiRootTest, TriesSyncFirst) {
  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(e_by_e_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(b_by_f_);
  sync_certs.AddCert(f_by_e_);

  AsyncCertIssuerSourceStatic async_certs;
  async_certs.AddCert(b_by_c_);
  async_certs.AddCert(c_by_e_);

  CertPathBuilder path_builder(
      a_by_b_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&async_certs);
  path_builder.AddCertIssuerSource(&sync_certs);

  auto result = path_builder.Run();

  EXPECT_TRUE(result.HasValidPath());
  EXPECT_EQ(0, async_certs.num_async_gets());

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
}

TEST_F(PathBuilderMultiRootTest, TestAsyncSimultaneous) {
  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(e_by_e_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(b_by_c_);
  sync_certs.AddCert(b_by_f_);

  AsyncCertIssuerSourceStatic async_certs1;
  async_certs1.AddCert(c_by_e_);

  AsyncCertIssuerSourceStatic async_certs2;
  async_certs2.AddCert(f_by_e_);

  CertPathBuilder path_builder(
      a_by_b_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&async_certs1);
  path_builder.AddCertIssuerSource(&async_certs2);
  path_builder.AddCertIssuerSource(&sync_certs);

  auto result = path_builder.Run();

  EXPECT_TRUE(result.HasValidPath());
  EXPECT_EQ(1, async_certs1.num_async_gets());
  EXPECT_EQ(1, async_certs2.num_async_gets());

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
}

TEST_F(PathBuilderMultiRootTest, TestLongChain) {

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(d_by_d_);
  trust_store.AddTrustAnchor(c_by_d_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(b_by_c_);
  sync_certs.AddCert(c_by_d_);

  CertPathBuilder path_builder(
      a_by_b_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&sync_certs);

  auto result = path_builder.Run();

  ASSERT_TRUE(result.HasValidPath());

  EXPECT_EQ(3U, result.GetBestValidPath()->certs.size());

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
}

TEST_F(PathBuilderMultiRootTest, TestBacktracking) {

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(d_by_d_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(b_by_f_);
  sync_certs.AddCert(f_by_e_);

  AsyncCertIssuerSourceStatic async_certs;
  async_certs.AddCert(b_by_c_);
  async_certs.AddCert(c_by_d_);

  CertPathBuilder path_builder(
      a_by_b_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&sync_certs);
  path_builder.AddCertIssuerSource(&async_certs);

  auto result = path_builder.Run();

  ASSERT_TRUE(result.HasValidPath());

  ASSERT_EQ(2U, result.paths.size());
  EXPECT_FALSE(result.paths[0]->IsValid());
  ASSERT_EQ(3U, result.paths[0]->certs.size());
  EXPECT_EQ(a_by_b_, result.paths[0]->certs[0]);
  EXPECT_EQ(b_by_f_, result.paths[0]->certs[1]);
  EXPECT_EQ(f_by_e_, result.paths[0]->certs[2]);

  EXPECT_EQ(1U, result.best_result_index);
  EXPECT_TRUE(result.paths[1]->IsValid());
  const auto &path = *result.GetBestValidPath();
  ASSERT_EQ(4U, path.certs.size());
  EXPECT_EQ(a_by_b_, path.certs[0]);
  EXPECT_EQ(b_by_c_, path.certs[1]);
  EXPECT_EQ(c_by_d_, path.certs[2]);
  EXPECT_EQ(d_by_d_, path.certs[3]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
}

TEST_F(PathBuilderMultiRootTest, TestOnlyPartialPathResult) {
  TrustStoreInMemory trust_store;

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(b_by_f_);
  sync_certs.AddCert(f_by_e_);

  CertPathBuilder path_builder(
      a_by_b_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&sync_certs);

  auto result = path_builder.Run();

  EXPECT_FALSE(result.HasValidPath());

  ASSERT_EQ(1U, result.paths.size());
  EXPECT_FALSE(result.paths[0]->IsValid());
  ASSERT_EQ(3U, result.paths[0]->certs.size());
  EXPECT_EQ(a_by_b_, result.paths[0]->certs[0]);
  EXPECT_EQ(b_by_f_, result.paths[0]->certs[1]);
  EXPECT_EQ(f_by_e_, result.paths[0]->certs[2]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_NOT_FOUND)
      << error.DiagnosticString();
}

TEST_F(PathBuilderMultiRootTest, TestTwoPartialPathResults) {
  TrustStoreInMemory trust_store;

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(b_by_f_);
  sync_certs.AddCert(f_by_e_);

  AsyncCertIssuerSourceStatic async_certs;
  async_certs.AddCert(b_by_c_);
  async_certs.AddCert(c_by_d_);

  CertPathBuilder path_builder(
      a_by_b_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&sync_certs);
  path_builder.AddCertIssuerSource(&async_certs);

  auto result = path_builder.Run();

  EXPECT_FALSE(result.HasValidPath());

  EXPECT_EQ(0U, result.best_result_index);

  ASSERT_EQ(2U, result.paths.size());
  EXPECT_FALSE(result.paths[0]->IsValid());
  ASSERT_EQ(3U, result.paths[0]->certs.size());
  EXPECT_EQ(a_by_b_, result.paths[0]->certs[0]);
  EXPECT_EQ(b_by_f_, result.paths[0]->certs[1]);
  EXPECT_EQ(f_by_e_, result.paths[0]->certs[2]);

  EXPECT_FALSE(result.paths[1]->IsValid());
  ASSERT_EQ(3U, result.paths[1]->certs.size());
  EXPECT_EQ(a_by_b_, result.paths[1]->certs[0]);
  EXPECT_EQ(b_by_c_, result.paths[1]->certs[1]);
  EXPECT_EQ(c_by_d_, result.paths[1]->certs[2]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_NOT_FOUND)
      << error.DiagnosticString();
}

TEST_F(PathBuilderMultiRootTest, TestDistrustedPathPreferredOverPartialPath) {

  TrustStoreInMemory trust_store;
  trust_store.AddDistrustedCertificateForTest(d_by_d_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(b_by_f_);
  sync_certs.AddCert(f_by_e_);

  AsyncCertIssuerSourceStatic async_certs;
  async_certs.AddCert(b_by_c_);
  async_certs.AddCert(c_by_d_);

  CertPathBuilder path_builder(
      a_by_b_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&sync_certs);
  path_builder.AddCertIssuerSource(&async_certs);

  auto result = path_builder.Run();

  EXPECT_FALSE(result.HasValidPath());

  ASSERT_EQ(2U, result.paths.size());
  EXPECT_FALSE(result.paths[0]->IsValid());
  ASSERT_EQ(3U, result.paths[0]->certs.size());
  EXPECT_EQ(a_by_b_, result.paths[0]->certs[0]);
  EXPECT_EQ(b_by_f_, result.paths[0]->certs[1]);
  EXPECT_EQ(f_by_e_, result.paths[0]->certs[2]);

  EXPECT_EQ(1U, result.best_result_index);
  EXPECT_FALSE(result.paths[1]->IsValid());
  const auto &path = *result.GetBestPathPossiblyInvalid();
  ASSERT_EQ(4U, path.certs.size());
  EXPECT_EQ(a_by_b_, path.certs[0]);
  EXPECT_EQ(b_by_c_, path.certs[1]);
  EXPECT_EQ(c_by_d_, path.certs[2]);
  EXPECT_EQ(d_by_d_, path.certs[3]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_NOT_FOUND)
      << error.DiagnosticString();
}

TEST_F(PathBuilderMultiRootTest, TestCertIssuerOrdering) {

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(d_by_d_);

  for (bool reverse_order : {false, true}) {
    SCOPED_TRACE(reverse_order);
    std::vector<std::shared_ptr<const ParsedCertificate>> certs = {
        b_by_c_, b_by_f_, f_by_e_, c_by_d_, c_by_e_};
    CertIssuerSourceStatic sync_certs;
    if (reverse_order) {
      for (auto it = certs.rbegin(); it != certs.rend(); ++it) {
        sync_certs.AddCert(*it);
      }
    } else {
      for (const auto &cert : certs) {
        sync_certs.AddCert(cert);
      }
    }

    CertPathBuilder path_builder(
        a_by_b_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
        initial_explicit_policy_, user_initial_policy_set_,
        initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
    path_builder.AddCertIssuerSource(&sync_certs);

    auto result = path_builder.Run();

    ASSERT_TRUE(result.HasValidPath());

    const auto &path = *result.GetBestValidPath();
    ASSERT_EQ(4U, path.certs.size());
    EXPECT_EQ(a_by_b_, path.certs[0]);
    EXPECT_EQ(b_by_c_, path.certs[1]);
    EXPECT_EQ(c_by_d_, path.certs[2]);
    EXPECT_EQ(d_by_d_, path.certs[3]);

    VerifyError error = result.GetBestPathVerifyError();
    ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
        << error.DiagnosticString();
  }
}

TEST_F(PathBuilderMultiRootTest, TestIterationLimit) {

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(d_by_d_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(b_by_c_);
  sync_certs.AddCert(c_by_d_);

  for (const bool insufficient_limit : {true, false}) {
    SCOPED_TRACE(insufficient_limit);

    StrictMock<MockPathBuilderDelegate> mock_delegate;

    EXPECT_CALL(mock_delegate, CheckPathAfterVerification(_, _));

    CertPathBuilder path_builder(
        a_by_b_, &trust_store, &mock_delegate, time_, KeyPurpose::ANY_EKU,
        initial_explicit_policy_, user_initial_policy_set_,
        initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
    path_builder.AddCertIssuerSource(&sync_certs);

    if (insufficient_limit) {

      path_builder.SetIterationLimit(1);
    } else {

      path_builder.SetIterationLimit(5);
    }

    auto result = path_builder.Run();

    EXPECT_EQ(!insufficient_limit, result.HasValidPath());
    EXPECT_EQ(insufficient_limit, result.exceeded_iteration_limit);

    VerifyError error = result.GetBestPathVerifyError();
    if (insufficient_limit) {
      EXPECT_EQ(2U, result.iteration_count);
      ASSERT_EQ(error.Code(),
                VerifyError::StatusCode::PATH_ITERATION_COUNT_EXCEEDED)
          << error.DiagnosticString();
    } else {
      EXPECT_EQ(3U, result.iteration_count);
      ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
          << error.DiagnosticString();
    }
  }
}

TEST_F(PathBuilderMultiRootTest, TestTrivialDeadline) {

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(c_by_d_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(b_by_c_);

  for (const bool insufficient_limit : {true, false}) {
    SCOPED_TRACE(insufficient_limit);

    CertPathBuilder path_builder(
        a_by_b_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
        initial_explicit_policy_, user_initial_policy_set_,
        initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
    path_builder.AddCertIssuerSource(&sync_certs);

    delegate_.SetDeadlineExpiredForTesting(insufficient_limit);

    auto result = path_builder.Run();

    EXPECT_EQ(!insufficient_limit, result.HasValidPath());
    EXPECT_EQ(insufficient_limit, result.exceeded_deadline);
    EXPECT_EQ(delegate_.IsDeadlineExpired(), insufficient_limit);

    if (insufficient_limit) {
      ASSERT_EQ(1U, result.paths.size());
      EXPECT_FALSE(result.paths[0]->IsValid());
      ASSERT_EQ(1U, result.paths[0]->certs.size());
      EXPECT_EQ(a_by_b_, result.paths[0]->certs[0]);
      EXPECT_TRUE(result.paths[0]->errors.ContainsError(
          cert_errors::kDeadlineExceeded));
    } else {
      ASSERT_EQ(1U, result.paths.size());
      EXPECT_TRUE(result.paths[0]->IsValid());
      ASSERT_EQ(3U, result.paths[0]->certs.size());
      EXPECT_EQ(a_by_b_, result.paths[0]->certs[0]);
      EXPECT_EQ(b_by_c_, result.paths[0]->certs[1]);
      EXPECT_EQ(c_by_d_, result.paths[0]->certs[2]);
    }
  }
}

TEST_F(PathBuilderMultiRootTest, TestVerifyCache) {

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(c_by_d_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(b_by_c_);

  EXPECT_FALSE(delegate_.GetVerifyCache());
  delegate_.ActivateCache();
  EXPECT_TRUE(delegate_.GetVerifyCache());
  delegate_.DeActivateCache();
  EXPECT_FALSE(delegate_.GetVerifyCache());
  delegate_.ActivateCache();
  EXPECT_TRUE(delegate_.GetVerifyCache());
  for (size_t i = 0; i < 3; i++) {
    SCOPED_TRACE(i);

    CertPathBuilder path_builder(
        a_by_b_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
        initial_explicit_policy_, user_initial_policy_set_,
        initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
    path_builder.AddCertIssuerSource(&sync_certs);

    auto result = path_builder.Run();

    ASSERT_EQ(1U, result.paths.size());
    EXPECT_TRUE(result.paths[0]->IsValid());
    ASSERT_EQ(3U, result.paths[0]->certs.size());
    EXPECT_EQ(a_by_b_, result.paths[0]->certs[0]);
    EXPECT_EQ(b_by_c_, result.paths[0]->certs[1]);
    EXPECT_EQ(c_by_d_, result.paths[0]->certs[2]);

    EXPECT_EQ(delegate_.GetMockVerifyCache()->CacheHits(), i * 2);
    EXPECT_EQ(delegate_.GetMockVerifyCache()->CacheMisses(), 2U);
    EXPECT_EQ(delegate_.GetMockVerifyCache()->CacheStores(), 2U);

    VerifyError error = result.GetBestPathVerifyError();
    ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
        << error.DiagnosticString();
  }
}

TEST_F(PathBuilderMultiRootTest, TestDeadline) {
  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(d_by_d_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(b_by_c_);

  AsyncCertIssuerSourceStatic async_certs;
  async_certs.AddCert(c_by_d_);
  async_certs.SetAsyncGetCallback(
      [&] { delegate_.SetDeadlineExpiredForTesting(true); });

  CertPathBuilder path_builder(
      a_by_b_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&sync_certs);
  path_builder.AddCertIssuerSource(&async_certs);

  auto result = path_builder.Run();

  EXPECT_FALSE(result.HasValidPath());
  EXPECT_TRUE(result.exceeded_deadline);
  EXPECT_TRUE(delegate_.IsDeadlineExpired());

  ASSERT_EQ(1U, result.paths.size());
  EXPECT_FALSE(result.paths[0]->IsValid());
  ASSERT_EQ(3U, result.paths[0]->certs.size());
  EXPECT_EQ(a_by_b_, result.paths[0]->certs[0]);
  EXPECT_EQ(b_by_c_, result.paths[0]->certs[1]);
  EXPECT_EQ(c_by_d_, result.paths[0]->certs[2]);
  EXPECT_TRUE(
      result.paths[0]->errors.ContainsError(cert_errors::kDeadlineExceeded));

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_DEADLINE_EXCEEDED)
      << error.DiagnosticString();
}

TEST_F(PathBuilderMultiRootTest, TestDepthLimit) {

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(d_by_d_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(b_by_c_);
  sync_certs.AddCert(c_by_d_);

  for (const bool insufficient_limit : {true, false}) {
    CertPathBuilder path_builder(
        a_by_b_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
        initial_explicit_policy_, user_initial_policy_set_,
        initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
    path_builder.AddCertIssuerSource(&sync_certs);

    if (insufficient_limit) {

      path_builder.SetDepthLimit(2);
    } else {

      path_builder.SetDepthLimit(5);
    }

    auto result = path_builder.Run();

    EXPECT_EQ(!insufficient_limit, result.HasValidPath());
    EXPECT_EQ(insufficient_limit,
              result.AnyPathContainsError(cert_errors::kDepthLimitExceeded));
    VerifyError error = result.GetBestPathVerifyError();
    if (insufficient_limit) {
      EXPECT_EQ(2U, result.max_depth_seen);
      ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_DEPTH_LIMIT_REACHED)
          << error.DiagnosticString();
    } else {
      EXPECT_EQ(4U, result.max_depth_seen);
      ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
          << error.DiagnosticString();
    }
  }
}

TEST_F(PathBuilderMultiRootTest, TestDepthLimitMultiplePaths) {

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(e_by_e_);
  trust_store.AddTrustAnchor(c_by_d_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(b_by_c_);
  sync_certs.AddCert(b_by_f_);
  sync_certs.AddCert(f_by_e_);

  CertPathBuilder path_builder(
      a_by_b_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&sync_certs);

  path_builder.SetDepthLimit(3);

  auto result = path_builder.Run();

  EXPECT_TRUE(result.HasValidPath());
  EXPECT_TRUE(result.AnyPathContainsError(cert_errors::kDepthLimitExceeded));

  ASSERT_EQ(result.paths.size(), 2u);

  const CertPathBuilderResultPath *truncated_path = result.paths[0].get();
  EXPECT_FALSE(truncated_path->IsValid());
  EXPECT_TRUE(
      truncated_path->errors.ContainsError(cert_errors::kDepthLimitExceeded));
  ASSERT_EQ(truncated_path->certs.size(), 3u);
  EXPECT_EQ(a_by_b_, truncated_path->certs[0]);
  EXPECT_EQ(b_by_f_, truncated_path->certs[1]);
  EXPECT_EQ(f_by_e_, truncated_path->certs[2]);

  const CertPathBuilderResultPath *valid_path = result.paths[1].get();
  EXPECT_TRUE(valid_path->IsValid());
  EXPECT_FALSE(
      valid_path->errors.ContainsError(cert_errors::kDepthLimitExceeded));
  ASSERT_EQ(valid_path->certs.size(), 3u);
  EXPECT_EQ(a_by_b_, valid_path->certs[0]);
  EXPECT_EQ(b_by_c_, valid_path->certs[1]);
  EXPECT_EQ(c_by_d_, valid_path->certs[2]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
}

TEST_F(PathBuilderMultiRootTest, TestPreCertificate) {

  std::string test_dir =
      "testdata/path_builder_unittest/precertificate/";
  std::shared_ptr<const ParsedCertificate> root1 =
      ReadCertFromFile(test_dir + "root.pem");
  ASSERT_TRUE(root1);
  std::shared_ptr<const ParsedCertificate> target =
      ReadCertFromFile(test_dir + "precertificate.pem");
  ASSERT_TRUE(target);

  der::GeneralizedTime precert_time = {2023, 10, 1, 0, 0, 0};

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(root1);

  EXPECT_FALSE(delegate_.AcceptPreCertificates());
  CertPathBuilder path_builder(
      target, &trust_store, &delegate_, precert_time, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  auto result = path_builder.Run();
  ASSERT_EQ(1U, result.paths.size());
  ASSERT_FALSE(result.paths[0]->IsValid())
      << result.paths[0]->errors.ToDebugString(result.paths[0]->certs);
  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::CERTIFICATE_INVALID)
      << error.DiagnosticString();

  delegate_.AllowPrecert();
  EXPECT_TRUE(delegate_.AcceptPreCertificates());
  CertPathBuilder path_builder2(
      target, &trust_store, &delegate_, precert_time, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  auto result2 = path_builder2.Run();
  ASSERT_EQ(1U, result2.paths.size());
  ASSERT_TRUE(result2.paths[0]->IsValid())
      << result2.paths[0]->errors.ToDebugString(result.paths[0]->certs);
  VerifyError error2 = result2.GetBestPathVerifyError();
  ASSERT_EQ(error2.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error2.DiagnosticString();
}

class PathBuilderKeyRolloverTest : public ::testing::Test {
 public:
  PathBuilderKeyRolloverTest()
      : delegate_(1024,
                  SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1) {}

  void SetUp() override {
    ParsedCertificateList path;

    VerifyCertChainTest test;
    ASSERT_TRUE(ReadVerifyCertChainTestFromFile(
        "testdata/verify_certificate_chain_unittest/key-rollover/oldchain.test",
        &test));
    path = test.chain;
    ASSERT_EQ(3U, path.size());
    target_ = path[0];
    oldintermediate_ = path[1];
    oldroot_ = path[2];
    time_ = test.time;

    ASSERT_TRUE(target_);
    ASSERT_TRUE(oldintermediate_);

    ASSERT_TRUE(ReadVerifyCertChainTestFromFile(
        "testdata/verify_certificate_chain_unittest/"
        "key-rollover/longrolloverchain.test",
        &test));
    path = test.chain;

    ASSERT_EQ(5U, path.size());
    newintermediate_ = path[1];
    newroot_ = path[2];
    newrootrollover_ = path[3];
    ASSERT_TRUE(newintermediate_);
    ASSERT_TRUE(newroot_);
    ASSERT_TRUE(newrootrollover_);
  }

 protected:

  std::shared_ptr<const ParsedCertificate> target_;
  std::shared_ptr<const ParsedCertificate> oldintermediate_;
  std::shared_ptr<const ParsedCertificate> newintermediate_;
  std::shared_ptr<const ParsedCertificate> oldroot_;
  std::shared_ptr<const ParsedCertificate> newroot_;
  std::shared_ptr<const ParsedCertificate> newrootrollover_;

  SimplePathBuilderDelegate delegate_;
  der::GeneralizedTime time_;

  const InitialExplicitPolicy initial_explicit_policy_ =
      InitialExplicitPolicy::kFalse;
  const std::set<der::Input> user_initial_policy_set_ = {
      der::Input(kAnyPolicyOid)};
  const InitialPolicyMappingInhibit initial_policy_mapping_inhibit_ =
      InitialPolicyMappingInhibit::kFalse;
  const InitialAnyPolicyInhibit initial_any_policy_inhibit_ =
      InitialAnyPolicyInhibit::kFalse;
};

TEST_F(PathBuilderKeyRolloverTest, TestRolloverOnlyOldRootTrusted) {

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(oldroot_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(newintermediate_);
  sync_certs.AddCert(newrootrollover_);

  CertPathBuilder path_builder(
      target_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&sync_certs);

  auto result = path_builder.Run();

  EXPECT_TRUE(result.HasValidPath());

  ASSERT_EQ(1U, result.paths.size());
  const auto &path0 = *result.paths[0];
  EXPECT_EQ(0U, result.best_result_index);
  EXPECT_TRUE(path0.IsValid());
  ASSERT_EQ(4U, path0.certs.size());
  EXPECT_EQ(target_, path0.certs[0]);
  EXPECT_EQ(newintermediate_, path0.certs[1]);
  EXPECT_EQ(newrootrollover_, path0.certs[2]);
  EXPECT_EQ(oldroot_, path0.certs[3]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
}

TEST_F(PathBuilderKeyRolloverTest, TestRolloverBothRootsTrusted) {

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(oldroot_);
  trust_store.AddTrustAnchor(newroot_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(oldintermediate_);
  sync_certs.AddCert(newintermediate_);
  sync_certs.AddCert(newrootrollover_);

  CertPathBuilder path_builder(
      target_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&sync_certs);

  auto result = path_builder.Run();

  EXPECT_TRUE(result.HasValidPath());

  ASSERT_EQ(1U, result.paths.size());
  const auto &path = *result.paths[0];
  EXPECT_TRUE(result.paths[0]->IsValid());
  ASSERT_EQ(3U, path.certs.size());
  EXPECT_EQ(target_, path.certs[0]);

  EXPECT_EQ(newintermediate_, path.certs[1]);
  EXPECT_EQ(newroot_, path.certs[2]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
}

TEST_F(PathBuilderKeyRolloverTest, TestAnchorsNoMatchAndNoIssuerSources) {
  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(newroot_);

  CertPathBuilder path_builder(
      target_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);

  auto result = path_builder.Run();

  EXPECT_FALSE(result.HasValidPath());

  ASSERT_EQ(1U, result.paths.size());
  const auto &path = *result.paths[0];
  EXPECT_FALSE(result.paths[0]->IsValid());
  ASSERT_EQ(1U, path.certs.size());
  EXPECT_EQ(target_, path.certs[0]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_NOT_FOUND)
      << error.DiagnosticString();
}

TEST_F(PathBuilderKeyRolloverTest, TestReturnsPartialPathEndedByLoopChecker) {
  TrustStoreInMemory trust_store;

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(newintermediate_);
  sync_certs.AddCert(newroot_);

  CertIssuerSourceStatic rollover_certs;
  rollover_certs.AddCert(newrootrollover_);

  CertPathBuilder path_builder(
      target_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&sync_certs);

  path_builder.AddCertIssuerSource(&rollover_certs);

  auto result = path_builder.Run();

  EXPECT_FALSE(result.HasValidPath());
  ASSERT_EQ(2U, result.paths.size());

  {
    const auto &path = *result.paths[0];
    EXPECT_FALSE(path.IsValid());
    ASSERT_EQ(3U, path.certs.size());
    EXPECT_EQ(target_, path.certs[0]);
    EXPECT_EQ(newintermediate_, path.certs[1]);
    EXPECT_EQ(newroot_, path.certs[2]);
    EXPECT_TRUE(path.errors.ContainsError(cert_errors::kNoIssuersFound));
  }

  {
    const auto &path = *result.paths[1];
    EXPECT_FALSE(path.IsValid());
    ASSERT_EQ(3U, path.certs.size());
    EXPECT_EQ(target_, path.certs[0]);
    EXPECT_EQ(newintermediate_, path.certs[1]);
    EXPECT_EQ(newrootrollover_, path.certs[2]);
    EXPECT_TRUE(path.errors.ContainsError(cert_errors::kNoIssuersFound));
  }

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_NOT_FOUND)
      << error.DiagnosticString();
}

TEST_F(PathBuilderKeyRolloverTest, TestMultipleRootMatchesOnlyOneWorks) {
  TrustStoreCollection trust_store_collection;
  TrustStoreInMemory trust_store1;
  TrustStoreInMemory trust_store2;
  trust_store_collection.AddTrustStore(&trust_store1);
  trust_store_collection.AddTrustStore(&trust_store2);

  trust_store1.AddTrustAnchor(newroot_);
  trust_store2.AddTrustAnchor(oldroot_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(oldintermediate_);

  CertPathBuilder path_builder(
      target_, &trust_store_collection, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&sync_certs);

  auto result = path_builder.Run();

  EXPECT_TRUE(result.HasValidPath());
  ASSERT_EQ(1U, result.paths.size());

  EXPECT_TRUE(result.paths[result.best_result_index]->IsValid());
  const auto &path = *result.paths[result.best_result_index];
  ASSERT_EQ(3U, path.certs.size());
  EXPECT_EQ(target_, path.certs[0]);
  EXPECT_EQ(oldintermediate_, path.certs[1]);
  EXPECT_EQ(oldroot_, path.certs[2]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
}

TEST_F(PathBuilderKeyRolloverTest, TestRolloverLongChain) {

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(oldroot_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(newintermediate_);
  sync_certs.AddCert(newroot_);

  AsyncCertIssuerSourceStatic async_certs;
  async_certs.AddCert(newrootrollover_);

  CertPathBuilder path_builder(
      target_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&sync_certs);
  path_builder.AddCertIssuerSource(&async_certs);

  auto result = path_builder.Run();

  EXPECT_TRUE(result.HasValidPath());
  ASSERT_EQ(3U, result.paths.size());

  EXPECT_FALSE(result.paths[0]->IsValid());
  const auto &path0 = *result.paths[0];
  ASSERT_EQ(4U, path0.certs.size());
  EXPECT_EQ(target_, path0.certs[0]);
  EXPECT_EQ(newintermediate_, path0.certs[1]);
  EXPECT_EQ(newroot_, path0.certs[2]);
  EXPECT_EQ(oldroot_, path0.certs[3]);

  EXPECT_FALSE(result.paths[1]->IsValid());
  const auto &path1 = *result.paths[1];
  ASSERT_EQ(3U, path1.certs.size());
  EXPECT_EQ(target_, path1.certs[0]);
  EXPECT_EQ(newintermediate_, path1.certs[1]);
  EXPECT_EQ(oldroot_, path1.certs[2]);

  EXPECT_EQ(2U, result.best_result_index);
  EXPECT_TRUE(result.paths[2]->IsValid());
  const auto &path2 = *result.paths[2];
  ASSERT_EQ(4U, path2.certs.size());
  EXPECT_EQ(target_, path2.certs[0]);
  EXPECT_EQ(newintermediate_, path2.certs[1]);
  EXPECT_EQ(newrootrollover_, path2.certs[2]);
  EXPECT_EQ(oldroot_, path2.certs[3]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
}

TEST_F(PathBuilderKeyRolloverTest, ExploreAllPathsWithIterationLimit) {
  struct Expectation {
    int iteration_limit;
    size_t expected_num_paths;
    std::vector<std::shared_ptr<const ParsedCertificate>> partial_path;
  } kExpectations[] = {

      {0, 4, {}},

      {1, 0, {target_, newintermediate_}},

      {2, 1, {target_, newintermediate_}},

      {3, 2, {target_, newintermediate_}},

      {4, 2, {target_}},

      {5, 2, {target_, oldintermediate_}},

      {6, 3, {target_, oldintermediate_}},

      {7, 4, {target_, oldintermediate_}},
  };

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(oldroot_);
  trust_store.AddTrustAnchor(newroot_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(oldintermediate_);
  sync_certs.AddCert(newintermediate_);

  for (const auto &expectation : kExpectations) {
    SCOPED_TRACE(expectation.iteration_limit);

    CertPathBuilder path_builder(
        target_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
        initial_explicit_policy_, user_initial_policy_set_,
        initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
    path_builder.AddCertIssuerSource(&sync_certs);

    path_builder.SetExploreAllPaths(true);

    path_builder.SetIterationLimit(expectation.iteration_limit);

    auto result = path_builder.Run();

    EXPECT_EQ(expectation.expected_num_paths > 0, result.HasValidPath());
    VerifyError error = result.GetBestPathVerifyError();
    if (expectation.expected_num_paths > 0) {
      ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
          << error.DiagnosticString();
    } else {
      ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_ITERATION_COUNT_EXCEEDED)
          << error.DiagnosticString();
    }

    if (expectation.partial_path.empty()) {
      ASSERT_EQ(expectation.expected_num_paths, result.paths.size());
    } else {
      ASSERT_EQ(1 + expectation.expected_num_paths, result.paths.size());
      const auto &path = *result.paths[result.paths.size() - 1];
      EXPECT_FALSE(path.IsValid());
      EXPECT_EQ(expectation.partial_path, path.certs);
      EXPECT_TRUE(
          path.errors.ContainsError(cert_errors::kIterationLimitExceeded));
    }

    if (expectation.expected_num_paths > 0) {

      const auto &path0 = *result.paths[0];
      EXPECT_TRUE(path0.IsValid());
      ASSERT_EQ(3U, path0.certs.size());
      EXPECT_EQ(target_, path0.certs[0]);
      EXPECT_EQ(newintermediate_, path0.certs[1]);
      EXPECT_EQ(newroot_, path0.certs[2]);
      EXPECT_EQ(3U, result.max_depth_seen);
    }

    if (expectation.expected_num_paths > 1) {

      const auto &path1 = *result.paths[1];
      EXPECT_FALSE(path1.IsValid());
      ASSERT_EQ(3U, path1.certs.size());
      EXPECT_EQ(target_, path1.certs[0]);
      EXPECT_EQ(newintermediate_, path1.certs[1]);
      EXPECT_EQ(oldroot_, path1.certs[2]);
      EXPECT_EQ(3U, result.max_depth_seen);
    }

    if (expectation.expected_num_paths > 2) {

      const auto &path2 = *result.paths[2];
      EXPECT_TRUE(path2.IsValid());
      ASSERT_EQ(3U, path2.certs.size());
      EXPECT_EQ(target_, path2.certs[0]);
      EXPECT_EQ(oldintermediate_, path2.certs[1]);
      EXPECT_EQ(oldroot_, path2.certs[2]);
      EXPECT_EQ(3U, result.max_depth_seen);
    }

    if (expectation.expected_num_paths > 3) {

      const auto &path3 = *result.paths[3];
      EXPECT_FALSE(path3.IsValid());
      ASSERT_EQ(3U, path3.certs.size());
      EXPECT_EQ(target_, path3.certs[0]);
      EXPECT_EQ(oldintermediate_, path3.certs[1]);
      EXPECT_EQ(newroot_, path3.certs[2]);
      EXPECT_EQ(3U, result.max_depth_seen);
    }
  }
}

TEST_F(PathBuilderKeyRolloverTest, ExplorePathsWithPathLimit) {
  struct Expectation {
    size_t valid_path_limit;
    size_t expected_num_paths;
  } kExpectations[] = {
      {0, 4},
      {1, 1},
      {2, 3},
      {3, 4}, {4, 4}, {5, 4},
  };

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(oldroot_);
  trust_store.AddTrustAnchor(newroot_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(oldintermediate_);
  sync_certs.AddCert(newintermediate_);

  for (const auto &expectation : kExpectations) {
    SCOPED_TRACE(expectation.valid_path_limit);

    CertPathBuilder path_builder(
        target_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
        initial_explicit_policy_, user_initial_policy_set_,
        initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
    path_builder.AddCertIssuerSource(&sync_certs);

    path_builder.SetValidPathLimit(expectation.valid_path_limit);

    auto result = path_builder.Run();

    EXPECT_TRUE(result.HasValidPath());
    ASSERT_EQ(expectation.expected_num_paths, result.paths.size());

    if (result.paths.size() > 0) {

      const auto &path0 = *result.paths[0];
      EXPECT_TRUE(path0.IsValid());
      ASSERT_EQ(3U, path0.certs.size());
      EXPECT_EQ(target_, path0.certs[0]);
      EXPECT_EQ(newintermediate_, path0.certs[1]);
      EXPECT_EQ(newroot_, path0.certs[2]);
      EXPECT_EQ(3U, result.max_depth_seen);
    }

    if (result.paths.size() > 1) {

      const auto &path1 = *result.paths[1];
      EXPECT_FALSE(path1.IsValid());
      ASSERT_EQ(3U, path1.certs.size());
      EXPECT_EQ(target_, path1.certs[0]);
      EXPECT_EQ(newintermediate_, path1.certs[1]);
      EXPECT_EQ(oldroot_, path1.certs[2]);
      EXPECT_EQ(3U, result.max_depth_seen);
    }

    if (result.paths.size() > 2) {

      const auto &path2 = *result.paths[2];
      EXPECT_TRUE(path2.IsValid());
      ASSERT_EQ(3U, path2.certs.size());
      EXPECT_EQ(target_, path2.certs[0]);
      EXPECT_EQ(oldintermediate_, path2.certs[1]);
      EXPECT_EQ(oldroot_, path2.certs[2]);
      EXPECT_EQ(3U, result.max_depth_seen);
    }

    if (result.paths.size() > 3) {

      const auto &path3 = *result.paths[3];
      EXPECT_FALSE(path3.IsValid());
      ASSERT_EQ(3U, path3.certs.size());
      EXPECT_EQ(target_, path3.certs[0]);
      EXPECT_EQ(oldintermediate_, path3.certs[1]);
      EXPECT_EQ(newroot_, path3.certs[2]);
      EXPECT_EQ(3U, result.max_depth_seen);
    }

    VerifyError error = result.GetBestPathVerifyError();
    ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
        << error.DiagnosticString();
  }
}

TEST_F(PathBuilderKeyRolloverTest, TestEndEntityIsTrustRoot) {

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(newintermediate_);

  CertPathBuilder path_builder(
      newintermediate_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);

  auto result = path_builder.Run();

  EXPECT_FALSE(result.HasValidPath());

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_NOT_FOUND)
      << error.DiagnosticString();
}

TEST_F(PathBuilderKeyRolloverTest,
       TestEndEntityHasSameNameAndSpkiAsIntermediate) {

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(oldroot_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(newrootrollover_);

  CertPathBuilder path_builder(
      newroot_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&sync_certs);

  auto result = path_builder.Run();

  EXPECT_FALSE(result.HasValidPath());

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(),
            VerifyError::StatusCode::CERTIFICATE_INVALID_SIGNATURE)
      << error.DiagnosticString();
}

TEST_F(PathBuilderKeyRolloverTest,
       TestEndEntityHasSameNameAndSpkiAsTrustAnchor) {

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(newrootrollover_);

  CertPathBuilder path_builder(
      newroot_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);

  auto result = path_builder.Run();

  ASSERT_TRUE(result.HasValidPath());

  const CertPathBuilderResultPath *best_result = result.GetBestValidPath();

  EXPECT_TRUE(best_result->IsValid());
  ASSERT_EQ(2U, best_result->certs.size());
  EXPECT_EQ(newroot_, best_result->certs[0]);
  EXPECT_EQ(newrootrollover_, best_result->certs[1]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
}

TEST_F(PathBuilderKeyRolloverTest, TestDuplicateIntermediates) {

  std::shared_ptr<const ParsedCertificate> oldintermediate_dupe(
      ParsedCertificate::Create(
          bssl::UniquePtr<CRYPTO_BUFFER>(
              CRYPTO_BUFFER_new(oldintermediate_->der_cert().data(),
                                oldintermediate_->der_cert().size(), nullptr)),
          {}, nullptr));

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(newroot_);

  CertIssuerSourceStatic sync_certs1;
  sync_certs1.AddCert(oldintermediate_);
  CertIssuerSourceStatic sync_certs2;
  sync_certs2.AddCert(oldintermediate_dupe);

  AsyncCertIssuerSourceStatic async_certs;
  async_certs.AddCert(newintermediate_);

  CertPathBuilder path_builder(
      target_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&sync_certs1);
  path_builder.AddCertIssuerSource(&sync_certs2);
  path_builder.AddCertIssuerSource(&async_certs);

  auto result = path_builder.Run();

  EXPECT_TRUE(result.HasValidPath());
  ASSERT_EQ(2U, result.paths.size());

  EXPECT_FALSE(result.paths[0]->IsValid());
  const auto &path0 = *result.paths[0];

  ASSERT_EQ(3U, path0.certs.size());
  EXPECT_EQ(target_, path0.certs[0]);

  EXPECT_EQ(oldintermediate_->der_cert(), path0.certs[1]->der_cert());
  EXPECT_EQ(newroot_, path0.certs[2]);

  EXPECT_EQ(1U, result.best_result_index);
  EXPECT_TRUE(result.paths[1]->IsValid());
  const auto &path1 = *result.paths[1];
  ASSERT_EQ(3U, path1.certs.size());
  EXPECT_EQ(target_, path1.certs[0]);
  EXPECT_EQ(newintermediate_, path1.certs[1]);
  EXPECT_EQ(newroot_, path1.certs[2]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
}

TEST_F(PathBuilderKeyRolloverTest, TestDuplicateIntermediateAndRoot) {

  std::shared_ptr<const ParsedCertificate> newroot_dupe(
      ParsedCertificate::Create(
          bssl::UniquePtr<CRYPTO_BUFFER>(
              CRYPTO_BUFFER_new(newroot_->der_cert().data(),
                                newroot_->der_cert().size(), nullptr)),
          {}, nullptr));

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(newroot_);

  CertIssuerSourceStatic sync_certs;
  sync_certs.AddCert(oldintermediate_);
  sync_certs.AddCert(newroot_dupe);

  CertPathBuilder path_builder(
      target_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&sync_certs);

  auto result = path_builder.Run();

  EXPECT_FALSE(result.HasValidPath());
  ASSERT_EQ(1U, result.paths.size());

  EXPECT_FALSE(result.paths[0]->IsValid());
  const auto &path = *result.paths[0];
  ASSERT_EQ(3U, path.certs.size());
  EXPECT_EQ(target_, path.certs[0]);
  EXPECT_EQ(oldintermediate_, path.certs[1]);

  EXPECT_EQ(newroot_->der_cert(), path.certs[2]->der_cert());

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(),
            VerifyError::StatusCode::CERTIFICATE_INVALID_SIGNATURE)
      << error.DiagnosticString();
}

class MockCertIssuerSourceRequest : public CertIssuerSource::Request {
 public:
  MOCK_METHOD1(GetNext, void(ParsedCertificateList *));
};

class MockCertIssuerSource : public CertIssuerSource {
 public:
  MOCK_METHOD2(SyncGetIssuersOf,
               void(const ParsedCertificate *, ParsedCertificateList *));
  MOCK_METHOD2(AsyncGetIssuersOf,
               void(const ParsedCertificate *, std::unique_ptr<Request> *));
};

class CertIssuerSourceRequestMover {
 public:
  explicit CertIssuerSourceRequestMover(
      std::unique_ptr<CertIssuerSource::Request> req)
      : request_(std::move(req)) {}
  void MoveIt(const ParsedCertificate *cert,
              std::unique_ptr<CertIssuerSource::Request> *out_req) {
    *out_req = std::move(request_);
  }

 private:
  std::unique_ptr<CertIssuerSource::Request> request_;
};

class AppendCertToList {
 public:
  explicit AppendCertToList(
      const std::shared_ptr<const ParsedCertificate> &cert)
      : cert_(cert) {}

  void operator()(ParsedCertificateList *out) { out->push_back(cert_); }

 private:
  std::shared_ptr<const ParsedCertificate> cert_;
};

TEST_F(PathBuilderKeyRolloverTest, TestMultipleAsyncIssuersFromSingleSource) {
  StrictMock<MockCertIssuerSource> cert_issuer_source;

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(newroot_);

  CertPathBuilder path_builder(
      target_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&cert_issuer_source);

  auto target_issuers_req_owner =
      std::make_unique<StrictMock<MockCertIssuerSourceRequest>>();

  StrictMock<MockCertIssuerSourceRequest> *target_issuers_req =
      target_issuers_req_owner.get();

  CertIssuerSourceRequestMover req_mover(std::move(target_issuers_req_owner));
  {
    ::testing::InSequence s;
    EXPECT_CALL(cert_issuer_source, SyncGetIssuersOf(target_.get(), _));
    EXPECT_CALL(cert_issuer_source, AsyncGetIssuersOf(target_.get(), _))
        .WillOnce(Invoke(&req_mover, &CertIssuerSourceRequestMover::MoveIt));
  }

  EXPECT_CALL(*target_issuers_req, GetNext(_))

      .WillOnce(Invoke(AppendCertToList(oldintermediate_)))

      .WillOnce(Invoke(AppendCertToList(newintermediate_)));
  {
    ::testing::InSequence s;

    EXPECT_CALL(cert_issuer_source,
                SyncGetIssuersOf(oldintermediate_.get(), _));
    EXPECT_CALL(cert_issuer_source,
                AsyncGetIssuersOf(oldintermediate_.get(), _));
  }

  EXPECT_CALL(cert_issuer_source, SyncGetIssuersOf(newintermediate_.get(), _));

  auto result = path_builder.Run();

  ::testing::Mock::VerifyAndClearExpectations(&cert_issuer_source);

  EXPECT_TRUE(result.HasValidPath());
  ASSERT_EQ(2U, result.paths.size());

  EXPECT_FALSE(result.paths[0]->IsValid());
  const auto &path0 = *result.paths[0];
  ASSERT_EQ(3U, path0.certs.size());
  EXPECT_EQ(target_, path0.certs[0]);
  EXPECT_EQ(oldintermediate_, path0.certs[1]);
  EXPECT_EQ(newroot_, path0.certs[2]);

  EXPECT_TRUE(result.paths[1]->IsValid());
  const auto &path1 = *result.paths[1];
  ASSERT_EQ(3U, path1.certs.size());
  EXPECT_EQ(target_, path1.certs[0]);
  EXPECT_EQ(newintermediate_, path1.certs[1]);
  EXPECT_EQ(newroot_, path1.certs[2]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
}

TEST_F(PathBuilderKeyRolloverTest, TestDuplicateAsyncIntermediates) {
  StrictMock<MockCertIssuerSource> cert_issuer_source;

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(newroot_);

  CertPathBuilder path_builder(
      target_, &trust_store, &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);
  path_builder.AddCertIssuerSource(&cert_issuer_source);

  auto target_issuers_req_owner =
      std::make_unique<StrictMock<MockCertIssuerSourceRequest>>();

  StrictMock<MockCertIssuerSourceRequest> *target_issuers_req =
      target_issuers_req_owner.get();

  CertIssuerSourceRequestMover req_mover(std::move(target_issuers_req_owner));
  {
    ::testing::InSequence s;
    EXPECT_CALL(cert_issuer_source, SyncGetIssuersOf(target_.get(), _));
    EXPECT_CALL(cert_issuer_source, AsyncGetIssuersOf(target_.get(), _))
        .WillOnce(Invoke(&req_mover, &CertIssuerSourceRequestMover::MoveIt));
  }

  std::shared_ptr<const ParsedCertificate> oldintermediate_dupe(
      ParsedCertificate::Create(
          bssl::UniquePtr<CRYPTO_BUFFER>(
              CRYPTO_BUFFER_new(oldintermediate_->der_cert().data(),
                                oldintermediate_->der_cert().size(), nullptr)),
          {}, nullptr));

  EXPECT_CALL(*target_issuers_req, GetNext(_))

      .WillOnce(Invoke(AppendCertToList(oldintermediate_)))

      .WillOnce(Invoke(AppendCertToList(oldintermediate_dupe)))

      .WillOnce(Invoke(AppendCertToList(newintermediate_)));

  {
    ::testing::InSequence s;

    EXPECT_CALL(cert_issuer_source,
                SyncGetIssuersOf(oldintermediate_.get(), _));
    EXPECT_CALL(cert_issuer_source,
                AsyncGetIssuersOf(oldintermediate_.get(), _));
  }

  EXPECT_CALL(cert_issuer_source, SyncGetIssuersOf(newintermediate_.get(), _));

  auto result = path_builder.Run();

  ::testing::Mock::VerifyAndClearExpectations(&cert_issuer_source);

  EXPECT_TRUE(result.HasValidPath());
  ASSERT_EQ(2U, result.paths.size());

  EXPECT_FALSE(result.paths[0]->IsValid());
  const auto &path0 = *result.paths[0];
  ASSERT_EQ(3U, path0.certs.size());
  EXPECT_EQ(target_, path0.certs[0]);
  EXPECT_EQ(oldintermediate_, path0.certs[1]);
  EXPECT_EQ(newroot_, path0.certs[2]);

  EXPECT_TRUE(result.paths[1]->IsValid());
  const auto &path1 = *result.paths[1];
  ASSERT_EQ(3U, path1.certs.size());
  EXPECT_EQ(target_, path1.certs[0]);
  EXPECT_EQ(newintermediate_, path1.certs[1]);
  EXPECT_EQ(newroot_, path1.certs[2]);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
}

class PathBuilderSimpleChainTest : public ::testing::Test {
 public:
  PathBuilderSimpleChainTest() = default;

 protected:
  void SetUp() override {

    ASSERT_TRUE(ReadVerifyCertChainTestFromFile(
        "testdata/verify_certificate_chain_unittest/target-and-intermediate/"
        "main.test",
        &test_));
    ASSERT_EQ(3u, test_.chain.size());
  }

  CertPathBuilder::Result RunPathBuilder(
      const std::shared_ptr<const ParsedCertificate> &distrusted_cert,
      CertPathBuilderDelegate *optional_delegate) {

    TrustStoreInMemory trust_store;
    if (distrusted_cert != test_.chain.back()) {
      trust_store.AddTrustAnchor(test_.chain.back());
    }
    if (distrusted_cert) {
      trust_store.AddDistrustedCertificateForTest(distrusted_cert);
    }

    CertIssuerSourceStatic intermediates;
    intermediates.AddCert(test_.chain[1]);

    SimplePathBuilderDelegate default_delegate(
        1024, SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1);
    CertPathBuilderDelegate *delegate =
        optional_delegate ? optional_delegate : &default_delegate;

    const InitialExplicitPolicy initial_explicit_policy =
        InitialExplicitPolicy::kFalse;
    const std::set<der::Input> user_initial_policy_set = {
        der::Input(kAnyPolicyOid)};
    const InitialPolicyMappingInhibit initial_policy_mapping_inhibit =
        InitialPolicyMappingInhibit::kFalse;
    const InitialAnyPolicyInhibit initial_any_policy_inhibit =
        InitialAnyPolicyInhibit::kFalse;

    CertPathBuilder path_builder(
        test_.chain.front(), &trust_store, delegate, test_.time,
        KeyPurpose::ANY_EKU, initial_explicit_policy, user_initial_policy_set,
        initial_policy_mapping_inhibit, initial_any_policy_inhibit);
    path_builder.AddCertIssuerSource(&intermediates);
    return path_builder.Run();
  }

 protected:
  VerifyCertChainTest test_;
};

class PathBuilderDistrustTest : public PathBuilderSimpleChainTest {
 public:
  PathBuilderDistrustTest() = default;

 protected:

  CertPathBuilder::Result RunPathBuilderWithDistrustedCert(
      const std::shared_ptr<const ParsedCertificate> &distrusted_cert) {
    return RunPathBuilder(distrusted_cert, nullptr);
  }
};

TEST_F(PathBuilderDistrustTest, TargetIntermediateRoot) {

  CertPathBuilder::Result result = RunPathBuilderWithDistrustedCert(nullptr);
  {
    ASSERT_TRUE(result.HasValidPath());

    const auto &path = *result.GetBestValidPath();
    ASSERT_EQ(test_.chain.size(), path.certs.size());
    for (size_t i = 0; i < test_.chain.size(); ++i) {
      EXPECT_EQ(test_.chain[i], path.certs[i]);
    }
  }

  result = RunPathBuilderWithDistrustedCert(test_.chain[0]);
  {
    EXPECT_FALSE(result.HasValidPath());
    ASSERT_LT(result.best_result_index, result.paths.size());
    const auto &best_path = result.paths[result.best_result_index];

    ASSERT_EQ(1u, best_path->certs.size());
    EXPECT_EQ(best_path->certs[0], test_.chain[0]);
    EXPECT_TRUE(best_path->errors.ContainsHighSeverityErrors());
    best_path->errors.ContainsError(cert_errors::kDistrustedByTrustStore);
  }

  result = RunPathBuilderWithDistrustedCert(test_.chain[1]);
  {
    EXPECT_FALSE(result.HasValidPath());
    ASSERT_LT(result.best_result_index, result.paths.size());
    const auto &best_path = result.paths[result.best_result_index];

    ASSERT_EQ(2u, best_path->certs.size());
    EXPECT_EQ(best_path->certs[0], test_.chain[0]);
    EXPECT_EQ(best_path->certs[1], test_.chain[1]);
    EXPECT_TRUE(best_path->errors.ContainsHighSeverityErrors());
    best_path->errors.ContainsError(cert_errors::kDistrustedByTrustStore);
  }

  result = RunPathBuilderWithDistrustedCert(test_.chain[2]);
  {
    EXPECT_FALSE(result.HasValidPath());
    ASSERT_LT(result.best_result_index, result.paths.size());
    const auto &best_path = result.paths[result.best_result_index];

    ASSERT_EQ(3u, best_path->certs.size());
    EXPECT_EQ(best_path->certs[0], test_.chain[0]);
    EXPECT_EQ(best_path->certs[1], test_.chain[1]);
    EXPECT_EQ(best_path->certs[2], test_.chain[2]);
    EXPECT_TRUE(best_path->errors.ContainsHighSeverityErrors());
    best_path->errors.ContainsError(cert_errors::kDistrustedByTrustStore);
  }
}

class PathBuilderCheckPathAfterVerificationTest
    : public PathBuilderSimpleChainTest {};

TEST_F(PathBuilderCheckPathAfterVerificationTest, NoOpToValidPath) {
  StrictMock<MockPathBuilderDelegate> delegate;

  EXPECT_CALL(delegate, CheckPathAfterVerification(_, _));

  CertPathBuilder::Result result = RunPathBuilder(nullptr, &delegate);
  EXPECT_TRUE(result.HasValidPath());
}

DEFINE_CERT_ERROR_ID(kWarningFromDelegate, "Warning from delegate");

class AddWarningPathBuilderDelegate : public CertPathBuilderDelegateBase {
 public:
  void CheckPathAfterVerification(const CertPathBuilder &path_builder,
                                  CertPathBuilderResultPath *path) override {
    path->errors.GetErrorsForCert(1)->AddWarning(kWarningFromDelegate, nullptr);
  }
};

TEST_F(PathBuilderCheckPathAfterVerificationTest, AddsWarningToValidPath) {
  AddWarningPathBuilderDelegate delegate;
  CertPathBuilder::Result result = RunPathBuilder(nullptr, &delegate);
  ASSERT_TRUE(result.HasValidPath());

  const CertErrors *cert1_errors =
      result.GetBestValidPath()->errors.GetErrorsForCert(1);
  ASSERT_TRUE(cert1_errors);
  EXPECT_TRUE(cert1_errors->ContainsErrorWithSeverity(
      kWarningFromDelegate, CertError::SEVERITY_WARNING));

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();
}

TEST_F(PathBuilderCheckPathAfterVerificationTest, TestVerifyErrorMapping) {
  struct error_mapping {
    CertErrorId internal_error;
    VerifyError::StatusCode code;
  };
  struct error_mapping AllErrors[] = {
      {cert_errors::kInternalError,
       VerifyError::StatusCode::VERIFICATION_FAILURE},
      {cert_errors::kValidityFailedNotAfter,
       VerifyError::StatusCode::CERTIFICATE_EXPIRED},
      {cert_errors::kValidityFailedNotBefore,
       VerifyError::StatusCode::CERTIFICATE_NOT_YET_VALID},
      {cert_errors::kDistrustedByTrustStore,
       VerifyError::StatusCode::PATH_NOT_FOUND},
      {cert_errors::kSignatureAlgorithmMismatch,
       VerifyError::StatusCode::CERTIFICATE_INVALID},
      {cert_errors::kChainIsEmpty,
       VerifyError::StatusCode::VERIFICATION_FAILURE},
      {cert_errors::kUnconsumedCriticalExtension,
       VerifyError::StatusCode::CERTIFICATE_INVALID},
      {cert_errors::kKeyCertSignBitNotSet,
       VerifyError::StatusCode::CERTIFICATE_INVALID},
      {cert_errors::kMaxPathLengthViolated,
       VerifyError::StatusCode::PATH_NOT_FOUND},
      {cert_errors::kBasicConstraintsIndicatesNotCa,
       VerifyError::StatusCode::CERTIFICATE_INVALID},
      {cert_errors::kTargetCertShouldNotBeCa,
       VerifyError::StatusCode::CERTIFICATE_INVALID},
      {cert_errors::kMissingBasicConstraints,
       VerifyError::StatusCode::CERTIFICATE_INVALID},
      {cert_errors::kNotPermittedByNameConstraints,
       VerifyError::StatusCode::CERTIFICATE_INVALID},
      {cert_errors::kTooManyNameConstraintChecks,
       VerifyError::StatusCode::CERTIFICATE_INVALID},
      {cert_errors::kSubjectDoesNotMatchIssuer,
       VerifyError::StatusCode::PATH_NOT_FOUND},
      {cert_errors::kVerifySignedDataFailed,
       VerifyError::StatusCode::CERTIFICATE_INVALID_SIGNATURE},
      {cert_errors::kSignatureAlgorithmsDifferentEncoding,
       VerifyError::StatusCode::CERTIFICATE_INVALID},
      {cert_errors::kEkuLacksServerAuth,
       VerifyError::StatusCode::CERTIFICATE_NO_MATCHING_EKU},
      {cert_errors::kEkuLacksServerAuthButHasAnyEKU,
       VerifyError::StatusCode::CERTIFICATE_NO_MATCHING_EKU},
      {cert_errors::kEkuLacksClientAuth,
       VerifyError::StatusCode::CERTIFICATE_NO_MATCHING_EKU},
      {cert_errors::kEkuLacksClientAuthButHasAnyEKU,
       VerifyError::StatusCode::CERTIFICATE_NO_MATCHING_EKU},
      {cert_errors::kEkuLacksClientAuthOrServerAuth,
       VerifyError::StatusCode::CERTIFICATE_NO_MATCHING_EKU},
      {cert_errors::kEkuHasProhibitedOCSPSigning,
       VerifyError::StatusCode::CERTIFICATE_INVALID},
      {cert_errors::kEkuHasProhibitedTimeStamping,
       VerifyError::StatusCode::CERTIFICATE_INVALID},
      {cert_errors::kEkuHasProhibitedCodeSigning,
       VerifyError::StatusCode::CERTIFICATE_INVALID},
      {cert_errors::kEkuNotPresent,
       VerifyError::StatusCode::CERTIFICATE_INVALID},
      {cert_errors::kCertIsNotTrustAnchor,
       VerifyError::StatusCode::PATH_NOT_FOUND},
      {cert_errors::kNoValidPolicy,
       VerifyError::StatusCode::CERTIFICATE_INVALID},
      {cert_errors::kPolicyMappingAnyPolicy,
       VerifyError::StatusCode::CERTIFICATE_INVALID},
      {cert_errors::kFailedParsingSpki,
       VerifyError::StatusCode::CERTIFICATE_INVALID},
      {cert_errors::kUnacceptableSignatureAlgorithm,
       VerifyError::StatusCode::CERTIFICATE_UNSUPPORTED_SIGNATURE_ALGORITHM},
      {cert_errors::kUnacceptablePublicKey,
       VerifyError::StatusCode::CERTIFICATE_UNSUPPORTED_KEY},
      {cert_errors::kCertificateRevoked,
       VerifyError::StatusCode::CERTIFICATE_REVOKED},
      {cert_errors::kNoRevocationMechanism,
       VerifyError::StatusCode::CERTIFICATE_NO_REVOCATION_MECHANISM},
      {cert_errors::kUnableToCheckRevocation,
       VerifyError::StatusCode::CERTIFICATE_UNABLE_TO_CHECK_REVOCATION},
      {cert_errors::kNoIssuersFound, VerifyError::StatusCode::PATH_NOT_FOUND},
      {cert_errors::kDeadlineExceeded,
       VerifyError::StatusCode::PATH_DEADLINE_EXCEEDED},
      {cert_errors::kIterationLimitExceeded,
       VerifyError::StatusCode::PATH_ITERATION_COUNT_EXCEEDED},
      {cert_errors::kDepthLimitExceeded,
       VerifyError::StatusCode::PATH_DEPTH_LIMIT_REACHED},
  };

  for (struct error_mapping mapping : AllErrors) {
    AddWarningPathBuilderDelegate delegate;
    CertPathBuilder::Result result = RunPathBuilder(nullptr, &delegate);
    ASSERT_TRUE(result.HasValidPath());

    CertErrors *errors =
        (CertErrors *)result.GetBestValidPath()->errors.GetErrorsForCert(1);
    errors->AddError(mapping.internal_error, nullptr);

    VerifyError error = result.GetBestPathVerifyError();
    ASSERT_EQ(error.Code(), mapping.code)
        << error.DiagnosticString();
  }
}

TEST_F(PathBuilderCheckPathAfterVerificationTest,
       TestVerifyErrorMulipleMapping) {
  AddWarningPathBuilderDelegate delegate;
  CertPathBuilder::Result result = RunPathBuilder(nullptr, &delegate);
  ASSERT_TRUE(result.HasValidPath());

  CertErrors *errors =
      (CertErrors *)result.GetBestValidPath()->errors.GetErrorsForCert(1);
  errors->AddError(cert_errors::kEkuNotPresent, nullptr);
  errors->AddError(cert_errors::kNoValidPolicy, nullptr);

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_MULTIPLE_ERRORS)
      << error.DiagnosticString();
}

DEFINE_CERT_ERROR_ID(kErrorFromDelegate, "Error from delegate");

class AddErrorPathBuilderDelegate : public CertPathBuilderDelegateBase {
 public:
  void CheckPathAfterVerification(const CertPathBuilder &path_builder,
                                  CertPathBuilderResultPath *path) override {
    path->errors.GetErrorsForCert(2)->AddError(kErrorFromDelegate, nullptr);
  }
};

TEST_F(PathBuilderCheckPathAfterVerificationTest, AddsErrorToValidPath) {
  AddErrorPathBuilderDelegate delegate;
  CertPathBuilder::Result result = RunPathBuilder(nullptr, &delegate);

  ASSERT_FALSE(result.HasValidPath());

  ASSERT_LT(result.best_result_index, result.paths.size());
  const CertPathBuilderResultPath *failed_path =
      result.paths[result.best_result_index].get();
  ASSERT_TRUE(failed_path);

  const CertErrors *cert2_errors = failed_path->errors.GetErrorsForCert(2);
  ASSERT_TRUE(cert2_errors);
  EXPECT_TRUE(cert2_errors->ContainsError(kErrorFromDelegate));

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::CERTIFICATE_INVALID)
      << error.DiagnosticString();
}

class AddOtherErrorPathBuilderDelegate : public CertPathBuilderDelegateBase {
 public:
  void CheckPathAfterVerification(const CertPathBuilder &path_builder,
                                  CertPathBuilderResultPath *path) override {
    path->errors.GetOtherErrors()->AddError(kErrorFromDelegate, nullptr);
  }
};

TEST_F(PathBuilderCheckPathAfterVerificationTest, AddsErrorToOtherErrors) {
  AddOtherErrorPathBuilderDelegate delegate;
  CertPathBuilder::Result result = RunPathBuilder(nullptr, &delegate);

  ASSERT_FALSE(result.HasValidPath());

  ASSERT_LT(result.best_result_index, result.paths.size());
  const CertPathBuilderResultPath *failed_path =
      result.paths[result.best_result_index].get();
  ASSERT_TRUE(failed_path);

  const CertErrors *other_errors = failed_path->errors.GetOtherErrors();
  ASSERT_TRUE(other_errors);
  EXPECT_TRUE(other_errors->ContainsError(kErrorFromDelegate));

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::VERIFICATION_FAILURE)
      << error.DiagnosticString();
}

TEST_F(PathBuilderCheckPathAfterVerificationTest, NoopToAlreadyInvalidPath) {
  StrictMock<MockPathBuilderDelegate> delegate;

  EXPECT_CALL(delegate, CheckPathAfterVerification(_, _));

  CertPathBuilder::Result result = RunPathBuilder(test_.chain[1], &delegate);
  EXPECT_FALSE(result.HasValidPath());

  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_NOT_FOUND)
      << error.DiagnosticString();
}

struct DelegateData : public CertPathBuilderDelegateData {
  int value = 0xB33F;
};

class SetsDelegateDataPathBuilderDelegate : public CertPathBuilderDelegateBase {
 public:
  void CheckPathAfterVerification(const CertPathBuilder &path_builder,
                                  CertPathBuilderResultPath *path) override {
    path->delegate_data = std::make_unique<DelegateData>();
  }
};

TEST_F(PathBuilderCheckPathAfterVerificationTest, SetsDelegateData) {
  SetsDelegateDataPathBuilderDelegate delegate;
  CertPathBuilder::Result result = RunPathBuilder(nullptr, &delegate);
  ASSERT_TRUE(result.HasValidPath());

  DelegateData *data = reinterpret_cast<DelegateData *>(
      result.GetBestValidPath()->delegate_data.get());

  EXPECT_EQ(0xB33F, data->value);
}

TEST(PathBuilderPrioritizationTest, DatePrioritization) {
  std::string test_dir =
      "testdata/path_builder_unittest/validity_date_prioritization/";
  std::shared_ptr<const ParsedCertificate> root =
      ReadCertFromFile(test_dir + "root.pem");
  ASSERT_TRUE(root);
  std::shared_ptr<const ParsedCertificate> int_ac =
      ReadCertFromFile(test_dir + "int_ac.pem");
  ASSERT_TRUE(int_ac);
  std::shared_ptr<const ParsedCertificate> int_ad =
      ReadCertFromFile(test_dir + "int_ad.pem");
  ASSERT_TRUE(int_ad);
  std::shared_ptr<const ParsedCertificate> int_bc =
      ReadCertFromFile(test_dir + "int_bc.pem");
  ASSERT_TRUE(int_bc);
  std::shared_ptr<const ParsedCertificate> int_bd =
      ReadCertFromFile(test_dir + "int_bd.pem");
  ASSERT_TRUE(int_bd);
  std::shared_ptr<const ParsedCertificate> target =
      ReadCertFromFile(test_dir + "target.pem");
  ASSERT_TRUE(target);

  SimplePathBuilderDelegate delegate(
      1024, SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1);
  der::GeneralizedTime verify_time = {2017, 3, 1, 0, 0, 0};

  TrustStoreInMemory trust_store;
  trust_store.AddDistrustedCertificateForTest(root);

  for (bool reverse_input_order : {false, true}) {
    SCOPED_TRACE(reverse_input_order);

    CertIssuerSourceStatic intermediates;

    if (reverse_input_order) {
      intermediates.AddCert(int_bd);
      intermediates.AddCert(int_bc);
      intermediates.AddCert(int_ad);
      intermediates.AddCert(int_ac);
    } else {
      intermediates.AddCert(int_ac);
      intermediates.AddCert(int_ad);
      intermediates.AddCert(int_bc);
      intermediates.AddCert(int_bd);
    }

    CertPathBuilder path_builder(
        target, &trust_store, &delegate, verify_time, KeyPurpose::ANY_EKU,
        InitialExplicitPolicy::kFalse, {der::Input(kAnyPolicyOid)},
        InitialPolicyMappingInhibit::kFalse, InitialAnyPolicyInhibit::kFalse);
    path_builder.AddCertIssuerSource(&intermediates);

    CertPathBuilder::Result result = path_builder.Run();
    EXPECT_FALSE(result.HasValidPath());

    VerifyError error = result.GetBestPathVerifyError();
    ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_NOT_FOUND)
        << error.DiagnosticString();

    ASSERT_EQ(4U, result.paths.size());

    EXPECT_FALSE(result.paths[0]->IsValid());
    ASSERT_EQ(3U, result.paths[0]->certs.size());
    EXPECT_EQ(target, result.paths[0]->certs[0]);
    EXPECT_EQ(int_bd, result.paths[0]->certs[1]);
    EXPECT_EQ(root, result.paths[0]->certs[2]);

    EXPECT_FALSE(result.paths[1]->IsValid());
    ASSERT_EQ(3U, result.paths[1]->certs.size());
    EXPECT_EQ(target, result.paths[1]->certs[0]);
    EXPECT_EQ(int_bc, result.paths[1]->certs[1]);
    EXPECT_EQ(root, result.paths[1]->certs[2]);

    EXPECT_FALSE(result.paths[2]->IsValid());
    ASSERT_EQ(3U, result.paths[2]->certs.size());
    EXPECT_EQ(target, result.paths[2]->certs[0]);
    EXPECT_EQ(int_ad, result.paths[2]->certs[1]);
    EXPECT_EQ(root, result.paths[2]->certs[2]);

    EXPECT_FALSE(result.paths[3]->IsValid());
    ASSERT_EQ(3U, result.paths[3]->certs.size());
    EXPECT_EQ(target, result.paths[3]->certs[0]);
    EXPECT_EQ(int_ac, result.paths[3]->certs[1]);
    EXPECT_EQ(root, result.paths[3]->certs[2]);
  }
}

TEST(PathBuilderPrioritizationTest, KeyIdPrioritization) {
  std::string test_dir =
      "testdata/path_builder_unittest/key_id_prioritization/";
  std::shared_ptr<const ParsedCertificate> root =
      ReadCertFromFile(test_dir + "root.pem");
  ASSERT_TRUE(root);
  std::shared_ptr<const ParsedCertificate> int_matching_ski_a =
      ReadCertFromFile(test_dir + "int_matching_ski_a.pem");
  ASSERT_TRUE(int_matching_ski_a);
  std::shared_ptr<const ParsedCertificate> int_matching_ski_b =
      ReadCertFromFile(test_dir + "int_matching_ski_b.pem");
  ASSERT_TRUE(int_matching_ski_b);
  std::shared_ptr<const ParsedCertificate> int_no_ski_a =
      ReadCertFromFile(test_dir + "int_no_ski_a.pem");
  ASSERT_TRUE(int_no_ski_a);
  std::shared_ptr<const ParsedCertificate> int_no_ski_b =
      ReadCertFromFile(test_dir + "int_no_ski_b.pem");
  ASSERT_TRUE(int_no_ski_b);
  std::shared_ptr<const ParsedCertificate> int_different_ski_a =
      ReadCertFromFile(test_dir + "int_different_ski_a.pem");
  ASSERT_TRUE(int_different_ski_a);
  std::shared_ptr<const ParsedCertificate> int_different_ski_b =
      ReadCertFromFile(test_dir + "int_different_ski_b.pem");
  ASSERT_TRUE(int_different_ski_b);
  std::shared_ptr<const ParsedCertificate> target =
      ReadCertFromFile(test_dir + "target.pem");
  ASSERT_TRUE(target);

  SimplePathBuilderDelegate delegate(
      1024, SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1);
  der::GeneralizedTime verify_time = {2017, 3, 1, 0, 0, 0};

  TrustStoreInMemory trust_store;
  trust_store.AddDistrustedCertificateForTest(root);

  for (bool reverse_input_order : {false, true}) {
    SCOPED_TRACE(reverse_input_order);

    CertIssuerSourceStatic intermediates;

    if (reverse_input_order) {
      intermediates.AddCert(int_different_ski_b);
      intermediates.AddCert(int_different_ski_a);
      intermediates.AddCert(int_no_ski_b);
      intermediates.AddCert(int_no_ski_a);
      intermediates.AddCert(int_matching_ski_b);
      intermediates.AddCert(int_matching_ski_a);
    } else {
      intermediates.AddCert(int_matching_ski_a);
      intermediates.AddCert(int_matching_ski_b);
      intermediates.AddCert(int_no_ski_a);
      intermediates.AddCert(int_no_ski_b);
      intermediates.AddCert(int_different_ski_a);
      intermediates.AddCert(int_different_ski_b);
    }

    CertPathBuilder path_builder(
        target, &trust_store, &delegate, verify_time, KeyPurpose::ANY_EKU,
        InitialExplicitPolicy::kFalse, {der::Input(kAnyPolicyOid)},
        InitialPolicyMappingInhibit::kFalse, InitialAnyPolicyInhibit::kFalse);
    path_builder.AddCertIssuerSource(&intermediates);

    CertPathBuilder::Result result = path_builder.Run();
    EXPECT_FALSE(result.HasValidPath());
    ASSERT_EQ(6U, result.paths.size());

    EXPECT_FALSE(result.paths[0]->IsValid());
    ASSERT_EQ(3U, result.paths[0]->certs.size());
    EXPECT_EQ(target, result.paths[0]->certs[0]);
    EXPECT_EQ(int_matching_ski_b, result.paths[0]->certs[1]);
    EXPECT_EQ(root, result.paths[0]->certs[2]);

    EXPECT_FALSE(result.paths[1]->IsValid());
    ASSERT_EQ(3U, result.paths[1]->certs.size());
    EXPECT_EQ(target, result.paths[1]->certs[0]);
    EXPECT_EQ(int_matching_ski_a, result.paths[1]->certs[1]);
    EXPECT_EQ(root, result.paths[1]->certs[2]);

    EXPECT_FALSE(result.paths[2]->IsValid());
    ASSERT_EQ(3U, result.paths[2]->certs.size());
    EXPECT_EQ(target, result.paths[2]->certs[0]);
    EXPECT_EQ(int_no_ski_b, result.paths[2]->certs[1]);
    EXPECT_EQ(root, result.paths[2]->certs[2]);

    EXPECT_FALSE(result.paths[3]->IsValid());
    ASSERT_EQ(3U, result.paths[3]->certs.size());
    EXPECT_EQ(target, result.paths[3]->certs[0]);
    EXPECT_EQ(int_no_ski_a, result.paths[3]->certs[1]);
    EXPECT_EQ(root, result.paths[3]->certs[2]);

    EXPECT_FALSE(result.paths[4]->IsValid());
    ASSERT_EQ(3U, result.paths[4]->certs.size());
    EXPECT_EQ(target, result.paths[4]->certs[0]);
    EXPECT_EQ(int_different_ski_b, result.paths[4]->certs[1]);
    EXPECT_EQ(root, result.paths[4]->certs[2]);

    EXPECT_FALSE(result.paths[5]->IsValid());
    ASSERT_EQ(3U, result.paths[5]->certs.size());
    EXPECT_EQ(target, result.paths[5]->certs[0]);
    EXPECT_EQ(int_different_ski_a, result.paths[5]->certs[1]);
    EXPECT_EQ(root, result.paths[5]->certs[2]);
  }
}

TEST(PathBuilderPrioritizationTest, TrustAndKeyIdPrioritization) {
  std::string test_dir =
      "testdata/path_builder_unittest/key_id_prioritization/";
  std::shared_ptr<const ParsedCertificate> root =
      ReadCertFromFile(test_dir + "root.pem");
  ASSERT_TRUE(root);
  std::shared_ptr<const ParsedCertificate> trusted_and_matching =
      ReadCertFromFile(test_dir + "int_matching_ski_a.pem");
  ASSERT_TRUE(trusted_and_matching);
  std::shared_ptr<const ParsedCertificate> matching =
      ReadCertFromFile(test_dir + "int_matching_ski_b.pem");
  ASSERT_TRUE(matching);
  std::shared_ptr<const ParsedCertificate> distrusted_and_matching =
      ReadCertFromFile(test_dir + "int_matching_ski_c.pem");
  ASSERT_TRUE(distrusted_and_matching);
  std::shared_ptr<const ParsedCertificate> trusted_and_no_match_data =
      ReadCertFromFile(test_dir + "int_no_ski_a.pem");
  ASSERT_TRUE(trusted_and_no_match_data);
  std::shared_ptr<const ParsedCertificate> no_match_data =
      ReadCertFromFile(test_dir + "int_no_ski_b.pem");
  ASSERT_TRUE(no_match_data);
  std::shared_ptr<const ParsedCertificate> distrusted_and_no_match_data =
      ReadCertFromFile(test_dir + "int_no_ski_c.pem");
  ASSERT_TRUE(distrusted_and_no_match_data);
  std::shared_ptr<const ParsedCertificate> trusted_and_mismatch =
      ReadCertFromFile(test_dir + "int_different_ski_a.pem");
  ASSERT_TRUE(trusted_and_mismatch);
  std::shared_ptr<const ParsedCertificate> mismatch =
      ReadCertFromFile(test_dir + "int_different_ski_b.pem");
  ASSERT_TRUE(mismatch);
  std::shared_ptr<const ParsedCertificate> distrusted_and_mismatch =
      ReadCertFromFile(test_dir + "int_different_ski_c.pem");
  ASSERT_TRUE(distrusted_and_mismatch);
  std::shared_ptr<const ParsedCertificate> target =
      ReadCertFromFile(test_dir + "target.pem");
  ASSERT_TRUE(target);

  SimplePathBuilderDelegate delegate(
      1024, SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1);
  der::GeneralizedTime verify_time = {2017, 3, 1, 0, 0, 0};

  for (bool reverse_input_order : {false, true}) {
    SCOPED_TRACE(reverse_input_order);

    TrustStoreInMemory trust_store;

    if (reverse_input_order) {
      trust_store.AddTrustAnchor(trusted_and_matching);
      trust_store.AddCertificateWithUnspecifiedTrust(matching);
      trust_store.AddDistrustedCertificateForTest(distrusted_and_matching);
      trust_store.AddTrustAnchor(trusted_and_no_match_data);
      trust_store.AddCertificateWithUnspecifiedTrust(no_match_data);
      trust_store.AddDistrustedCertificateForTest(distrusted_and_no_match_data);
      trust_store.AddTrustAnchor(trusted_and_mismatch);
      trust_store.AddCertificateWithUnspecifiedTrust(mismatch);
      trust_store.AddDistrustedCertificateForTest(distrusted_and_mismatch);
    } else {
      trust_store.AddDistrustedCertificateForTest(distrusted_and_matching);
      trust_store.AddCertificateWithUnspecifiedTrust(no_match_data);
      trust_store.AddTrustAnchor(trusted_and_no_match_data);
      trust_store.AddTrustAnchor(trusted_and_matching);
      trust_store.AddCertificateWithUnspecifiedTrust(matching);
      trust_store.AddCertificateWithUnspecifiedTrust(mismatch);
      trust_store.AddDistrustedCertificateForTest(distrusted_and_no_match_data);
      trust_store.AddTrustAnchor(trusted_and_mismatch);
      trust_store.AddDistrustedCertificateForTest(distrusted_and_mismatch);
    }

    trust_store.AddDistrustedCertificateForTest(root);

    CertPathBuilder path_builder(
        target, &trust_store, &delegate, verify_time, KeyPurpose::ANY_EKU,
        InitialExplicitPolicy::kFalse, {der::Input(kAnyPolicyOid)},
        InitialPolicyMappingInhibit::kFalse, InitialAnyPolicyInhibit::kFalse);
    path_builder.SetValidPathLimit(0);

    CertPathBuilder::Result result = path_builder.Run();
    EXPECT_TRUE(result.HasValidPath());
    ASSERT_EQ(9U, result.paths.size());

    EXPECT_TRUE(result.paths[0]->IsValid());
    ASSERT_EQ(2U, result.paths[0]->certs.size());
    EXPECT_EQ(target, result.paths[0]->certs[0]);
    EXPECT_EQ(trusted_and_matching, result.paths[0]->certs[1]);

    EXPECT_TRUE(result.paths[1]->IsValid());
    ASSERT_EQ(2U, result.paths[1]->certs.size());
    EXPECT_EQ(target, result.paths[1]->certs[0]);
    EXPECT_EQ(trusted_and_no_match_data, result.paths[1]->certs[1]);

    EXPECT_FALSE(result.paths[2]->IsValid());
    ASSERT_EQ(3U, result.paths[2]->certs.size());
    EXPECT_EQ(target, result.paths[2]->certs[0]);
    EXPECT_EQ(matching, result.paths[2]->certs[1]);
    EXPECT_EQ(root, result.paths[2]->certs[2]);

    EXPECT_FALSE(result.paths[3]->IsValid());
    ASSERT_EQ(3U, result.paths[3]->certs.size());
    EXPECT_EQ(target, result.paths[3]->certs[0]);
    EXPECT_EQ(no_match_data, result.paths[3]->certs[1]);
    EXPECT_EQ(root, result.paths[3]->certs[2]);

    EXPECT_FALSE(result.paths[4]->IsValid());
    ASSERT_EQ(2U, result.paths[4]->certs.size());
    EXPECT_EQ(target, result.paths[4]->certs[0]);
    EXPECT_EQ(trusted_and_mismatch, result.paths[4]->certs[1]);

    EXPECT_FALSE(result.paths[5]->IsValid());
    ASSERT_EQ(3U, result.paths[5]->certs.size());
    EXPECT_EQ(target, result.paths[5]->certs[0]);
    EXPECT_EQ(mismatch, result.paths[5]->certs[1]);
    EXPECT_EQ(root, result.paths[5]->certs[2]);

    EXPECT_FALSE(result.paths[6]->IsValid());
    ASSERT_EQ(2U, result.paths[6]->certs.size());
    EXPECT_EQ(target, result.paths[6]->certs[0]);
    EXPECT_EQ(distrusted_and_matching, result.paths[6]->certs[1]);

    EXPECT_FALSE(result.paths[7]->IsValid());
    ASSERT_EQ(2U, result.paths[7]->certs.size());
    EXPECT_EQ(target, result.paths[7]->certs[0]);
    EXPECT_EQ(distrusted_and_no_match_data, result.paths[7]->certs[1]);

    EXPECT_FALSE(result.paths[8]->IsValid());
    ASSERT_EQ(2U, result.paths[8]->certs.size());
    EXPECT_EQ(target, result.paths[8]->certs[0]);
    EXPECT_EQ(distrusted_and_mismatch, result.paths[8]->certs[1]);
  }
}

TEST(PathBuilderPrioritizationTest, KeyIdNameAndSerialPrioritization) {
  std::string test_dir =
      "testdata/path_builder_unittest/key_id_name_and_serial_prioritization/";
  std::shared_ptr<const ParsedCertificate> root =
      ReadCertFromFile(test_dir + "root.pem");
  ASSERT_TRUE(root);
  std::shared_ptr<const ParsedCertificate> root2 =
      ReadCertFromFile(test_dir + "root2.pem");
  ASSERT_TRUE(root2);
  std::shared_ptr<const ParsedCertificate> int_matching =
      ReadCertFromFile(test_dir + "int_matching.pem");
  ASSERT_TRUE(int_matching);
  std::shared_ptr<const ParsedCertificate> int_match_name_only =
      ReadCertFromFile(test_dir + "int_match_name_only.pem");
  ASSERT_TRUE(int_match_name_only);
  std::shared_ptr<const ParsedCertificate> int_mismatch =
      ReadCertFromFile(test_dir + "int_mismatch.pem");
  ASSERT_TRUE(int_mismatch);
  std::shared_ptr<const ParsedCertificate> target =
      ReadCertFromFile(test_dir + "target.pem");
  ASSERT_TRUE(target);

  SimplePathBuilderDelegate delegate(
      1024, SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1);
  der::GeneralizedTime verify_time = {2017, 3, 1, 0, 0, 0};

  TrustStoreInMemory trust_store;
  trust_store.AddDistrustedCertificateForTest(root);
  trust_store.AddDistrustedCertificateForTest(root2);

  for (bool reverse_input_order : {false, true}) {
    SCOPED_TRACE(reverse_input_order);

    CertIssuerSourceStatic intermediates;

    if (reverse_input_order) {
      intermediates.AddCert(int_mismatch);
      intermediates.AddCert(int_match_name_only);
      intermediates.AddCert(int_matching);
    } else {
      intermediates.AddCert(int_matching);
      intermediates.AddCert(int_match_name_only);
      intermediates.AddCert(int_mismatch);
    }

    CertPathBuilder path_builder(
        target, &trust_store, &delegate, verify_time, KeyPurpose::ANY_EKU,
        InitialExplicitPolicy::kFalse, {der::Input(kAnyPolicyOid)},
        InitialPolicyMappingInhibit::kFalse, InitialAnyPolicyInhibit::kFalse);
    path_builder.AddCertIssuerSource(&intermediates);

    CertPathBuilder::Result result = path_builder.Run();
    EXPECT_FALSE(result.HasValidPath());
    ASSERT_EQ(3U, result.paths.size());
    VerifyError error = result.GetBestPathVerifyError();
    ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_NOT_FOUND)
        << error.DiagnosticString();

    EXPECT_FALSE(result.paths[0]->IsValid());
    ASSERT_EQ(3U, result.paths[0]->certs.size());
    EXPECT_EQ(target, result.paths[0]->certs[0]);
    EXPECT_EQ(int_mismatch, result.paths[0]->certs[1]);
    EXPECT_EQ(root2, result.paths[0]->certs[2]);

    EXPECT_FALSE(result.paths[1]->IsValid());
    ASSERT_EQ(3U, result.paths[1]->certs.size());
    EXPECT_EQ(target, result.paths[1]->certs[0]);
    EXPECT_EQ(int_match_name_only, result.paths[1]->certs[1]);
    EXPECT_EQ(root, result.paths[1]->certs[2]);

    EXPECT_FALSE(result.paths[2]->IsValid());
    ASSERT_EQ(3U, result.paths[2]->certs.size());
    EXPECT_EQ(target, result.paths[2]->certs[0]);
    EXPECT_EQ(int_matching, result.paths[2]->certs[1]);
    EXPECT_EQ(root, result.paths[2]->certs[2]);
  }
}

TEST(PathBuilderPrioritizationTest, SelfIssuedPrioritization) {
  std::string test_dir =
      "testdata/path_builder_unittest/self_issued_prioritization/";
  std::shared_ptr<const ParsedCertificate> root1 =
      ReadCertFromFile(test_dir + "root1.pem");
  ASSERT_TRUE(root1);
  std::shared_ptr<const ParsedCertificate> root1_cross =
      ReadCertFromFile(test_dir + "root1_cross.pem");
  ASSERT_TRUE(root1_cross);
  std::shared_ptr<const ParsedCertificate> target =
      ReadCertFromFile(test_dir + "target.pem");
  ASSERT_TRUE(target);

  SimplePathBuilderDelegate delegate(
      1024, SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1);
  der::GeneralizedTime verify_time = {2017, 3, 1, 0, 0, 0};

  TrustStoreInMemory trust_store;
  trust_store.AddTrustAnchor(root1);
  trust_store.AddTrustAnchor(root1_cross);
  CertPathBuilder path_builder(
      target, &trust_store, &delegate, verify_time, KeyPurpose::ANY_EKU,
      InitialExplicitPolicy::kFalse, {der::Input(kAnyPolicyOid)},
      InitialPolicyMappingInhibit::kFalse, InitialAnyPolicyInhibit::kFalse);
  path_builder.SetValidPathLimit(0);

  CertPathBuilder::Result result = path_builder.Run();
  EXPECT_TRUE(result.HasValidPath());
  VerifyError error = result.GetBestPathVerifyError();
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_VERIFIED)
      << error.DiagnosticString();

  ASSERT_EQ(2U, result.paths.size());

  EXPECT_TRUE(result.paths[0]->IsValid());
  ASSERT_EQ(2U, result.paths[0]->certs.size());
  EXPECT_EQ(target, result.paths[0]->certs[0]);
  EXPECT_EQ(root1, result.paths[0]->certs[1]);

  EXPECT_TRUE(result.paths[1]->IsValid());
  ASSERT_EQ(2U, result.paths[1]->certs.size());
  EXPECT_EQ(target, result.paths[1]->certs[0]);
  EXPECT_EQ(root1_cross, result.paths[1]->certs[1]);
}

}

BSSL_NAMESPACE_END
