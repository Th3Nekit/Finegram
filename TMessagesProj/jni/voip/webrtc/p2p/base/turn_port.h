/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_TURN_PORT_H_
#define P2P_BASE_TURN_PORT_H_

#include <stdio.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/async_dns_resolver.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "p2p/base/port.h"
#include "p2p/base/port_allocator.h"
#include "p2p/client/relay_port_factory_interface.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/ssl_certificate.h"

namespace webrtc {
class TurnCustomizer;
}

namespace cricket {

const int kMaxTurnUsernameLength = 509;

extern const int STUN_ATTR_TURN_LOGGING_ID;
extern const char TURN_PORT_TYPE[];
class TurnAllocateRequest;
class TurnEntry;

class TurnPort : public Port {
 public:
  enum PortState {
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_READY,
    STATE_RECEIVEONLY,
    STATE_DISCONNECTED,

  };

  static bool Validate(const CreateRelayPortArgs& args) {

    if (args.config->credentials.username.size() > kMaxTurnUsernameLength) {
      RTC_LOG(LS_ERROR) << "Attempt to use TURN with a too long username "
                        << "of length "
                        << args.config->credentials.username.size();
      return false;
    }

    if (!AllowedTurnPort(args.server_address->address.port(),
                         args.field_trials)) {
      RTC_LOG(LS_ERROR) << "Attempt to use TURN to connect to port "
                        << args.server_address->address.port();
      return false;
    }
    return true;
  }

  static std::unique_ptr<TurnPort> Create(const CreateRelayPortArgs& args,
                                          rtc::AsyncPacketSocket* socket) {
    if (!Validate(args)) {
      return nullptr;
    }

    return absl::WrapUnique(
        new TurnPort(args.network_thread, args.socket_factory, args.network,
                     socket, args.username, args.password, *args.server_address,
                     args.config->credentials, args.relative_priority,
                     args.config->tls_alpn_protocols,
                     args.config->tls_elliptic_curves, args.turn_customizer,
                     args.config->tls_cert_verifier, args.field_trials));
  }

  static std::unique_ptr<TurnPort> Create(const CreateRelayPortArgs& args,
                                          int min_port,
                                          int max_port) {
    if (!Validate(args)) {
      return nullptr;
    }

    return absl::WrapUnique(
        new TurnPort(args.network_thread, args.socket_factory, args.network,
                     min_port, max_port, args.username, args.password,
                     *args.server_address, args.config->credentials,
                     args.relative_priority, args.config->tls_alpn_protocols,
                     args.config->tls_elliptic_curves, args.turn_customizer,
                     args.config->tls_cert_verifier, args.field_trials));
  }

  ~TurnPort() override;

  const ProtocolAddress& server_address() const { return server_address_; }

  rtc::SocketAddress GetLocalAddress() const;

  bool ready() const { return state_ == STATE_READY; }
  bool connected() const {
    return state_ == STATE_READY || state_ == STATE_CONNECTED;
  }
  const RelayCredentials& credentials() const { return credentials_; }

  ProtocolType GetProtocol() const override;

  virtual TlsCertPolicy GetTlsCertPolicy() const;
  virtual void SetTlsCertPolicy(TlsCertPolicy tls_cert_policy);

  void SetTurnLoggingId(absl::string_view turn_logging_id);

  virtual std::vector<std::string> GetTlsAlpnProtocols() const;
  virtual std::vector<std::string> GetTlsEllipticCurves() const;

  void Release();

  void PrepareAddress() override;
  Connection* CreateConnection(const Candidate& c,
                               PortInterface::CandidateOrigin origin) override;
  int SendTo(const void* data,
             size_t size,
             const rtc::SocketAddress& addr,
             const rtc::PacketOptions& options,
             bool payload) override;
  int SetOption(rtc::Socket::Option opt, int value) override;
  int GetOption(rtc::Socket::Option opt, int* value) override;
  int GetError() override;

  bool HandleIncomingPacket(rtc::AsyncPacketSocket* socket,
                            const rtc::ReceivedPacket& packet) override;
  bool CanHandleIncomingPacketsFrom(
      const rtc::SocketAddress& addr) const override;

  void SendBindingErrorResponse(StunMessage* message,
                                const rtc::SocketAddress& addr,
                                int error_code,
                                absl::string_view reason) override;

  virtual void OnReadPacket(rtc::AsyncPacketSocket* socket,
                            const rtc::ReceivedPacket& packet);

  void OnSentPacket(rtc::AsyncPacketSocket* socket,
                    const rtc::SentPacket& sent_packet) override;
  virtual void OnReadyToSend(rtc::AsyncPacketSocket* socket);
  bool SupportsProtocol(absl::string_view protocol) const override;

  void OnSocketConnect(rtc::AsyncPacketSocket* socket);
  void OnSocketClose(rtc::AsyncPacketSocket* socket, int error);

  const std::string& hash() const { return hash_; }
  const std::string& nonce() const { return nonce_; }

  int error() const { return error_; }

  void OnAllocateMismatch();

  rtc::AsyncPacketSocket* socket() const { return socket_; }
  StunRequestManager& request_manager() { return request_manager_; }

  bool HasRequests() { return !request_manager_.empty(); }
  void set_credentials(const RelayCredentials& credentials) {
    credentials_ = credentials;
  }

  bool SetEntryChannelId(const rtc::SocketAddress& address, int channel_id);

  void HandleConnectionDestroyed(Connection* conn) override;

  void CloseForTest() { Close(); }

  class CallbacksForTest {
   public:
    virtual ~CallbacksForTest() {}
    virtual void OnTurnCreatePermissionResult(int code) = 0;
    virtual void OnTurnRefreshResult(int code) = 0;
    virtual void OnTurnPortClosed() = 0;
  };
  void SetCallbacksForTest(CallbacksForTest* callbacks);

 protected:
  TurnPort(webrtc::TaskQueueBase* thread,
           rtc::PacketSocketFactory* factory,
           const rtc::Network* network,
           rtc::AsyncPacketSocket* socket,
           absl::string_view username,
           absl::string_view password,
           const ProtocolAddress& server_address,
           const RelayCredentials& credentials,
           int server_priority,
           const std::vector<std::string>& tls_alpn_protocols,
           const std::vector<std::string>& tls_elliptic_curves,
           webrtc::TurnCustomizer* customizer,
           rtc::SSLCertificateVerifier* tls_cert_verifier = nullptr,
           const webrtc::FieldTrialsView* field_trials = nullptr);

  TurnPort(webrtc::TaskQueueBase* thread,
           rtc::PacketSocketFactory* factory,
           const rtc::Network* network,
           uint16_t min_port,
           uint16_t max_port,
           absl::string_view username,
           absl::string_view password,
           const ProtocolAddress& server_address,
           const RelayCredentials& credentials,
           int server_priority,
           const std::vector<std::string>& tls_alpn_protocols,
           const std::vector<std::string>& tls_elliptic_curves,
           webrtc::TurnCustomizer* customizer,
           rtc::SSLCertificateVerifier* tls_cert_verifier = nullptr,
           const webrtc::FieldTrialsView* field_trials = nullptr);

  bool CreateOrRefreshEntry(Connection* conn, int channel_number);

  rtc::DiffServCodePoint StunDscpValue() const override;

  void Close();

 private:
  typedef std::map<rtc::Socket::Option, int> SocketOptionsMap;
  typedef std::set<rtc::SocketAddress> AttemptedServerSet;

  static bool AllowedTurnPort(int port,
                              const webrtc::FieldTrialsView* field_trials);
  void TryAlternateServer();

  bool CreateTurnClientSocket();

  void set_nonce(absl::string_view nonce) { nonce_ = std::string(nonce); }
  void set_realm(absl::string_view realm) {
    if (realm != realm_) {
      realm_ = std::string(realm);
      UpdateHash();
    }
  }

  void OnRefreshError();
  void HandleRefreshError();
  bool SetAlternateServer(const rtc::SocketAddress& address);
  void ResolveTurnAddress(const rtc::SocketAddress& address);
  void OnResolveResult(const webrtc::AsyncDnsResolverResult& result);

  void AddRequestAuthInfo(StunMessage* msg);
  void OnSendStunPacket(const void* data, size_t size, StunRequest* request);

  void OnStunAddress(const rtc::SocketAddress& address);
  void OnAllocateSuccess(const rtc::SocketAddress& address,
                         const rtc::SocketAddress& stun_address);
  void OnAllocateError(int error_code, absl::string_view reason);
  void OnAllocateRequestTimeout();

  void HandleDataIndication(const char* data,
                            size_t size,
                            int64_t packet_time_us);
  void HandleChannelData(int channel_id,
                         const char* data,
                         size_t size,
                         int64_t packet_time_us);
  void DispatchPacket(const char* data,
                      size_t size,
                      const rtc::SocketAddress& remote_addr,
                      ProtocolType proto,
                      int64_t packet_time_us);

  bool ScheduleRefresh(uint32_t lifetime);
  void SendRequest(StunRequest* request, int delay);
  int Send(const void* data, size_t size, const rtc::PacketOptions& options);
  void UpdateHash();
  bool UpdateNonce(StunMessage* response);
  void ResetNonce();

  bool HasPermission(const rtc::IPAddress& ipaddr) const;
  TurnEntry* FindEntry(const rtc::SocketAddress& address) const;
  TurnEntry* FindEntry(int channel_id) const;

  bool FailAndPruneConnection(const rtc::SocketAddress& address);

  void MaybeAddTurnLoggingId(StunMessage* message);

  void TurnCustomizerMaybeModifyOutgoingStunMessage(StunMessage* message);
  bool TurnCustomizerAllowChannelData(const void* data,
                                      size_t size,
                                      bool payload);

  ProtocolAddress server_address_;

  std::string ReconstructServerUrl();
  std::string server_url_;

  TlsCertPolicy tls_cert_policy_ = TlsCertPolicy::TLS_CERT_POLICY_SECURE;
  std::vector<std::string> tls_alpn_protocols_;
  std::vector<std::string> tls_elliptic_curves_;
  rtc::SSLCertificateVerifier* tls_cert_verifier_;
  RelayCredentials credentials_;
  AttemptedServerSet attempted_server_addresses_;

  rtc::AsyncPacketSocket* socket_;
  SocketOptionsMap socket_options_;
  std::unique_ptr<webrtc::AsyncDnsResolverInterface> resolver_;
  int error_;
  rtc::DiffServCodePoint stun_dscp_value_;

  StunRequestManager request_manager_;
  std::string realm_;
  std::string nonce_;
  std::string hash_;

  int next_channel_number_;
  std::vector<std::unique_ptr<TurnEntry>> entries_;

  PortState state_;

  int server_priority_;

  size_t allocate_mismatch_retries_;

  webrtc::TurnCustomizer* turn_customizer_ = nullptr;

  std::string turn_logging_id_;

  webrtc::ScopedTaskSafety task_safety_;

  CallbacksForTest* callbacks_for_test_ = nullptr;

  friend class TurnEntry;
  friend class TurnAllocateRequest;
  friend class TurnRefreshRequest;
  friend class TurnCreatePermissionRequest;
  friend class TurnChannelBindRequest;
};

}

#endif
