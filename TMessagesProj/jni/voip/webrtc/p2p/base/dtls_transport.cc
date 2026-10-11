/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/dtls_transport.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/dtls_transport_interface.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/events/rtc_event_dtls_transport_state.h"
#include "logging/rtc_event_log/events/rtc_event_dtls_writable_state.h"
#include "p2p/base/packet_transport_internal.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/dscp.h"
#include "rtc_base/logging.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/stream.h"
#include "rtc_base/thread.h"

namespace cricket {

static const size_t kDtlsRecordHeaderLen = 13;
static const size_t kMaxDtlsPacketLen = 2048;
static const size_t kMinRtpPacketLen = 12;

static const size_t kMaxPendingPackets = 2;

static const int kMinHandshakeTimeout = 50;
static const int kMaxHandshakeTimeout = 3000;

static bool IsDtlsPacket(const char* data, size_t len) {
  const uint8_t* u = reinterpret_cast<const uint8_t*>(data);
  return (len >= kDtlsRecordHeaderLen && (u[0] > 19 && u[0] < 64));
}
static bool IsDtlsClientHelloPacket(const char* data, size_t len) {
  if (!IsDtlsPacket(data, len)) {
    return false;
  }
  const uint8_t* u = reinterpret_cast<const uint8_t*>(data);
  return len > 17 && u[0] == 22 && u[13] == 1;
}
static bool IsRtpPacket(const char* data, size_t len) {
  const uint8_t* u = reinterpret_cast<const uint8_t*>(data);
  return (len >= kMinRtpPacketLen && (u[0] & 0xC0) == 0x80);
}

StreamInterfaceChannel::StreamInterfaceChannel(
    IceTransportInternal* ice_transport)
    : ice_transport_(ice_transport),
      state_(rtc::SS_OPEN),
      packets_(kMaxPendingPackets, kMaxDtlsPacketLen) {}

rtc::StreamResult StreamInterfaceChannel::Read(rtc::ArrayView<uint8_t> buffer,
                                               size_t& read,
                                               int& error) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (state_ == rtc::SS_CLOSED)
    return rtc::SR_EOS;
  if (state_ == rtc::SS_OPENING)
    return rtc::SR_BLOCK;

  if (!packets_.ReadFront(buffer.data(), buffer.size(), &read)) {
    return rtc::SR_BLOCK;
  }

  return rtc::SR_SUCCESS;
}

rtc::StreamResult StreamInterfaceChannel::Write(
    rtc::ArrayView<const uint8_t> data,
    size_t& written,
    int& error) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  rtc::PacketOptions packet_options;
  ice_transport_->SendPacket(reinterpret_cast<const char*>(data.data()),
                             data.size(), packet_options);
  written = data.size();
  return rtc::SR_SUCCESS;
}

bool StreamInterfaceChannel::OnPacketReceived(const char* data, size_t size) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (packets_.size() > 0) {
    RTC_LOG(LS_WARNING) << "Packet already in queue.";
  }
  bool ret = packets_.WriteBack(data, size, NULL);
  if (!ret) {

    RTC_LOG(LS_ERROR) << "Failed to write packet to queue.";
  }
  SignalEvent(this, rtc::SE_READ, 0);
  return ret;
}

rtc::StreamState StreamInterfaceChannel::GetState() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return state_;
}

void StreamInterfaceChannel::Close() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  packets_.Clear();
  state_ = rtc::SS_CLOSED;
}

DtlsTransport::DtlsTransport(IceTransportInternal* ice_transport,
                             const webrtc::CryptoOptions& crypto_options,
                             webrtc::RtcEventLog* event_log,
                             rtc::SSLProtocolVersion max_version)
    : component_(ice_transport->component()),
      ice_transport_(ice_transport),
      downward_(NULL),
      srtp_ciphers_(crypto_options.GetSupportedDtlsSrtpCryptoSuites()),
      ssl_max_version_(max_version),
      event_log_(event_log) {
  RTC_DCHECK(ice_transport_);
  ConnectToIceTransport();
}

DtlsTransport::~DtlsTransport() = default;

webrtc::DtlsTransportState DtlsTransport::dtls_state() const {
  return dtls_state_;
}

const std::string& DtlsTransport::transport_name() const {
  return ice_transport_->transport_name();
}

int DtlsTransport::component() const {
  return component_;
}

bool DtlsTransport::IsDtlsActive() const {
  return dtls_active_;
}

bool DtlsTransport::SetLocalCertificate(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  if (dtls_active_) {
    if (certificate == local_certificate_) {

      RTC_LOG(LS_INFO) << ToString() << ": Ignoring identical DTLS identity";
      return true;
    } else {
      RTC_LOG(LS_ERROR) << ToString()
                        << ": Can't change DTLS local identity in this state";
      return false;
    }
  }

  if (certificate) {
    local_certificate_ = certificate;
    dtls_active_ = true;
  } else {
    RTC_LOG(LS_INFO) << ToString()
                     << ": NULL DTLS identity supplied. Not doing DTLS";
  }

  return true;
}

rtc::scoped_refptr<rtc::RTCCertificate> DtlsTransport::GetLocalCertificate()
    const {
  return local_certificate_;
}

bool DtlsTransport::SetDtlsRole(rtc::SSLRole role) {
  if (dtls_) {
    RTC_DCHECK(dtls_role_);
    if (*dtls_role_ != role) {
      RTC_LOG(LS_ERROR)
          << "SSL Role can't be reversed after the session is setup.";
      return false;
    }
    return true;
  }

  dtls_role_ = role;
  return true;
}

bool DtlsTransport::GetDtlsRole(rtc::SSLRole* role) const {
  if (!dtls_role_) {
    return false;
  }
  *role = *dtls_role_;
  return true;
}

bool DtlsTransport::GetSslCipherSuite(int* cipher) {
  if (dtls_state() != webrtc::DtlsTransportState::kConnected) {
    return false;
  }

  return dtls_->GetSslCipherSuite(cipher);
}

webrtc::RTCError DtlsTransport::SetRemoteParameters(
    absl::string_view digest_alg,
    const uint8_t* digest,
    size_t digest_len,
    absl::optional<rtc::SSLRole> role) {
  rtc::Buffer remote_fingerprint_value(digest, digest_len);
  bool is_dtls_restart =
      dtls_active_ && remote_fingerprint_value_ != remote_fingerprint_value;

  if (role) {
    if (is_dtls_restart) {
      dtls_role_ = *role;
    } else {
      if (!SetDtlsRole(*role)) {
        return webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER,
                                "Failed to set SSL role for the transport.");
      }
    }
  }

  if (!SetRemoteFingerprint(digest_alg, digest, digest_len)) {
    return webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER,
                            "Failed to apply remote fingerprint.");
  }
  return webrtc::RTCError::OK();
}

bool DtlsTransport::SetRemoteFingerprint(absl::string_view digest_alg,
                                         const uint8_t* digest,
                                         size_t digest_len) {
  rtc::Buffer remote_fingerprint_value(digest, digest_len);

  if (dtls_active_ && remote_fingerprint_value_ == remote_fingerprint_value &&
      !digest_alg.empty()) {

    RTC_LOG(LS_INFO) << ToString()
                     << ": Ignoring identical remote DTLS fingerprint";
    return true;
  }

  if (digest_alg.empty()) {
    RTC_DCHECK(!digest_len);
    RTC_LOG(LS_INFO) << ToString() << ": Other side didn't support DTLS.";
    dtls_active_ = false;
    return true;
  }

  if (!dtls_active_) {
    RTC_LOG(LS_ERROR) << ToString()
                      << ": Can't set DTLS remote settings in this state.";
    return false;
  }

  bool fingerprint_changing = remote_fingerprint_value_.size() > 0u;
  remote_fingerprint_value_ = std::move(remote_fingerprint_value);
  remote_fingerprint_algorithm_ = std::string(digest_alg);

  if (dtls_ && !fingerprint_changing) {

    rtc::SSLPeerCertificateDigestError err;
    if (!dtls_->SetPeerCertificateDigest(
            remote_fingerprint_algorithm_,
            reinterpret_cast<unsigned char*>(remote_fingerprint_value_.data()),
            remote_fingerprint_value_.size(), &err)) {
      RTC_LOG(LS_ERROR) << ToString()
                        << ": Couldn't set DTLS certificate digest.";
      set_dtls_state(webrtc::DtlsTransportState::kFailed);

      return err == rtc::SSLPeerCertificateDigestError::VERIFICATION_FAILED;
    }
    return true;
  }

  if (dtls_ && fingerprint_changing) {
    dtls_.reset(nullptr);
    set_dtls_state(webrtc::DtlsTransportState::kNew);
    set_writable(false);
  }

  if (!SetupDtls()) {
    set_dtls_state(webrtc::DtlsTransportState::kFailed);
    return false;
  }

  return true;
}

std::unique_ptr<rtc::SSLCertChain> DtlsTransport::GetRemoteSSLCertChain()
    const {
  if (!dtls_) {
    return nullptr;
  }

  return dtls_->GetPeerSSLCertChain();
}

bool DtlsTransport::ExportKeyingMaterial(absl::string_view label,
                                         const uint8_t* context,
                                         size_t context_len,
                                         bool use_context,
                                         uint8_t* result,
                                         size_t result_len) {
  return (dtls_.get())
             ? dtls_->ExportKeyingMaterial(label, context, context_len,
                                           use_context, result, result_len)
             : false;
}

bool DtlsTransport::SetupDtls() {
  RTC_DCHECK(dtls_role_);
  {
    auto downward = std::make_unique<StreamInterfaceChannel>(ice_transport_);
    StreamInterfaceChannel* downward_ptr = downward.get();

    dtls_ = rtc::SSLStreamAdapter::Create(
        std::move(downward),
        [this](rtc::SSLHandshakeError error) { OnDtlsHandshakeError(error); });
    if (!dtls_) {
      RTC_LOG(LS_ERROR) << ToString() << ": Failed to create DTLS adapter.";
      return false;
    }
    downward_ = downward_ptr;
  }

  dtls_->SetIdentity(local_certificate_->identity()->Clone());
  dtls_->SetMode(rtc::SSL_MODE_DTLS);
  dtls_->SetMaxProtocolVersion(ssl_max_version_);
  dtls_->SetServerRole(*dtls_role_);
  dtls_->SignalEvent.connect(this, &DtlsTransport::OnDtlsEvent);
  if (remote_fingerprint_value_.size() &&
      !dtls_->SetPeerCertificateDigest(
          remote_fingerprint_algorithm_,
          reinterpret_cast<unsigned char*>(remote_fingerprint_value_.data()),
          remote_fingerprint_value_.size())) {
    RTC_LOG(LS_ERROR) << ToString()
                      << ": Couldn't set DTLS certificate digest.";
    return false;
  }

  if (!srtp_ciphers_.empty()) {
    if (!dtls_->SetDtlsSrtpCryptoSuites(srtp_ciphers_)) {
      RTC_LOG(LS_ERROR) << ToString() << ": Couldn't set DTLS-SRTP ciphers.";
      return false;
    }
  } else {
    RTC_LOG(LS_INFO) << ToString() << ": Not using DTLS-SRTP.";
  }

  RTC_LOG(LS_INFO) << ToString() << ": DTLS setup complete.";

  MaybeStartDtls();
  return true;
}

bool DtlsTransport::GetSrtpCryptoSuite(int* cipher) {
  if (dtls_state() != webrtc::DtlsTransportState::kConnected) {
    return false;
  }

  return dtls_->GetDtlsSrtpCryptoSuite(cipher);
}

bool DtlsTransport::GetSslVersionBytes(int* version) const {
  if (dtls_state() != webrtc::DtlsTransportState::kConnected) {
    return false;
  }

  return dtls_->GetSslVersionBytes(version);
}

uint16_t DtlsTransport::GetSslPeerSignatureAlgorithm() const {
  if (dtls_state() != webrtc::DtlsTransportState::kConnected) {
    return rtc::kSslSignatureAlgorithmUnknown;
  }
  return dtls_->GetPeerSignatureAlgorithm();
}

int DtlsTransport::SendPacket(const char* data,
                              size_t size,
                              const rtc::PacketOptions& options,
                              int flags) {
  if (!dtls_active_) {

    return ice_transport_->SendPacket(data, size, options);
  }

  switch (dtls_state()) {
    case webrtc::DtlsTransportState::kNew:

      return -1;
    case webrtc::DtlsTransportState::kConnecting:

      return -1;
    case webrtc::DtlsTransportState::kConnected:
      if (flags & PF_SRTP_BYPASS) {
        RTC_DCHECK(!srtp_ciphers_.empty());
        if (!IsRtpPacket(data, size)) {
          return -1;
        }

        return ice_transport_->SendPacket(data, size, options);
      } else {
        size_t written;
        int error;
        return (dtls_->WriteAll(
                    rtc::MakeArrayView(reinterpret_cast<const uint8_t*>(data),
                                       size),
                    written, error) == rtc::SR_SUCCESS)
                   ? static_cast<int>(size)
                   : -1;
      }
    case webrtc::DtlsTransportState::kFailed:

      RTC_LOG(LS_ERROR) << ToString()
                        << ": Couldn't send packet due to "
                           "webrtc::DtlsTransportState::kFailed.";
      return -1;
    case webrtc::DtlsTransportState::kClosed:

      RTC_LOG(LS_ERROR) << ToString()
                        << ": Couldn't send packet due to "
                           "webrtc::DtlsTransportState::kClosed.";
      return -1;
    default:
      RTC_DCHECK_NOTREACHED();
      return -1;
  }
}

IceTransportInternal* DtlsTransport::ice_transport() {
  return ice_transport_;
}

bool DtlsTransport::IsDtlsConnected() {
  return dtls_ && dtls_->IsTlsConnected();
}

bool DtlsTransport::receiving() const {
  return receiving_;
}

bool DtlsTransport::writable() const {
  return writable_;
}

int DtlsTransport::GetError() {
  return ice_transport_->GetError();
}

absl::optional<rtc::NetworkRoute> DtlsTransport::network_route() const {
  return ice_transport_->network_route();
}

bool DtlsTransport::GetOption(rtc::Socket::Option opt, int* value) {
  return ice_transport_->GetOption(opt, value);
}

int DtlsTransport::SetOption(rtc::Socket::Option opt, int value) {
  return ice_transport_->SetOption(opt, value);
}

void DtlsTransport::ConnectToIceTransport() {
  RTC_DCHECK(ice_transport_);
  ice_transport_->SignalWritableState.connect(this,
                                              &DtlsTransport::OnWritableState);
  ice_transport_->SignalReadPacket.connect(this, &DtlsTransport::OnReadPacket);
  ice_transport_->SignalSentPacket.connect(this, &DtlsTransport::OnSentPacket);
  ice_transport_->SignalReadyToSend.connect(this,
                                            &DtlsTransport::OnReadyToSend);
  ice_transport_->SignalReceivingState.connect(
      this, &DtlsTransport::OnReceivingState);
  ice_transport_->SignalNetworkRouteChanged.connect(
      this, &DtlsTransport::OnNetworkRouteChanged);
}

void DtlsTransport::OnWritableState(rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(transport == ice_transport_);
  RTC_LOG(LS_VERBOSE) << ToString()
                      << ": ice_transport writable state changed to "
                      << ice_transport_->writable();

  if (!dtls_active_) {

    set_writable(ice_transport_->writable());
    return;
  }

  switch (dtls_state()) {
    case webrtc::DtlsTransportState::kNew:
      MaybeStartDtls();
      break;
    case webrtc::DtlsTransportState::kConnected:

      set_writable(ice_transport_->writable());
      break;
    case webrtc::DtlsTransportState::kConnecting:

      break;
    case webrtc::DtlsTransportState::kFailed:

      RTC_LOG(LS_ERROR) << ToString()
                        << ": OnWritableState() called in state "
                           "webrtc::DtlsTransportState::kFailed.";
      break;
    case webrtc::DtlsTransportState::kClosed:

      RTC_LOG(LS_ERROR) << ToString()
                        << ": OnWritableState() called in state "
                           "webrtc::DtlsTransportState::kClosed.";
      break;
    case webrtc::DtlsTransportState::kNumValues:
      RTC_DCHECK_NOTREACHED();
      break;
  }
}

void DtlsTransport::OnReceivingState(rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(transport == ice_transport_);
  RTC_LOG(LS_VERBOSE) << ToString()
                      << ": ice_transport "
                         "receiving state changed to "
                      << ice_transport_->receiving();
  if (!dtls_active_ || dtls_state() == webrtc::DtlsTransportState::kConnected) {

    set_receiving(ice_transport_->receiving());
  }
}

void DtlsTransport::OnReadPacket(rtc::PacketTransportInternal* transport,
                                 const char* data,
                                 size_t size,
                                 const int64_t& packet_time_us,
                                 int flags) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(transport == ice_transport_);
  RTC_DCHECK(flags == 0);

  if (!dtls_active_) {

    SignalReadPacket(this, data, size, packet_time_us, 0);
    return;
  }

  switch (dtls_state()) {
    case webrtc::DtlsTransportState::kNew:
      if (dtls_) {
        RTC_LOG(LS_INFO) << ToString()
                         << ": Packet received before DTLS started.";
      } else {
        RTC_LOG(LS_WARNING) << ToString()
                            << ": Packet received before we know if we are "
                               "doing DTLS or not.";
      }

      if (IsDtlsClientHelloPacket(data, size)) {
        RTC_LOG(LS_INFO) << ToString()
                         << ": Caching DTLS ClientHello packet until DTLS is "
                            "started.";
        cached_client_hello_.SetData(data, size);

        if (!dtls_ && local_certificate_) {
          SetDtlsRole(rtc::SSL_SERVER);
          SetupDtls();
        }
      } else {
        RTC_LOG(LS_INFO) << ToString()
                         << ": Not a DTLS ClientHello packet; dropping.";
      }
      break;

    case webrtc::DtlsTransportState::kConnecting:
    case webrtc::DtlsTransportState::kConnected:

      if (IsDtlsPacket(data, size)) {
        if (!HandleDtlsPacket(data, size)) {
          RTC_LOG(LS_ERROR) << ToString() << ": Failed to handle DTLS packet.";
          return;
        }
      } else {

        if (dtls_state() != webrtc::DtlsTransportState::kConnected) {
          RTC_LOG(LS_ERROR) << ToString()
                            << ": Received non-DTLS packet before DTLS "
                               "complete.";
          return;
        }

        if (!IsRtpPacket(data, size)) {
          RTC_LOG(LS_ERROR)
              << ToString() << ": Received unexpected non-DTLS packet.";
          return;
        }

        RTC_DCHECK(!srtp_ciphers_.empty());

        SignalReadPacket(this, data, size, packet_time_us, PF_SRTP_BYPASS);
      }
      break;
    case webrtc::DtlsTransportState::kFailed:
    case webrtc::DtlsTransportState::kClosed:
    case webrtc::DtlsTransportState::kNumValues:

      break;
  }
}

void DtlsTransport::OnSentPacket(rtc::PacketTransportInternal* transport,
                                 const rtc::SentPacket& sent_packet) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  SignalSentPacket(this, sent_packet);
}

void DtlsTransport::OnReadyToSend(rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (writable()) {
    SignalReadyToSend(this);
  }
}

void DtlsTransport::OnDtlsEvent(rtc::StreamInterface* dtls, int sig, int err) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(dtls == dtls_.get());
  if (sig & rtc::SE_OPEN) {

    RTC_LOG(LS_INFO) << ToString() << ": DTLS handshake complete.";
    if (dtls_->GetState() == rtc::SS_OPEN) {

      set_dtls_state(webrtc::DtlsTransportState::kConnected);
      set_writable(true);
    }
  }
  if (sig & rtc::SE_READ) {
    uint8_t buf[kMaxDtlsPacketLen];
    size_t read;
    int read_error;
    rtc::StreamResult ret;

    do {
      ret = dtls_->Read(buf, read, read_error);
      if (ret == rtc::SR_SUCCESS) {
        SignalReadPacket(this, reinterpret_cast<const char*>(buf), read,
                         rtc::TimeMicros(), 0);
      } else if (ret == rtc::SR_EOS) {

        RTC_LOG(LS_INFO) << ToString() << ": DTLS transport closed by remote";
        set_writable(false);
        set_dtls_state(webrtc::DtlsTransportState::kClosed);
        SignalClosed(this);
      } else if (ret == rtc::SR_ERROR) {

        RTC_LOG(LS_INFO)
            << ToString()
            << ": Closed by remote with DTLS transport error, code="
            << read_error;
        set_writable(false);
        set_dtls_state(webrtc::DtlsTransportState::kFailed);
        SignalClosed(this);
      }
    } while (ret == rtc::SR_SUCCESS);
  }
  if (sig & rtc::SE_CLOSE) {
    RTC_DCHECK(sig == rtc::SE_CLOSE);
    set_writable(false);
    if (!err) {
      RTC_LOG(LS_INFO) << ToString() << ": DTLS transport closed";
      set_dtls_state(webrtc::DtlsTransportState::kClosed);
    } else {
      RTC_LOG(LS_INFO) << ToString() << ": DTLS transport error, code=" << err;
      set_dtls_state(webrtc::DtlsTransportState::kFailed);
    }
  }
}

void DtlsTransport::OnNetworkRouteChanged(
    absl::optional<rtc::NetworkRoute> network_route) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  SignalNetworkRouteChanged(network_route);
}

void DtlsTransport::MaybeStartDtls() {
  if (dtls_ && ice_transport_->writable()) {
    ConfigureHandshakeTimeout();

    if (dtls_->StartSSL()) {

      RTC_DCHECK_NOTREACHED() << "StartSSL failed.";
      RTC_LOG(LS_ERROR) << ToString() << ": Couldn't start DTLS handshake";
      set_dtls_state(webrtc::DtlsTransportState::kFailed);
      return;
    }
    RTC_LOG(LS_INFO) << ToString()
                     << ": DtlsTransport: Started DTLS handshake active="
                     << IsDtlsActive();
    set_dtls_state(webrtc::DtlsTransportState::kConnecting);

    if (cached_client_hello_.size()) {
      if (*dtls_role_ == rtc::SSL_SERVER) {
        RTC_LOG(LS_INFO) << ToString()
                         << ": Handling cached DTLS ClientHello packet.";
        if (!HandleDtlsPacket(cached_client_hello_.data<char>(),
                              cached_client_hello_.size())) {
          RTC_LOG(LS_ERROR) << ToString() << ": Failed to handle DTLS packet.";
        }
      } else {
        RTC_LOG(LS_WARNING) << ToString()
                            << ": Discarding cached DTLS ClientHello packet "
                               "because we don't have the server role.";
      }
      cached_client_hello_.Clear();
    }
  }
}

bool DtlsTransport::HandleDtlsPacket(const char* data, size_t size) {

  const uint8_t* tmp_data = reinterpret_cast<const uint8_t*>(data);
  size_t tmp_size = size;
  while (tmp_size > 0) {
    if (tmp_size < kDtlsRecordHeaderLen)
      return false;

    size_t record_len = (tmp_data[11] << 8) | (tmp_data[12]);
    if ((record_len + kDtlsRecordHeaderLen) > tmp_size)
      return false;

    tmp_data += record_len + kDtlsRecordHeaderLen;
    tmp_size -= record_len + kDtlsRecordHeaderLen;
  }

  return downward_->OnPacketReceived(data, size);
}

void DtlsTransport::set_receiving(bool receiving) {
  if (receiving_ == receiving) {
    return;
  }
  receiving_ = receiving;
  SignalReceivingState(this);
}

void DtlsTransport::set_writable(bool writable) {
  if (writable_ == writable) {
    return;
  }
  if (event_log_) {
    event_log_->Log(
        std::make_unique<webrtc::RtcEventDtlsWritableState>(writable));
  }
  RTC_LOG(LS_VERBOSE) << ToString() << ": set_writable to: " << writable;
  writable_ = writable;
  if (writable_) {
    SignalReadyToSend(this);
  }
  SignalWritableState(this);
}

void DtlsTransport::set_dtls_state(webrtc::DtlsTransportState state) {
  if (dtls_state_ == state) {
    return;
  }
  if (event_log_) {
    event_log_->Log(
        std::make_unique<webrtc::RtcEventDtlsTransportState>(state));
  }
  RTC_LOG(LS_VERBOSE) << ToString() << ": set_dtls_state from:"
                      << static_cast<int>(dtls_state_) << " to "
                      << static_cast<int>(state);
  dtls_state_ = state;
  SendDtlsState(this, state);
}

void DtlsTransport::OnDtlsHandshakeError(rtc::SSLHandshakeError error) {
  SendDtlsHandshakeError(error);
}

void DtlsTransport::ConfigureHandshakeTimeout() {
  RTC_DCHECK(dtls_);
  absl::optional<int> rtt = ice_transport_->GetRttEstimate();
  if (rtt) {

    int initial_timeout = std::max(kMinHandshakeTimeout,
                                   std::min(kMaxHandshakeTimeout, 2 * (*rtt)));
    RTC_LOG(LS_INFO) << ToString() << ": configuring DTLS handshake timeout "
                     << initial_timeout << " based on ICE RTT " << *rtt;

    dtls_->SetInitialRetransmissionTimeout(initial_timeout);
  } else {
    RTC_LOG(LS_INFO)
        << ToString()
        << ": no RTT estimate - using default DTLS handshake timeout";
  }
}

}
