#ifndef TGCALLS_V2WASM_REFERENCE_CALL_CORE_H
#define TGCALLS_V2WASM_REFERENCE_CALL_CORE_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "third-party/json11.hpp"
#include "v2wasm/SignalingFraming.h"

namespace tgcalls {
namespace v2wasm {

class ReferenceCallCore {
public:
    ReferenceCallCore(json11::Json const &config, std::function<void(json11::Json::object &&)> emit);
    virtual ~ReferenceCallCore() = default;

    void onEvent(json11::Json const &event);

protected:

    virtual std::string mungeLocalDescription(std::string const &type, std::string const &sdp);
    virtual void onStats(json11::Json const &event);
    virtual void onIceState(std::string const &state);
    void maybeRestartIce();
    void updateIsConnected(bool isConnected);

    virtual void mungeOutgoingSignalingMessage(json11::Json::object &message);

    virtual void onDataChannelEvent(json11::Json const &event);

    struct NetworkStateRecord {
        int64_t timestampMs = 0;
        bool isConnected = false;
        bool isFailed = false;
        json11::Json connection;
    };
    struct BitrateRecord {
        int64_t timestampMs = 0;
        int32_t bitrateKbps = 0;
    };

    void emit(json11::Json::object &&command);
    void emitLog(std::string const &message);
    void requestSetLocalDescription();
    void handleSignalingData(std::string const &data);
    void handleRemoteSdp(std::string const &type, std::string const &sdp);
    void handleMediaStateMessage(json11::Json const &message);
    void flushPendingRemoteCandidates();
    void sendMediaState();
    void updateNetworkState(bool isConnected, bool isFailed);
    void emitMappedState();
    void handleStop();
    void sendSignalingMessage(json11::Json::object &&message);

    std::function<void(json11::Json::object &&)> _emit;
    std::unique_ptr<SignalingFraming> _framing;

    bool _isOutgoing = false;
    bool _enableP2P = false;
    std::vector<json11::Json> _rtcServers;

    int64_t _nowMs = 0;

    bool _didBeginNegotiation = false;
    bool _isMakingOffer = false;
    bool _isSettingRemoteAnswerPending = false;
    bool _haveLocalDescription = false;
    bool _haveRemoteDescription = false;
    std::string _signalingState = "stable";
    std::vector<json11::Json> _pendingRemoteCandidates;

    bool _isMicrophoneMuted = false;
    bool _isBatteryLow = false;
    bool _hasVideoCapture = false;
    bool _hasVideoTrack = false;
    bool _isDataChannelOpen = false;

    bool _isConnected = false;
    bool _isFailed = false;
    bool _didEmitBaselineRecord = false;
    int64_t _lastDisconnectedTimestampMs = 0;
    int64_t _lastIceRestartTimestampMs = 0;
    int _connectionTimerGeneration = 0;
    int _disconnectReportGeneration = 0;
    json11::Json _currentConnection;
    std::vector<NetworkStateRecord> _networkStateRecords;
    std::vector<BitrateRecord> _bitrateRecords;
};

}
}

#endif
