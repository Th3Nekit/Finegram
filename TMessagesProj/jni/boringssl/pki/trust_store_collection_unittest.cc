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

#include "trust_store_collection.h"

#include <gtest/gtest.h>
#include "test_helpers.h"
#include "trust_store_in_memory.h"

BSSL_NAMESPACE_BEGIN

namespace {

class TrustStoreCollectionTest : public testing::Test {
 public:
  void SetUp() override {
    ParsedCertificateList chain;
    ASSERT_TRUE(ReadCertChainFromFile(
        "testdata/verify_certificate_chain_unittest/key-rollover/oldchain.pem",
        &chain));

    ASSERT_EQ(3U, chain.size());
    target_ = chain[0];
    oldintermediate_ = chain[1];
    oldroot_ = chain[2];
    ASSERT_TRUE(target_);
    ASSERT_TRUE(oldintermediate_);
    ASSERT_TRUE(oldroot_);

    ASSERT_TRUE(
        ReadCertChainFromFile("testdata/verify_certificate_chain_unittest/"
                              "key-rollover/longrolloverchain.pem",
                              &chain));

    ASSERT_EQ(5U, chain.size());
    newintermediate_ = chain[1];
    newroot_ = chain[2];
    newrootrollover_ = chain[3];
    ASSERT_TRUE(newintermediate_);
    ASSERT_TRUE(newroot_);
    ASSERT_TRUE(newrootrollover_);
  }

 protected:
  std::shared_ptr<const ParsedCertificate> oldroot_;
  std::shared_ptr<const ParsedCertificate> newroot_;
  std::shared_ptr<const ParsedCertificate> newrootrollover_;

  std::shared_ptr<const ParsedCertificate> target_;
  std::shared_ptr<const ParsedCertificate> oldintermediate_;
  std::shared_ptr<const ParsedCertificate> newintermediate_;
};

TEST_F(TrustStoreCollectionTest, NoStores) {
  ParsedCertificateList issuers;

  TrustStoreCollection collection;
  collection.SyncGetIssuersOf(target_.get(), &issuers);

  EXPECT_TRUE(issuers.empty());
}

TEST_F(TrustStoreCollectionTest, OneStore) {
  ParsedCertificateList issuers;

  TrustStoreCollection collection;
  TrustStoreInMemory in_memory;
  in_memory.AddTrustAnchor(newroot_);
  collection.AddTrustStore(&in_memory);
  collection.SyncGetIssuersOf(newintermediate_.get(), &issuers);

  ASSERT_EQ(1U, issuers.size());
  EXPECT_EQ(newroot_.get(), issuers[0].get());

  CertificateTrust trust = collection.GetTrust(newroot_.get());
  EXPECT_EQ(CertificateTrust::ForTrustAnchor().ToDebugString(),
            trust.ToDebugString());

  trust = collection.GetTrust(oldroot_.get());
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

TEST_F(TrustStoreCollectionTest, OutputVectorsAppendedTo) {
  ParsedCertificateList issuers;

  issuers.resize(3);

  TrustStoreCollection collection;
  TrustStoreInMemory in_memory;
  in_memory.AddTrustAnchor(newroot_);
  collection.AddTrustStore(&in_memory);
  collection.SyncGetIssuersOf(newintermediate_.get(), &issuers);

  ASSERT_EQ(4U, issuers.size());
  EXPECT_EQ(newroot_.get(), issuers[3].get());

  CertificateTrust trust = collection.GetTrust(newroot_.get());
  EXPECT_EQ(CertificateTrust::ForTrustAnchor().ToDebugString(),
            trust.ToDebugString());

  trust = collection.GetTrust(newrootrollover_.get());
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

TEST_F(TrustStoreCollectionTest, TwoStores) {
  ParsedCertificateList issuers;

  TrustStoreCollection collection;
  TrustStoreInMemory in_memory1;
  TrustStoreInMemory in_memory2;
  in_memory1.AddTrustAnchor(newroot_);
  in_memory2.AddTrustAnchor(oldroot_);
  collection.AddTrustStore(&in_memory1);
  collection.AddTrustStore(&in_memory2);
  collection.SyncGetIssuersOf(newintermediate_.get(), &issuers);

  ASSERT_EQ(2U, issuers.size());
  EXPECT_EQ(newroot_.get(), issuers[0].get());
  EXPECT_EQ(oldroot_.get(), issuers[1].get());

  CertificateTrust trust = collection.GetTrust(newroot_.get());
  EXPECT_EQ(CertificateTrust::ForTrustAnchor().ToDebugString(),
            trust.ToDebugString());

  trust = collection.GetTrust(oldroot_.get());
  EXPECT_EQ(CertificateTrust::ForTrustAnchor().ToDebugString(),
            trust.ToDebugString());

  trust = collection.GetTrust(newrootrollover_.get());
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

TEST_F(TrustStoreCollectionTest, DistrustTakesPriority) {
  ParsedCertificateList issuers;

  TrustStoreCollection collection;
  TrustStoreInMemory in_memory1;
  TrustStoreInMemory in_memory2;

  in_memory1.AddTrustAnchor(newroot_);
  in_memory2.AddDistrustedCertificateForTest(newroot_);

  in_memory1.AddDistrustedCertificateForTest(oldintermediate_);
  in_memory2.AddTrustAnchor(oldintermediate_);

  collection.AddTrustStore(&in_memory1);
  collection.AddTrustStore(&in_memory2);

  CertificateTrust trust = collection.GetTrust(newroot_.get());
  EXPECT_EQ(CertificateTrust::ForDistrusted().ToDebugString(),
            trust.ToDebugString());

  trust = collection.GetTrust(oldintermediate_.get());
  EXPECT_EQ(CertificateTrust::ForDistrusted().ToDebugString(),
            trust.ToDebugString());

  trust = collection.GetTrust(newrootrollover_.get());
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

}

BSSL_NAMESPACE_END
