/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_DTLS_SRTP_TRANSPORT_H_
#define PC_DTLS_SRTP_TRANSPORT_H_

#include <functional>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/dtls_transport_interface.h"
#include "api/rtc_error.h"
#include "p2p/base/dtls_transport_internal.h"
#include "p2p/base/packet_transport_internal.h"
#include "pc/srtp_transport.h"
#include "rtc_base/buffer.h"

namespace webrtc {

class DtlsSrtpTransport : public SrtpTransport {
 public:
  DtlsSrtpTransport(bool rtcp_mux_enabled, const FieldTrialsView& field_trials);

  void SetDtlsTransports(cricket::DtlsTransportInternal* rtp_dtls_transport,
                         cricket::DtlsTransportInternal* rtcp_dtls_transport);

  void SetRtcpMuxEnabled(bool enable) override;

  void UpdateSendEncryptedHeaderExtensionIds(
      const std::vector<int>& send_extension_ids);

  void UpdateRecvEncryptedHeaderExtensionIds(
      const std::vector<int>& recv_extension_ids);

  void SetOnDtlsStateChange(std::function<void(void)> callback);

  void SetActiveResetSrtpParams(bool active_reset_srtp_params) {
    active_reset_srtp_params_ = active_reset_srtp_params;
  }

 private:
  bool IsDtlsActive();
  bool IsDtlsConnected();
  bool IsDtlsWritable();
  bool DtlsHandshakeCompleted();
  void MaybeSetupDtlsSrtp();
  void SetupRtpDtlsSrtp();
  void SetupRtcpDtlsSrtp();
  bool ExtractParams(cricket::DtlsTransportInternal* dtls_transport,
                     int* selected_crypto_suite,
                     rtc::ZeroOnFreeBuffer<unsigned char>* send_key,
                     rtc::ZeroOnFreeBuffer<unsigned char>* recv_key);
  void SetDtlsTransport(cricket::DtlsTransportInternal* new_dtls_transport,
                        cricket::DtlsTransportInternal** old_dtls_transport);
  void SetRtpDtlsTransport(cricket::DtlsTransportInternal* rtp_dtls_transport);
  void SetRtcpDtlsTransport(
      cricket::DtlsTransportInternal* rtcp_dtls_transport);

  void OnDtlsState(cricket::DtlsTransportInternal* dtls_transport,
                   DtlsTransportState state);

  void OnWritableState(rtc::PacketTransportInternal* packet_transport) override;

  cricket::DtlsTransportInternal* rtp_dtls_transport_ = nullptr;
  cricket::DtlsTransportInternal* rtcp_dtls_transport_ = nullptr;

  absl::optional<std::vector<int>> send_extension_ids_;
  absl::optional<std::vector<int>> recv_extension_ids_;

  bool active_reset_srtp_params_ = false;
  std::function<void(void)> on_dtls_state_change_;
};

}

#endif
