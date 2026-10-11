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

#ifndef BSSL_PKI_CERT_ISSUER_SOURCE_H_
#define BSSL_PKI_CERT_ISSUER_SOURCE_H_

#include <memory>
#include <vector>

#include <openssl/base.h>

#include "parsed_certificate.h"

BSSL_NAMESPACE_BEGIN

class OPENSSL_EXPORT CertIssuerSource {
 public:
  class OPENSSL_EXPORT Request {
   public:
    Request() = default;

    Request(const Request &) = delete;
    Request &operator=(const Request &) = delete;

    virtual ~Request() = default;

    virtual void GetNext(ParsedCertificateList *issuers) = 0;
  };

  virtual ~CertIssuerSource() = default;

  virtual void SyncGetIssuersOf(const ParsedCertificate *cert,
                                ParsedCertificateList *issuers) = 0;

  virtual void AsyncGetIssuersOf(const ParsedCertificate *cert,
                                 std::unique_ptr<Request> *out_req) = 0;
};

BSSL_NAMESPACE_END

#endif
