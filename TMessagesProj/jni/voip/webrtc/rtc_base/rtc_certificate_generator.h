/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_RTC_CERTIFICATE_GENERATOR_H_
#define RTC_BASE_RTC_CERTIFICATE_GENERATOR_H_

#include <stdint.h>

#include "absl/functional/any_invocable.h"
#include "absl/types/optional.h"
#include "api/scoped_refptr.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread.h"

namespace rtc {

class RTCCertificateGeneratorInterface {
 public:

  using Callback = absl::AnyInvocable<void(scoped_refptr<RTCCertificate>) &&>;

  virtual ~RTCCertificateGeneratorInterface() = default;

  virtual void GenerateCertificateAsync(
      const KeyParams& key_params,
      const absl::optional<uint64_t>& expires_ms,
      Callback callback) = 0;
};

class RTC_EXPORT RTCCertificateGenerator
    : public RTCCertificateGeneratorInterface {
 public:

  static scoped_refptr<RTCCertificate> GenerateCertificate(
      const KeyParams& key_params,
      const absl::optional<uint64_t>& expires_ms);

  RTCCertificateGenerator(Thread* signaling_thread, Thread* worker_thread);
  ~RTCCertificateGenerator() override {}

  void GenerateCertificateAsync(const KeyParams& key_params,
                                const absl::optional<uint64_t>& expires_ms,
                                Callback callback) override;

 private:
  Thread* const signaling_thread_;
  Thread* const worker_thread_;
};

}

#endif
