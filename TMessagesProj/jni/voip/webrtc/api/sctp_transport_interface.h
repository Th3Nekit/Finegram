/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_SCTP_TRANSPORT_INTERFACE_H_
#define API_SCTP_TRANSPORT_INTERFACE_H_

#include "absl/types/optional.h"
#include "api/dtls_transport_interface.h"
#include "api/ref_count.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"

namespace webrtc {

enum class SctpTransportState {
  kNew,
  kConnecting,
  kConnected,
  kClosed,
  kNumValues
};

class RTC_EXPORT SctpTransportInformation {
 public:
  SctpTransportInformation() = default;
  SctpTransportInformation(const SctpTransportInformation&) = default;
  explicit SctpTransportInformation(SctpTransportState state);
  SctpTransportInformation(
      SctpTransportState state,
      rtc::scoped_refptr<DtlsTransportInterface> dtls_transport,
      absl::optional<double> max_message_size,
      absl::optional<int> max_channels);
  ~SctpTransportInformation();

  rtc::scoped_refptr<DtlsTransportInterface> dtls_transport() const {
    return dtls_transport_;
  }
  SctpTransportState state() const { return state_; }
  absl::optional<double> MaxMessageSize() const { return max_message_size_; }
  absl::optional<int> MaxChannels() const { return max_channels_; }

 private:
  SctpTransportState state_;
  rtc::scoped_refptr<DtlsTransportInterface> dtls_transport_;
  absl::optional<double> max_message_size_;
  absl::optional<int> max_channels_;
};

class SctpTransportObserverInterface {
 public:

  virtual void OnStateChange(SctpTransportInformation info) = 0;

 protected:
  virtual ~SctpTransportObserverInterface() = default;
};

class SctpTransportInterface : public webrtc::RefCountInterface {
 public:

  virtual rtc::scoped_refptr<DtlsTransportInterface> dtls_transport() const = 0;

  virtual SctpTransportInformation Information() const = 0;

  virtual void RegisterObserver(SctpTransportObserverInterface* observer) = 0;
  virtual void UnregisterObserver() = 0;
};

}

#endif
