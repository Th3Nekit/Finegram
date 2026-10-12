/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_DTLS_TRANSPORT_H_
#define P2P_BASE_DTLS_TRANSPORT_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/crypto/crypto_options.h"
#include "api/dtls_transport_interface.h"
#include "api/sequence_checker.h"
#include "p2p/base/dtls_transport_internal.h"
#include "p2p/base/ice_transport_internal.h"
#include "rtc_base/buffer.h"
#include "rtc_base/buffer_queue.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/stream.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/no_unique_address.h"

namespace rtc {
class PacketTransportInternal;
}

namespace cricket {

class StreamInterfaceChannel : public rtc::StreamInterface {
 public:
  explicit StreamInterfaceChannel(IceTransportInternal* ice_transport);

  StreamInterfaceChannel(const StreamInterfaceChannel&) = delete;
  StreamInterfaceChannel& operator=(const StreamInterfaceChannel&) = delete;

  bool OnPacketReceived(const char* data, size_t size);

  rtc::StreamState GetState() const override;
  void Close() override;
  rtc::StreamResult Read(rtc::ArrayView<uint8_t> buffer,
                         size_t& read,
                         int& error) override;
  rtc::StreamResult Write(rtc::ArrayView<const uint8_t> data,
                          size_t& written,
                          int& error) override;

 private:
  RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker sequence_checker_;
  IceTransportInternal* const ice_transport_;
  rtc::StreamState state_ RTC_GUARDED_BY(sequence_checker_);
  rtc::BufferQueue packets_ RTC_GUARDED_BY(sequence_checker_);
};

class DtlsTransport : public DtlsTransportInternal {
 public:

  DtlsTransport(
      IceTransportInternal* ice_transport,
      const webrtc::CryptoOptions& crypto_options,
      webrtc::RtcEventLog* event_log,
      rtc::SSLProtocolVersion max_version = rtc::SSL_PROTOCOL_DTLS_12);

  ~DtlsTransport() override;

  DtlsTransport(const DtlsTransport&) = delete;
  DtlsTransport& operator=(const DtlsTransport&) = delete;

  webrtc::DtlsTransportState dtls_state() const override;
  const std::string& transport_name() const override;
  int component() const override;

  bool IsDtlsActive() const override;

  bool SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) override;
  rtc::scoped_refptr<rtc::RTCCertificate> GetLocalCertificate() const override;

  bool SetRemoteFingerprint(absl::string_view digest_alg,
                            const uint8_t* digest,
                            size_t digest_len) override;

  webrtc::RTCError SetRemoteParameters(
      absl::string_view digest_alg,
      const uint8_t* digest,
      size_t digest_len,
      absl::optional<rtc::SSLRole> role) override;

  int SendPacket(const char* data,
                 size_t size,
                 const rtc::PacketOptions& options,
                 int flags) override;

  bool GetOption(rtc::Socket::Option opt, int* value) override;

  bool GetSslVersionBytes(int* version) const override;

  bool GetSrtpCryptoSuite(int* cipher) override;

  uint16_t GetSslPeerSignatureAlgorithm() const override;

  bool GetDtlsRole(rtc::SSLRole* role) const override;
  bool SetDtlsRole(rtc::SSLRole role) override;

  bool GetSslCipherSuite(int* cipher) override;

  std::unique_ptr<rtc::SSLCertChain> GetRemoteSSLCertChain() const override;

  bool ExportKeyingMaterial(absl::string_view label,
                            const uint8_t* context,
                            size_t context_len,
                            bool use_context,
                            uint8_t* result,
                            size_t result_len) override;

  IceTransportInternal* ice_transport() override;

  bool IsDtlsConnected();

  bool receiving() const override;
  bool writable() const override;

  int GetError() override;

  absl::optional<rtc::NetworkRoute> network_route() const override;

  int SetOption(rtc::Socket::Option opt, int value) override;

  std::string ToString() const {
    const absl::string_view RECEIVING_ABBREV[2] = {"_", "R"};
    const absl::string_view WRITABLE_ABBREV[2] = {"_", "W"};
    rtc::StringBuilder sb;
    sb << "DtlsTransport[" << transport_name() << "|" << component_ << "|"
       << RECEIVING_ABBREV[receiving()] << WRITABLE_ABBREV[writable()] << "]";
    return sb.Release();
  }

 private:
  void ConnectToIceTransport();

  void OnWritableState(rtc::PacketTransportInternal* transport);
  void OnReadPacket(rtc::PacketTransportInternal* transport,
                    const char* data,
                    size_t size,
                    const int64_t& packet_time_us,
                    int flags);
  void OnSentPacket(rtc::PacketTransportInternal* transport,
                    const rtc::SentPacket& sent_packet);
  void OnReadyToSend(rtc::PacketTransportInternal* transport);
  void OnReceivingState(rtc::PacketTransportInternal* transport);
  void OnDtlsEvent(rtc::StreamInterface* stream_, int sig, int err);
  void OnNetworkRouteChanged(absl::optional<rtc::NetworkRoute> network_route);
  bool SetupDtls();
  void MaybeStartDtls();
  bool HandleDtlsPacket(const char* data, size_t size);
  void OnDtlsHandshakeError(rtc::SSLHandshakeError error);
  void ConfigureHandshakeTimeout();

  void set_receiving(bool receiving);
  void set_writable(bool writable);

  void set_dtls_state(webrtc::DtlsTransportState state);

  webrtc::SequenceChecker thread_checker_;

  const int component_;
  webrtc::DtlsTransportState dtls_state_ = webrtc::DtlsTransportState::kNew;

  IceTransportInternal* const ice_transport_;
  std::unique_ptr<rtc::SSLStreamAdapter> dtls_;
  StreamInterfaceChannel*
      downward_;
  const std::vector<int> srtp_ciphers_;
  bool dtls_active_ = false;
  rtc::scoped_refptr<rtc::RTCCertificate> local_certificate_;
  absl::optional<rtc::SSLRole> dtls_role_;
  const rtc::SSLProtocolVersion ssl_max_version_;
  rtc::Buffer remote_fingerprint_value_;
  std::string remote_fingerprint_algorithm_;

  rtc::Buffer cached_client_hello_;

  bool receiving_ = false;
  bool writable_ = false;

  webrtc::RtcEventLog* const event_log_;
};

}

#endif
