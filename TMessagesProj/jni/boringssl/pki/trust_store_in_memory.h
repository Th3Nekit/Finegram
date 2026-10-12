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

#ifndef BSSL_PKI_TRUST_STORE_IN_MEMORY_H_
#define BSSL_PKI_TRUST_STORE_IN_MEMORY_H_

#include <set>
#include <unordered_map>

#include <openssl/base.h>

#include "trust_store.h"

BSSL_NAMESPACE_BEGIN

class OPENSSL_EXPORT TrustStoreInMemory : public TrustStore {
 public:
  TrustStoreInMemory();

  TrustStoreInMemory(const TrustStoreInMemory &) = delete;
  TrustStoreInMemory &operator=(const TrustStoreInMemory &) = delete;

  ~TrustStoreInMemory() override;

  bool IsEmpty() const;

  void Clear();

  void AddCertificate(std::shared_ptr<const ParsedCertificate> cert,
                      const CertificateTrust &trust);

  void AddTrustAnchor(std::shared_ptr<const ParsedCertificate> cert);

  void AddTrustAnchorWithExpiration(
      std::shared_ptr<const ParsedCertificate> cert);

  void AddTrustAnchorWithConstraints(
      std::shared_ptr<const ParsedCertificate> cert);

  void AddDistrustedCertificateForTest(
      std::shared_ptr<const ParsedCertificate> cert);

  void AddDistrustedCertificateBySPKI(std::string spki);

  void AddCertificateWithUnspecifiedTrust(
      std::shared_ptr<const ParsedCertificate> cert);

  void SyncGetIssuersOf(const ParsedCertificate *cert,
                        ParsedCertificateList *issuers) override;
  CertificateTrust GetTrust(const ParsedCertificate *cert) override;

  bool Contains(const ParsedCertificate *cert) const;

 private:
  struct Entry {
    Entry();
    Entry(const Entry &other);
    ~Entry();

    std::shared_ptr<const ParsedCertificate> cert;
    CertificateTrust trust;
  };

  std::unordered_multimap<std::string_view, Entry> entries_;

  std::set<std::string, std::less<>> distrusted_spkis_;

  const Entry *GetEntry(const ParsedCertificate *cert) const;
};

BSSL_NAMESPACE_END

#endif
