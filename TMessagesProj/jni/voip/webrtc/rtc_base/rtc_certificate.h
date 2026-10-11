/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_RTC_CERTIFICATE_H_
#define RTC_BASE_RTC_CERTIFICATE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

namespace rtc {

class SSLCertChain;
class SSLCertificate;
class SSLIdentity;

class RTCCertificatePEM {
 public:
  RTCCertificatePEM(absl::string_view private_key,
                    absl::string_view certificate)
      : private_key_(private_key), certificate_(certificate) {}

  const std::string& private_key() const { return private_key_; }
  const std::string& certificate() const { return certificate_; }

 private:
  std::string private_key_;
  std::string certificate_;
};

class RTC_EXPORT RTCCertificate final
    : public RefCountedNonVirtual<RTCCertificate> {
 public:

  static scoped_refptr<RTCCertificate> Create(
      std::unique_ptr<SSLIdentity> identity);

  uint64_t Expires() const;

  bool HasExpired(uint64_t now) const;

  const SSLCertificate& GetSSLCertificate() const;
  const SSLCertChain& GetSSLCertificateChain() const;

  SSLIdentity* identity() const { return identity_.get(); }

  RTCCertificatePEM ToPEM() const;

  static scoped_refptr<RTCCertificate> FromPEM(const RTCCertificatePEM& pem);
  bool operator==(const RTCCertificate& certificate) const;
  bool operator!=(const RTCCertificate& certificate) const;

 protected:
  explicit RTCCertificate(SSLIdentity* identity);

  friend class RefCountedNonVirtual<RTCCertificate>;
  ~RTCCertificate();

 private:

  const std::unique_ptr<SSLIdentity> identity_;
};

}

#endif
