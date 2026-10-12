/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SRTP_TRANSPORT_H_
#define PC_SRTP_TRANSPORT_H_

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/field_trials_view.h"
#include "api/rtc_error.h"
#include "p2p/base/packet_transport_internal.h"
#include "pc/rtp_transport.h"
#include "pc/srtp_session.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/buffer.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/network_route.h"

namespace webrtc {

class SrtpTransport : public RtpTransport {
 public:
  SrtpTransport(bool rtcp_mux_enabled, const FieldTrialsView& field_trials);

  virtual ~SrtpTransport() = default;

  bool SendRtpPacket(rtc::CopyOnWriteBuffer* packet,
                     const rtc::PacketOptions& options,
                     int flags) override;

  bool SendRtcpPacket(rtc::CopyOnWriteBuffer* packet,
                      const rtc::PacketOptions& options,
                      int flags) override;

  bool IsSrtpActive() const override;

  bool IsWritable(bool rtcp) const override;

  bool SetRtpParams(int send_crypto_suite,
                    const uint8_t* send_key,
                    int send_key_len,
                    const std::vector<int>& send_extension_ids,
                    int recv_crypto_suite,
                    const uint8_t* recv_key,
                    int recv_key_len,
                    const std::vector<int>& recv_extension_ids);

  bool SetRtcpParams(int send_crypto_suite,
                     const uint8_t* send_key,
                     int send_key_len,
                     const std::vector<int>& send_extension_ids,
                     int recv_crypto_suite,
                     const uint8_t* recv_key,
                     int recv_key_len,
                     const std::vector<int>& recv_extension_ids);

  void ResetParams();

  void EnableExternalAuth();
  bool IsExternalAuthEnabled() const;

  bool IsExternalAuthActive() const;

  bool GetSrtpOverhead(int* srtp_overhead) const;

  bool GetRtpAuthParams(uint8_t** key, int* key_len, int* tag_len);

  void CacheRtpAbsSendTimeHeaderExtension(int rtp_abs_sendtime_extn_id) {
    rtp_abs_sendtime_extn_id_ = rtp_abs_sendtime_extn_id;
  }

  bool UnregisterRtpDemuxerSink(RtpPacketSinkInterface* sink) override;

 protected:

  void MaybeUpdateWritableState();

 private:
  void ConnectToRtpTransport();
  void CreateSrtpSessions();

  void OnRtpPacketReceived(rtc::CopyOnWriteBuffer packet,
                           int64_t packet_time_us) override;
  void OnRtcpPacketReceived(rtc::CopyOnWriteBuffer packet,
                            int64_t packet_time_us) override;
  void OnNetworkRouteChanged(
      absl::optional<rtc::NetworkRoute> network_route) override;

  void OnWritableState(rtc::PacketTransportInternal* packet_transport) override;

  bool ProtectRtp(void* data, int in_len, int max_len, int* out_len);

  bool ProtectRtp(void* data,
                  int in_len,
                  int max_len,
                  int* out_len,
                  int64_t* index);
  bool ProtectRtcp(void* data, int in_len, int max_len, int* out_len);

  bool UnprotectRtp(void* data, int in_len, int* out_len);

  bool UnprotectRtcp(void* data, int in_len, int* out_len);

  bool MaybeSetKeyParams();
  bool ParseKeyParams(const std::string& key_params, uint8_t* key, size_t len);

  const std::string content_name_;

  std::unique_ptr<cricket::SrtpSession> send_session_;
  std::unique_ptr<cricket::SrtpSession> recv_session_;
  std::unique_ptr<cricket::SrtpSession> send_rtcp_session_;
  std::unique_ptr<cricket::SrtpSession> recv_rtcp_session_;

  absl::optional<int> send_crypto_suite_;
  absl::optional<int> recv_crypto_suite_;
  rtc::ZeroOnFreeBuffer<uint8_t> send_key_;
  rtc::ZeroOnFreeBuffer<uint8_t> recv_key_;

  bool writable_ = false;

  bool external_auth_enabled_ = false;

  int rtp_abs_sendtime_extn_id_ = -1;

  int decryption_failure_count_ = 0;

  const FieldTrialsView& field_trials_;
};

}

#endif
