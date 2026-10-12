// Copyright 2017 The Chromium Authors
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

#ifndef BSSL_PKI_COMMON_CERT_ERRORS_H_
#define BSSL_PKI_COMMON_CERT_ERRORS_H_

#include <openssl/base.h>

#include "cert_errors.h"

BSSL_NAMESPACE_BEGIN
namespace cert_errors {

OPENSSL_EXPORT extern const CertErrorId kInternalError;

OPENSSL_EXPORT extern const CertErrorId kValidityFailedNotAfter;

OPENSSL_EXPORT extern const CertErrorId kValidityFailedNotBefore;

OPENSSL_EXPORT extern const CertErrorId kDistrustedByTrustStore;

OPENSSL_EXPORT extern const CertErrorId kSignatureAlgorithmMismatch;

OPENSSL_EXPORT extern const CertErrorId kChainIsEmpty;

OPENSSL_EXPORT extern const CertErrorId kUnconsumedCriticalExtension;

OPENSSL_EXPORT extern const CertErrorId kTargetCertShouldNotBeCa;

OPENSSL_EXPORT extern const CertErrorId kKeyCertSignBitNotSet;

OPENSSL_EXPORT extern const CertErrorId kKeyUsageIncorrectForRcsMlsClient;

OPENSSL_EXPORT extern const CertErrorId kMaxPathLengthViolated;

OPENSSL_EXPORT extern const CertErrorId kBasicConstraintsIndicatesNotCa;

OPENSSL_EXPORT extern const CertErrorId kMissingBasicConstraints;

OPENSSL_EXPORT extern const CertErrorId kNotPermittedByNameConstraints;

OPENSSL_EXPORT extern const CertErrorId kTooManyNameConstraintChecks;

OPENSSL_EXPORT extern const CertErrorId kSubjectDoesNotMatchIssuer;

OPENSSL_EXPORT extern const CertErrorId kVerifySignedDataFailed;

OPENSSL_EXPORT extern const CertErrorId kSignatureAlgorithmsDifferentEncoding;

OPENSSL_EXPORT extern const CertErrorId kEkuLacksServerAuth;

OPENSSL_EXPORT extern const CertErrorId kEkuLacksClientAuth;

OPENSSL_EXPORT extern const CertErrorId kCertIsNotTrustAnchor;

OPENSSL_EXPORT extern const CertErrorId kNoValidPolicy;

OPENSSL_EXPORT extern const CertErrorId kPolicyMappingAnyPolicy;

OPENSSL_EXPORT extern const CertErrorId kFailedParsingSpki;

OPENSSL_EXPORT extern const CertErrorId kUnacceptableSignatureAlgorithm;

OPENSSL_EXPORT extern const CertErrorId kUnacceptablePublicKey;

OPENSSL_EXPORT extern const CertErrorId kEkuLacksServerAuthButHasAnyEKU;

OPENSSL_EXPORT extern const CertErrorId kEkuLacksClientAuthButHasAnyEKU;

OPENSSL_EXPORT extern const CertErrorId kEkuLacksClientAuthOrServerAuth;

OPENSSL_EXPORT extern const CertErrorId kEkuHasProhibitedOCSPSigning;

OPENSSL_EXPORT extern const CertErrorId kEkuHasProhibitedTimeStamping;

OPENSSL_EXPORT extern const CertErrorId kEkuHasProhibitedCodeSigning;

OPENSSL_EXPORT extern const CertErrorId kEkuIncorrectForRcsMlsClient;

OPENSSL_EXPORT extern const CertErrorId kEkuNotPresent;

OPENSSL_EXPORT extern const CertErrorId kCertificateRevoked;

OPENSSL_EXPORT extern const CertErrorId kNoRevocationMechanism;

OPENSSL_EXPORT extern const CertErrorId kUnableToCheckRevocation;

OPENSSL_EXPORT extern const CertErrorId kNoIssuersFound;

OPENSSL_EXPORT extern const CertErrorId kDeadlineExceeded;

OPENSSL_EXPORT extern const CertErrorId kIterationLimitExceeded;

OPENSSL_EXPORT extern const CertErrorId kDepthLimitExceeded;

}
BSSL_NAMESPACE_END

#endif
