/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_JSEP_TRANSPORT_H_
#define PC_JSEP_TRANSPORT_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/candidate.h"
#include "api/ice_transport_interface.h"
#include "api/jsep.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/transport/data_channel_transport_interface.h"
#include "media/sctp/sctp_transport_internal.h"
#include "p2p/base/dtls_transport.h"
#include "p2p/base/dtls_transport_internal.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/transport_description.h"
#include "p2p/base/transport_info.h"
#include "pc/dtls_srtp_transport.h"
#include "pc/dtls_transport.h"
#include "pc/rtcp_mux_filter.h"
#include "pc/rtp_transport.h"
#include "pc/rtp_transport_internal.h"
#include "pc/sctp_transport.h"
#include "pc/session_description.h"
#include "pc/srtp_transport.h"
#include "pc/transport_stats.h"
#include "rtc_base/checks.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"

namespace cricket {

class DtlsTransportInternal;

struct JsepTransportDescription {
 public:
  JsepTransportDescription();
  JsepTransportDescription(
      bool rtcp_mux_enabled,
      const std::vector<int>& encrypted_header_extension_ids,
      int rtp_abs_sendtime_extn_id,
      const TransportDescription& transport_description);
  JsepTransportDescription(const JsepTransportDescription& from);
  ~JsepTransportDescription();

  JsepTransportDescription& operator=(const JsepTransportDescription& from);

  bool rtcp_mux_enabled = true;
  std::vector<int> encrypted_header_extension_ids;
  int rtp_abs_sendtime_extn_id = -1;

  TransportDescription transport_desc;
};

class JsepTransport {
 public:

  JsepTransport(
      const std::string& mid,
      const rtc::scoped_refptr<rtc::RTCCertificate>& local_certificate,
      rtc::scoped_refptr<webrtc::IceTransportInterface> ice_transport,
      rtc::scoped_refptr<webrtc::IceTransportInterface> rtcp_ice_transport,
      std::unique_ptr<webrtc::RtpTransport> unencrypted_rtp_transport,
      std::unique_ptr<webrtc::SrtpTransport> sdes_transport,
      std::unique_ptr<webrtc::DtlsSrtpTransport> dtls_srtp_transport,
      std::unique_ptr<DtlsTransportInternal> rtp_dtls_transport,
      std::unique_ptr<DtlsTransportInternal> rtcp_dtls_transport,
      std::unique_ptr<SctpTransportInternal> sctp_transport,
      std::function<void()> rtcp_mux_active_callback);

  ~JsepTransport();

  JsepTransport(const JsepTransport&) = delete;
  JsepTransport& operator=(const JsepTransport&) = delete;

  const std::string& mid() const { return mid_; }

  void SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& local_certificate) {
    RTC_DCHECK_RUN_ON(network_thread_);
    local_certificate_ = local_certificate;
  }

  rtc::scoped_refptr<rtc::RTCCertificate> GetLocalCertificate() const {
    RTC_DCHECK_RUN_ON(network_thread_);
    return local_certificate_;
  }

  webrtc::RTCError SetLocalJsepTransportDescription(
      const JsepTransportDescription& jsep_description,
      webrtc::SdpType type);

  webrtc::RTCError SetRemoteJsepTransportDescription(
      const JsepTransportDescription& jsep_description,
      webrtc::SdpType type);
  webrtc::RTCError AddRemoteCandidates(const Candidates& candidates);

  void SetNeedsIceRestartFlag();

  bool needs_ice_restart() const {
    RTC_DCHECK_RUN_ON(network_thread_);
    return needs_ice_restart_;
  }

  absl::optional<rtc::SSLRole> GetDtlsRole() const;

  bool GetStats(TransportStats* stats);

  const JsepTransportDescription* local_description() const {
    RTC_DCHECK_RUN_ON(network_thread_);
    return local_description_.get();
  }

  const JsepTransportDescription* remote_description() const {
    RTC_DCHECK_RUN_ON(network_thread_);
    return remote_description_.get();
  }

  webrtc::RtpTransportInternal* rtp_transport() const {
    if (dtls_srtp_transport_) {
      return dtls_srtp_transport_.get();
    }
    if (sdes_transport_) {
      return sdes_transport_.get();
    }
    if (unencrypted_rtp_transport_) {
      return unencrypted_rtp_transport_.get();
    }
    return nullptr;
  }

  const DtlsTransportInternal* rtp_dtls_transport() const {
    if (rtp_dtls_transport_) {
      return rtp_dtls_transport_->internal();
    }
    return nullptr;
  }

  DtlsTransportInternal* rtp_dtls_transport() {
    if (rtp_dtls_transport_) {
      return rtp_dtls_transport_->internal();
    }
    return nullptr;
  }

  const DtlsTransportInternal* rtcp_dtls_transport() const {
    RTC_DCHECK_RUN_ON(network_thread_);
    if (rtcp_dtls_transport_) {
      return rtcp_dtls_transport_->internal();
    }
    return nullptr;
  }

  DtlsTransportInternal* rtcp_dtls_transport() {
    RTC_DCHECK_RUN_ON(network_thread_);
    if (rtcp_dtls_transport_) {
      return rtcp_dtls_transport_->internal();
    }
    return nullptr;
  }

  rtc::scoped_refptr<webrtc::DtlsTransport> RtpDtlsTransport() {
    return rtp_dtls_transport_;
  }

  rtc::scoped_refptr<webrtc::SctpTransport> SctpTransport() const {
    return sctp_transport_;
  }

  webrtc::DataChannelTransportInterface* data_channel_transport() const {
    return sctp_transport_.get();
  }

  webrtc::RTCError VerifyCertificateFingerprint(
      const rtc::RTCCertificate* certificate,
      const rtc::SSLFingerprint* fingerprint) const;

  void SetActiveResetSrtpParams(bool active_reset_srtp_params);

 private:
  bool SetRtcpMux(bool enable, webrtc::SdpType type, ContentSource source);

  void ActivateRtcpMux() RTC_RUN_ON(network_thread_);

  webrtc::RTCError NegotiateAndSetDtlsParameters(
      webrtc::SdpType local_description_type);

  webrtc::RTCError NegotiateDtlsRole(
      webrtc::SdpType local_description_type,
      ConnectionRole local_connection_role,
      ConnectionRole remote_connection_role,
      absl::optional<rtc::SSLRole>* negotiated_dtls_role);

  void SetRemoteIceParameters(const IceParameters& ice_parameters,
                              IceTransportInternal* ice);

  static webrtc::RTCError SetNegotiatedDtlsParameters(
      DtlsTransportInternal* dtls_transport,
      absl::optional<rtc::SSLRole> dtls_role,
      rtc::SSLFingerprint* remote_fingerprint);

  bool GetTransportStats(DtlsTransportInternal* dtls_transport,
                         int component,
                         TransportStats* stats);

  const rtc::Thread* const network_thread_;
  const std::string mid_;

  bool needs_ice_restart_ RTC_GUARDED_BY(network_thread_) = false;
  rtc::scoped_refptr<rtc::RTCCertificate> local_certificate_
      RTC_GUARDED_BY(network_thread_);
  std::unique_ptr<JsepTransportDescription> local_description_
      RTC_GUARDED_BY(network_thread_);
  std::unique_ptr<JsepTransportDescription> remote_description_
      RTC_GUARDED_BY(network_thread_);

  const rtc::scoped_refptr<webrtc::IceTransportInterface> ice_transport_;
  const rtc::scoped_refptr<webrtc::IceTransportInterface> rtcp_ice_transport_;

  const std::unique_ptr<webrtc::RtpTransport> unencrypted_rtp_transport_;
  const std::unique_ptr<webrtc::SrtpTransport> sdes_transport_;
  const std::unique_ptr<webrtc::DtlsSrtpTransport> dtls_srtp_transport_;

  const rtc::scoped_refptr<webrtc::DtlsTransport> rtp_dtls_transport_;

  rtc::scoped_refptr<webrtc::DtlsTransport> rtcp_dtls_transport_
      RTC_GUARDED_BY(network_thread_);

  const rtc::scoped_refptr<webrtc::SctpTransport> sctp_transport_;

  RtcpMuxFilter rtcp_mux_negotiator_ RTC_GUARDED_BY(network_thread_);

  absl::optional<std::vector<int>> send_extension_ids_
      RTC_GUARDED_BY(network_thread_);
  absl::optional<std::vector<int>> recv_extension_ids_
      RTC_GUARDED_BY(network_thread_);

  std::function<void()> rtcp_mux_active_callback_;
};

}

#endif
