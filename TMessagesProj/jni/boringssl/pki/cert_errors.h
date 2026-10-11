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

// ----------------------------
// Overview of error design
// ----------------------------
//
// Certificate path building/validation/parsing may emit a sequence of errors
// and warnings.
//
// Each individual error/warning entry (CertError) is comprised of:
//
//   * A unique identifier.
//
//     This serves similarly to an error code, and is used to query if a
//     particular error/warning occurred.
//
//   * [optional] A parameters object.
//
//     Nodes may attach a heap-allocated subclass of CertErrorParams to carry
//     extra information that is used when reporting the error. For instance
//     a parsing error may describe where in the DER the failure happened, or
//     what the unexpected value was.
//
// A collection of errors is represented by the CertErrors object. This may be
// used to group errors that have a common context, such as all the
// errors/warnings that apply to a specific certificate.
//
// Lastly, CertPathErrors composes multiple CertErrors -- one for each
// certificate in the verified chain.
//
// ----------------------------
// Defining new errors
// ----------------------------
//
// The error IDs are extensible and do not need to be centrally defined.
//
// To define a new error use the macro DEFINE_CERT_ERROR_ID() in a .cc file.
// If consumers are to be able to query for this error then the symbol should
// also be exposed in a header file.
//
// Error IDs are in truth string literals, whose pointer value will be unique
// per process.

#ifndef BSSL_PKI_CERT_ERRORS_H_
#define BSSL_PKI_CERT_ERRORS_H_

#include <memory>
#include <vector>

#include <openssl/base.h>

#include "cert_error_id.h"
#include "parsed_certificate.h"

BSSL_NAMESPACE_BEGIN

class CertErrorParams;
class CertPathErrors;

struct OPENSSL_EXPORT CertError {
  enum Severity {
    SEVERITY_HIGH,
    SEVERITY_WARNING,
  };

  CertError();
  CertError(Severity severity, CertErrorId id,
            std::unique_ptr<CertErrorParams> params);
  CertError(CertError &&other);
  CertError &operator=(CertError &&);
  ~CertError();

  std::string ToDebugString() const;

  Severity severity;
  CertErrorId id;
  std::unique_ptr<CertErrorParams> params;
};

class OPENSSL_EXPORT CertErrors {
 public:
  CertErrors();
  CertErrors(CertErrors &&other);
  CertErrors &operator=(CertErrors &&);
  ~CertErrors();

  void Add(CertError::Severity severity, CertErrorId id,
           std::unique_ptr<CertErrorParams> params);

  void AddError(CertErrorId id, std::unique_ptr<CertErrorParams> params);
  void AddError(CertErrorId id);

  void AddWarning(CertErrorId id, std::unique_ptr<CertErrorParams> params);
  void AddWarning(CertErrorId id);

  std::string ToDebugString() const;

  bool ContainsErrorWithSeverity(CertErrorId id,
                                 CertError::Severity severity) const;

  bool ContainsError(CertErrorId id) const;

  bool ContainsAnyErrorWithSeverity(CertError::Severity severity) const;

 private:
 friend CertPathErrors;
  std::vector<CertError> nodes_;
};

class OPENSSL_EXPORT CertPathErrors {
 public:
  CertPathErrors();
  CertPathErrors(CertPathErrors &&other);
  CertPathErrors &operator=(CertPathErrors &&);
  ~CertPathErrors();

  CertErrors *GetErrorsForCert(size_t cert_index);

  const CertErrors *GetErrorsForCert(size_t cert_index) const;

  CertErrors *GetOtherErrors();
  const CertErrors *GetOtherErrors() const;

  bool ContainsError(CertErrorId id) const;

  bool ContainsAnyErrorWithSeverity(CertError::Severity severity) const;

  std::optional<CertErrorId> FindSingleHighSeverityError(
      ptrdiff_t &out_depth) const;

  bool ContainsHighSeverityErrors() const {
    return ContainsAnyErrorWithSeverity(CertError::SEVERITY_HIGH);
  }

  std::string ToDebugString(const ParsedCertificateList &certs) const;

 private:
  std::vector<CertErrors> cert_errors_;
  CertErrors other_errors_;
};

BSSL_NAMESPACE_END

#endif
