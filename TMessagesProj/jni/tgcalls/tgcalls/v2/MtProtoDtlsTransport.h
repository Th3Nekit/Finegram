#ifndef TGCALLS_MTPROTO_DTLS_TRANSPORT_H
#define TGCALLS_MTPROTO_DTLS_TRANSPORT_H

#include <memory>
#include <string>

#include "Instance.h"
#include "api/dtls_transport_interface.h"
#include "api/sequence_checker.h"
#include "p2p/base/dtls_transport_factory.h"
#include "p2p/base/dtls_transport_internal.h"
#include "p2p/base/ice_transport_internal.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/rtc_certificate.h"

namespace tgcalls {

class EncryptedConnection;

class MtProtoDtlsTransport : public cricket::DtlsTransportInternal {
public:
    MtProtoDtlsTransport(cricket::IceTransportInternal *ice, EncryptionKey encryptionKey);
    ~MtProtoDtlsTransport() override;

    const std::string &transport_name() const override;
    bool writable() const override;
    bool receiving() const override;
    int SendPacket(const char *data, size_t len, const rtc::PacketOptions &options, int flags) override;
    int SetOption(rtc::Socket::Option opt, int value) override;
    bool GetOption(rtc::Socket::Option opt, int *value) override;
    int GetError() override;
    absl::optional<rtc::NetworkRoute> network_route() const override;

    webrtc::DtlsTransportState dtls_state() const override;
    int component() const override;
    bool IsDtlsActive() const override;
    bool GetDtlsRole(rtc::SSLRole *role) const override;
    bool SetDtlsRole(rtc::SSLRole role) override;
    bool GetSslVersionBytes(int *version) const override;
    bool GetSrtpCryptoSuite(int *cipher) override;
    bool GetSslCipherSuite(int *cipher) override;
    uint16_t GetSslPeerSignatureAlgorithm() const override;
    rtc::scoped_refptr<rtc::RTCCertificate> GetLocalCertificate() const override;
    bool SetLocalCertificate(const rtc::scoped_refptr<rtc::RTCCertificate> &certificate) override;
    std::unique_ptr<rtc::SSLCertChain> GetRemoteSSLCertChain() const override;
    bool ExportKeyingMaterial(absl::string_view label, const uint8_t *context, size_t context_len, bool use_context, uint8_t *result, size_t result_len) override;
    bool SetRemoteFingerprint(absl::string_view digest_alg, const uint8_t *digest, size_t digest_len) override;
    webrtc::RTCError SetRemoteParameters(absl::string_view digest_alg, const uint8_t *digest, size_t digest_len, absl::optional<rtc::SSLRole> role) override;
    cricket::IceTransportInternal *ice_transport() override;

private:
    void onIceWritableState(rtc::PacketTransportInternal *transport);
    void onIceReceivingState(rtc::PacketTransportInternal *transport);
    void onIceReadyToSend(rtc::PacketTransportInternal *transport);
    void onIceReadPacket(rtc::PacketTransportInternal *transport, const char *data, size_t size, const int64_t &timestamp, int flags);
    void onIceSentPacket(rtc::PacketTransportInternal *transport, const rtc::SentPacket &packet);
    void onIceNetworkRouteChanged(absl::optional<rtc::NetworkRoute> route);
    void onIceClosed(rtc::PacketTransportInternal *transport);

    void emitDecryptedMessage(rtc::CopyOnWriteBuffer const &message, int64_t timestamp);
    void setDtlsState(webrtc::DtlsTransportState state);

    webrtc::SequenceChecker _sequenceChecker;

    cricket::IceTransportInternal *_ice = nullptr;
    std::unique_ptr<EncryptedConnection> _transportEncryption;
    webrtc::DtlsTransportState _dtlsState = webrtc::DtlsTransportState::kNew;
    absl::optional<rtc::SSLRole> _dtlsRole;
    rtc::scoped_refptr<rtc::RTCCertificate> _localCertificate;
    int _lastError = 0;
};

class MtProtoDtlsTransportFactory : public cricket::DtlsTransportFactory {
public:
    explicit MtProtoDtlsTransportFactory(EncryptionKey encryptionKey);

    std::unique_ptr<cricket::DtlsTransportInternal> CreateDtlsTransport(
        cricket::IceTransportInternal *ice,
        const webrtc::CryptoOptions &crypto_options,
        rtc::SSLProtocolVersion max_version) override;

private:
    EncryptionKey _encryptionKey;
};

}

#endif
