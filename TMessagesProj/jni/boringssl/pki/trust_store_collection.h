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

#ifndef BSSL_PKI_TRUST_STORE_COLLECTION_H_
#define BSSL_PKI_TRUST_STORE_COLLECTION_H_

#include <openssl/base.h>

#include "trust_store.h"

BSSL_NAMESPACE_BEGIN

class OPENSSL_EXPORT TrustStoreCollection : public TrustStore {
 public:
  TrustStoreCollection();

  TrustStoreCollection(const TrustStoreCollection &) = delete;
  TrustStoreCollection &operator=(const TrustStoreCollection &) = delete;

  ~TrustStoreCollection() override;

  void AddTrustStore(TrustStore *store);

  void SyncGetIssuersOf(const ParsedCertificate *cert,
                        ParsedCertificateList *issuers) override;
  CertificateTrust GetTrust(const ParsedCertificate *cert) override;

 private:
  std::vector<TrustStore *> stores_;
};

BSSL_NAMESPACE_END

#endif
